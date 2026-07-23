static int execute_select_into_user_variables_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *into_list = select_statement_into_list(statement);
    mylite_result *select_result = NULL;
    mylite_result *empty_result = NULL;
    size_t assigned_row_count = 0U;
    uint64_t found_rows = 0U;
    int rc = MYLITE_OK;

    rc = execute_select_statement(database, context, statement, true, &select_result);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = apply_select_into_user_variables(
        database,
        into_list,
        select_result,
        &assigned_row_count,
        &found_rows
    );
    if (rc != MYLITE_OK) {
        mylite_result_free(select_result);
        return rc;
    }

    rc = mylite_result_create(&empty_result);
    if (rc != MYLITE_OK) {
        mylite_result_free(select_result);
        set_nomem_error(database);
        return rc;
    }
    mylite_result_set_affected_rows(empty_result, (int64_t)assigned_row_count);
    mylite_result_set_found_row_count(empty_result, found_rows);

    mylite_result_free(select_result);
    return finish_successful_result(database, empty_result, out_result);
}

static const struct mylite_sql_ast_node *select_statement_into_list(
    const struct mylite_sql_ast_node *statement
) {
    return child_with_kind(statement, MYLITE_SQL_AST_SELECT_INTO_LIST);
}

static const struct mylite_sql_ast_node *select_statement_source_clause(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *source = child_at(statement, 1U);

    if (source == NULL) {
        return NULL;
    }
    if (source->kind == MYLITE_SQL_AST_FROM_DUAL || source->kind == MYLITE_SQL_AST_FROM_TABLE ||
        source->kind == MYLITE_SQL_AST_FROM_JOIN || source->kind == MYLITE_SQL_AST_FROM_DERIVED ||
        source->kind == MYLITE_SQL_AST_FROM_JSON_TABLE) {
        return source;
    }
    return NULL;
}

static const struct mylite_sql_ast_node *select_statement_optional_clause_start(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *source = select_statement_source_clause(statement);

    if (source != NULL) {
        return source->next_sibling;
    }
    return child_at(statement, 1U);
}

static int apply_select_into_user_variables(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *into_list,
    const mylite_result *select_result,
    size_t *out_assigned_row_count,
    uint64_t *out_found_rows
) {
    size_t variable_count = mylite_sql_ast_node_child_count(into_list);
    size_t column_count = mylite_result_column_count(select_result);
    size_t row_count = mylite_result_row_count(select_result);
    const struct mylite_sql_ast_node *variable = NULL;
    int rc = MYLITE_OK;

    if (out_assigned_row_count == NULL || out_found_rows == NULL) {
        return MYLITE_MISUSE;
    }
    *out_assigned_row_count = 0U;
    *out_found_rows = mylite_result_has_found_row_count(select_result)
                          ? mylite_result_found_row_count(select_result)
                          : (uint64_t)row_count;

    if (variable_count != column_count) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_select_reduced,
            "21000",
            "The used SELECT statements have a different number of columns"
        );
        return MYLITE_ERROR;
    }
    if (row_count == 0U) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_error_no_data,
            "02000",
            "No data - zero rows fetched, selected, or processed"
        );
    }
    if (row_count > 1U) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_result_consisted_more_than_one_row,
            "42000",
            "Result consisted of more than one row"
        );
        return MYLITE_ERROR;
    }

    variable = into_list == NULL ? NULL : into_list->first_child;
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const char *value = mylite_result_value_text(select_result, 0U, column_index);
        struct session_scalar_cell cell = {
            .value = value,
            .value_size = mylite_result_value_size(select_result, 0U, column_index),
            .has_value_size = true,
        };

        rc = mylite_execution_set_session_user_variable(
            database,
            variable,
            &cell,
            select_into_user_variable_value_kind(select_result, column_index)
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        variable = variable == NULL ? NULL : variable->next_sibling;
    }
    *out_assigned_row_count = 1U;
    return MYLITE_OK;
}

static enum mylite_session_user_variable_value_kind select_into_user_variable_value_kind(
    const mylite_result *select_result,
    size_t column_index
) {
    const char *value = mylite_result_value_text(select_result, 0U, column_index);
    enum mylite_result_column_type column_type =
        mylite_result_column_type(select_result, column_index);
    size_t value_size = 0U;

    if (value == NULL) {
        return MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    }

    switch (column_type) {
    case MYLITE_RESULT_COLUMN_TYPE_TINY:
    case MYLITE_RESULT_COLUMN_TYPE_SHORT:
    case MYLITE_RESULT_COLUMN_TYPE_LONG:
    case MYLITE_RESULT_COLUMN_TYPE_LONGLONG:
    case MYLITE_RESULT_COLUMN_TYPE_INT24:
    case MYLITE_RESULT_COLUMN_TYPE_YEAR:
    case MYLITE_RESULT_COLUMN_TYPE_BIT:
        return MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER;
    case MYLITE_RESULT_COLUMN_TYPE_DECIMAL:
    case MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL:
        return MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL;
    case MYLITE_RESULT_COLUMN_TYPE_FLOAT:
    case MYLITE_RESULT_COLUMN_TYPE_DOUBLE:
    case MYLITE_RESULT_COLUMN_TYPE_UNKNOWN:
    case MYLITE_RESULT_COLUMN_TYPE_NULL:
    case MYLITE_RESULT_COLUMN_TYPE_TIMESTAMP:
    case MYLITE_RESULT_COLUMN_TYPE_DATE:
    case MYLITE_RESULT_COLUMN_TYPE_TIME:
    case MYLITE_RESULT_COLUMN_TYPE_DATETIME:
    case MYLITE_RESULT_COLUMN_TYPE_VARCHAR:
    case MYLITE_RESULT_COLUMN_TYPE_JSON:
    case MYLITE_RESULT_COLUMN_TYPE_BLOB:
    case MYLITE_RESULT_COLUMN_TYPE_VAR_STRING:
    case MYLITE_RESULT_COLUMN_TYPE_STRING:
    case MYLITE_RESULT_COLUMN_TYPE_GEOMETRY:
        break;
    }

    value_size = mylite_result_value_size(select_result, 0U, column_index);
    if (mylite_execution_set_text_is_decimal_integer_literal(value, value_size)) {
        return MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER;
    }
    return MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
}
