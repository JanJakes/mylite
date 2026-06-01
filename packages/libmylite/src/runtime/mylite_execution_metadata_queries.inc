static int execute_do_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = child_at(expression_list, 0U);
    const char *argument_count_error_function = NULL;
    struct session_scalar_cell *cells = NULL;
    mylite_result *result = NULL;
    size_t expression_count = mylite_sql_ast_node_child_count(expression_list);
    size_t expression_index = 0U;
    uint64_t previous_last_insert_id = database->session.last_insert_id;
    int rc = MYLITE_OK;

    argument_count_error_function = do_statement_argument_count_error_function(statement);
    if (argument_count_error_function != NULL) {
        set_native_function_parameter_count_error(database, argument_count_error_function);
        return MYLITE_ERROR;
    }
    if (!do_statement_has_only_scalar_projection_expressions(statement)) {
        set_unsupported_error(
            database,
            "DO supports only session scalar values, integer/boolean/NULL values, scalar "
            "IF()/IFNULL()/COALESCE()/NULLIF()/ISNULL(), signed 64-bit +, binary -, and * "
            "arithmetic, %, MOD, DIV, top-level / division, limited numeric bitwise operators, "
            "signed 64-bit scalar comparison, keyword scalar logical operators, scalar IS "
            "predicates, limited numeric "
            "ABS()/SIGN()/BIT_COUNT()/BIN()/OCT()/CONV()/PI()/RAND()/SQRT()/DEGREES()/"
            "RADIANS()/ACOS()/ASIN()/SIN()/COS()/TAN()/COT()/ATAN()/ATAN2()/EXP()/LN()/"
            "LOG()/LOG10()/LOG2()/POW()/POWER(), CEIL()/CEILING()/FLOOR()/ROUND(), "
            "CRC32()/HEX()/UNHEX()/FORMAT()/TRUNCATE(), limited CAST(value AS BINARY/CHAR/"
            "SIGNED/UNSIGNED), limited CONVERT(value, BINARY/CHAR/SIGNED/UNSIGNED), limited "
            "DATE_ADD(... INTERVAL ... unit), limited DATE_FORMAT(), limited temporal "
            "extract, limited ELT(), FIELD(), GREATEST(), LEAST(), limited CONCAT_WS(), limited "
            "JSON_VALID(), JSON_VALUE(), JSON_ARRAY(), JSON_OBJECT(), and limited string length, "
            "string case, string slice, CHARSET(), COLLATION(), and COERCIBILITY() functions, "
            "and top-level CASE expressions"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (expression_count > SIZE_MAX / sizeof(*cells)) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(expression_count, sizeof(*cells));
    if (cells == NULL) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = append_session_scalar_do_warnings(database, statement);
    while (rc == MYLITE_OK && expression != NULL) {
        rc = session_scalar_value(database, expression, &cells[expression_index]);
        if (rc == MYLITE_OK) {
            ++expression_index;
            expression = expression->next_sibling;
        }
    }
    if (rc == MYLITE_OK || expression_index != 0U) {
        int warning_rc = append_session_scalar_cells_warnings(database, cells, expression_index);

        if (warning_rc != MYLITE_OK) {
            rc = warning_rc;
        }
    }
    session_scalar_cell_array_deinit(cells, expression_index);
    if (rc != MYLITE_OK) {
        database->session.last_insert_id = previous_last_insert_id;
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const char *argument_count_error_function = NULL;
    bool is_information_schema_query = false;
    bool is_mysql_data_dictionary_table_query = false;
    bool is_absent_mysql_enterprise_table_query = false;
    bool is_mysql_system_table_query = false;
    bool projected_statement_handled = false;
    char mysql_data_dictionary_table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char absent_mysql_enterprise_table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    memset(mysql_data_dictionary_table_name, 0, sizeof(mysql_data_dictionary_table_name));
    memset(absent_mysql_enterprise_table_name, 0, sizeof(absent_mysql_enterprise_table_name));
    argument_count_error_function = select_statement_argument_count_error_function(statement);
    if (argument_count_error_function != NULL) {
        set_native_function_parameter_count_error(database, argument_count_error_function);
        return MYLITE_ERROR;
    }
    rc = reject_select_modifier_usage_if_needed(database, statement);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = select_statement_targets_information_schema(
        database,
        statement,
        &is_information_schema_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_information_schema_query) {
        return execute_information_schema_select_statement(
            database,
            context,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    rc = select_statement_targets_mysql_data_dictionary_table(
        database,
        statement,
        mysql_data_dictionary_table_name,
        sizeof(mysql_data_dictionary_table_name),
        &is_mysql_data_dictionary_table_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_mysql_data_dictionary_table_query) {
        set_mysql_data_dictionary_table_access_error(database, mysql_data_dictionary_table_name);
        return MYLITE_ERROR;
    }
    rc = select_statement_targets_absent_mysql_enterprise_table(
        database,
        statement,
        absent_mysql_enterprise_table_name,
        sizeof(absent_mysql_enterprise_table_name),
        &is_absent_mysql_enterprise_table_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_absent_mysql_enterprise_table_query) {
        set_table_does_not_exist_error(database, "mysql", absent_mysql_enterprise_table_name);
        return MYLITE_ERROR;
    }
    rc = select_statement_targets_mysql_system_table(
        database,
        statement,
        &is_mysql_system_table_query
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_mysql_system_table_query) {
        return execute_mysql_system_table_select_statement(
            database,
            context,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    rc = execute_scalar_or_row_scalar_select_if_needed(
        database,
        statement,
        apply_sql_select_limit,
        out_result,
        &projected_statement_handled
    );
    if (projected_statement_handled || rc != MYLITE_OK) {
        return rc;
    }
    rc = reject_bare_native_function_identifier_lookup_if_needed(database, statement);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (select_statement_has_group_by_clause(statement)) {
        return execute_grouped_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_has_count_aggregate(statement)) {
        return execute_count_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_has_column_aggregate(statement)) {
        return execute_column_aggregate_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_is_scalar_projection_attempt(statement)) {
        set_unsupported_error(
            database,
            "SELECT scalar projection supports only session scalar values, integer/boolean/string/"
            "NULL, "
            "scalar comparison, scalar logical, scalar IS, signed 64-bit +, binary -, and * "
            "arithmetic, %, MOD, DIV, top-level / division, limited numeric bitwise, scalar "
            "functions, "
            "ABS()/SIGN()/BIT_COUNT()/BIN()/OCT()/CONV()/PI()/RAND()/SQRT()/DEGREES()/"
            "RADIANS()/ACOS()/ASIN()/SIN()/COS()/TAN()/COT()/ATAN()/ATAN2()/EXP()/LN()/"
            "LOG()/LOG10()/LOG2()/POW()/POWER()/CEIL()/CEILING()/FLOOR()/ROUND()/"
            "CRC32()/FORMAT()/TRUNCATE(), and "
            "CAST(value AS BINARY/CHAR/SIGNED/UNSIGNED), CONVERT(value, BINARY/CHAR/SIGNED/"
            "UNSIGNED), DATE_ADD(... INTERVAL ... unit), DATE_FORMAT(), and "
            "limited ELT(), FIELD(), GREATEST(), LEAST(), JSON_VALID(), JSON_VALUE(), "
            "JSON_ARRAY(), JSON_OBJECT(), "
            "string length, string case, string trim, and string slice functions"
        );
        return MYLITE_ERROR;
    }

    return execute_descriptor_select_statement(
        database,
        statement,
        apply_sql_select_limit,
        out_result
    );
}

static int execute_grouped_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_grouped_aggregate grouped_plan = {0};
    int rc = plan_grouped_aggregate(database, statement, &grouped_plan);

    if (rc == MYLITE_OK && apply_sql_select_limit) {
        apply_sql_select_limit_to_plan_limit(database, &grouped_plan.limit);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_grouped_aggregate_from_plan(database, &grouped_plan, out_result);
    }
    planned_grouped_aggregate_deinit(&grouped_plan);
    return rc;
}

static int execute_count_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_count count_plan = {false};
    int rc = plan_count(database, statement, &count_plan);

    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_count_from_plan(database, &count_plan, apply_sql_select_limit, out_result);
    }
    planned_count_deinit(&count_plan);
    return rc;
}

static int execute_column_aggregate_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_column_aggregate aggregate_plan = {0};
    int rc = plan_column_aggregate(database, statement, &aggregate_plan);

    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_column_aggregate_from_plan(
            database,
            &aggregate_plan,
            apply_sql_select_limit,
            out_result
        );
    }
    planned_column_aggregate_deinit(&aggregate_plan);
    return rc;
}

static int execute_descriptor_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_select plan = {0};
    int rc = plan_select(database, statement, true, &plan);

    if (rc == MYLITE_OK && apply_sql_select_limit) {
        apply_sql_select_limit_to_plan_limit(database, &plan.limit);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_select_from_plan(database, &plan, out_result);
    }
    planned_select_deinit(&plan);

    return rc;
}

static int execute_compound_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;
    mylite_result *result = NULL;
    struct compound_branch_results branch_results = {0};
    struct compound_row_comparison_columns comparison_columns = {0};
    size_t branch_index = 1U;
    enum mylite_sql_ast_set_operator operator_kind = MYLITE_SQL_AST_SET_OPERATOR_UNION;
    int rc = validate_compound_select_operator_chain(database, terms, &operator_kind);

    if (rc == MYLITE_OK) {
        rc = execute_compound_select_branch_results(
            database,
            context,
            statement,
            operator_kind,
            &branch_results
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_compound_result_columns(database, result, branch_results.items[0]);
    }
    if (rc == MYLITE_OK) {
        rc = init_compound_row_comparison_columns(
            database,
            statement,
            &branch_results,
            &comparison_columns
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_all_compound_rows(database, result, branch_results.items[0]);
    }

    term = child_at(terms, 0U);
    while (rc == MYLITE_OK && term != NULL) {
        rc = apply_compound_term_result(
            database,
            term,
            branch_results.items[branch_index],
            &result,
            &comparison_columns
        );
        ++branch_index;
        term = term->next_sibling;
    }

    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        deinit_compound_branch_results(&branch_results);
        deinit_compound_row_comparison_columns(&comparison_columns);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    apply_sql_select_limit_to_result(database, result);
    deinit_compound_branch_results(&branch_results);
    deinit_compound_row_comparison_columns(&comparison_columns);
    return finish_successful_result(database, result, out_result);
}

static int execute_compound_select_branch_results(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_set_operator operator_kind,
    struct compound_branch_results *out_results
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;
    const struct mylite_sql_ast_node *branch = child_at(statement, 0U);
    struct compound_branch_results results = {0};
    size_t branch_index = 0U;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    results.count = compound_select_branch_count(statement);
    if (results.count > SIZE_MAX / sizeof(*results.items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    results.items = (mylite_result **)calloc(results.count, sizeof(*results.items));
    if (results.items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = reject_compound_select_branch_shape(database, branch, operator_kind);
    if (rc == MYLITE_OK) {
        rc = execute_select_statement(
            database,
            context,
            branch,
            false,
            &results.items[branch_index]
        );
    }
    if (rc == MYLITE_OK) {
        column_count = mylite_result_column_count(results.items[branch_index]);
    }

    term = child_at(terms, 0U);
    while (rc == MYLITE_OK && term != NULL) {
        mylite_result *branch_result = NULL;

        ++branch_index;
        branch = child_at(term, 0U);
        rc = reject_compound_select_branch_shape(database, branch, operator_kind);
        if (rc == MYLITE_OK) {
            rc = execute_select_statement(database, context, branch, false, &branch_result);
        }
        if (rc == MYLITE_OK && mylite_result_column_count(branch_result) != column_count) {
            set_union_column_count_mismatch_error(database);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            results.items[branch_index] = branch_result;
            branch_result = NULL;
        }
        mylite_result_free(branch_result);
        term = term->next_sibling;
    }

    if (rc != MYLITE_OK) {
        deinit_compound_branch_results(&results);
        return rc;
    }

    *out_results = results;
    return MYLITE_OK;
}

static void deinit_compound_branch_results(struct compound_branch_results *results) {
    if (results == NULL) {
        return;
    }

    if (results->items != NULL) {
        for (size_t index = 0U; index < results->count; ++index) {
            mylite_result_free(results->items[index]);
        }
    }
    free((void *)results->items);
    *results = (struct compound_branch_results){0};
}

static size_t compound_select_branch_count(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    size_t count = 1U;

    for (const struct mylite_sql_ast_node *term = child_at(terms, 0U); term != NULL;
         term = term->next_sibling) {
        ++count;
    }
    return count;
}

static int init_compound_row_comparison_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct compound_branch_results *branch_results,
    struct compound_row_comparison_columns *out_columns
) {
    struct compound_row_comparison_columns columns = {0};
    size_t column_count = mylite_result_column_count(branch_results->items[0]);
    int rc = MYLITE_OK;

    if (column_count > SIZE_MAX / sizeof(*columns.string_collation_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (column_count != 0U) {
        columns.string_collation_columns =
            calloc(column_count, sizeof(*columns.string_collation_columns));
        columns.bytewise_collation_columns =
            calloc(column_count, sizeof(*columns.bytewise_collation_columns));
        if (columns.string_collation_columns == NULL ||
            columns.bytewise_collation_columns == NULL) {
            deinit_compound_row_comparison_columns(&columns);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    columns.column_count = column_count;

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        columns.bytewise_collation_columns[column_index] =
            compound_statement_column_has_binary_expression(statement, column_index);
    }

    for (size_t branch_index = 0U; rc == MYLITE_OK && branch_index < branch_results->count;
         ++branch_index) {
        rc = merge_compound_string_collation_columns(
            database,
            branch_results->items[branch_index],
            &columns
        );
    }
    if (rc != MYLITE_OK) {
        deinit_compound_row_comparison_columns(&columns);
        return rc;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (!columns.bytewise_collation_columns[column_index] &&
            compound_statement_column_uses_string_collation(statement, column_index)) {
            columns.string_collation_columns[column_index] = true;
        }
    }

    *out_columns = columns;
    return MYLITE_OK;
}

static void deinit_compound_row_comparison_columns(
    struct compound_row_comparison_columns *columns
) {
    if (columns == NULL) {
        return;
    }

    free(columns->string_collation_columns);
    free(columns->bytewise_collation_columns);
    *columns = (struct compound_row_comparison_columns){0};
}

static int apply_compound_term_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *term,
    const mylite_result *branch_result,
    mylite_result **in_out_result,
    struct compound_row_comparison_columns *comparison_columns
) {
    bool distinct =
        mylite_sql_ast_node_union_modifier(term) == MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT;
    enum mylite_sql_ast_set_operator operator_kind = mylite_sql_ast_node_set_operator_kind(term);
    int rc = MYLITE_OK;

    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        rc = append_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            distinct,
            comparison_columns->string_collation_columns
        );
        break;
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        rc = apply_intersect_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            distinct,
            comparison_columns->string_collation_columns
        );
        break;
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        rc = apply_except_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            distinct,
            comparison_columns->string_collation_columns
        );
        break;
    }

    return rc;
}

static void apply_sql_select_limit_to_plan_limit(
    const struct mylite_db *database,
    struct planned_select_limit *limit
) {
    uint64_t row_count = UINT64_MAX;

    if (database == NULL || limit == NULL || limit->has_limit) {
        return;
    }
    row_count = database->session.sql_select_limit;
    if (row_count == UINT64_MAX || row_count > (uint64_t)INT64_MAX) {
        return;
    }

    limit->has_limit = true;
    limit->row_count = (int64_t)row_count;
    limit->has_offset = false;
    limit->offset = 0;
}

static void apply_sql_select_limit_to_result(
    const struct mylite_db *database,
    mylite_result *result
) {
    uint64_t row_count = UINT64_MAX;

    if (database == NULL || result == NULL) {
        return;
    }
    row_count = database->session.sql_select_limit;
    if (row_count == UINT64_MAX || row_count > (uint64_t)SIZE_MAX) {
        return;
    }
    if ((uint64_t)mylite_result_row_count(result) > row_count) {
        mylite_result_truncate_rows(result, (size_t)row_count);
    }
}

static int reject_compound_select_branch_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    enum mylite_sql_ast_set_operator operator_kind
) {
    if (branch == NULL || branch->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, compound_select_non_select_branch_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_calc_found_rows(branch)) {
        set_unsupported_error(database, compound_select_calc_found_rows_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_options(branch) != 0U) {
        set_unsupported_error(database, compound_select_options_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (child_with_kind(branch, MYLITE_SQL_AST_ORDER_BY_CLAUSE) != NULL) {
        set_unsupported_error(database, compound_select_order_by_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (child_with_kind(branch, MYLITE_SQL_AST_LIMIT_CLAUSE) != NULL) {
        set_unsupported_error(database, compound_select_limit_error(operator_kind));
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_locking_clause(branch) !=
        MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        set_unsupported_error(database, compound_select_locking_clause_error(operator_kind));
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_compound_select_operator_chain(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *terms,
    enum mylite_sql_ast_set_operator *out_operator_kind
) {
    const struct mylite_sql_ast_node *term = child_at(terms, 0U);
    enum mylite_sql_ast_set_operator first_operator = MYLITE_SQL_AST_SET_OPERATOR_UNION;

    if (term == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_operator = mylite_sql_ast_node_set_operator_kind(term);
    for (const struct mylite_sql_ast_node *current = term; current != NULL;
         current = current->next_sibling) {
        if (mylite_sql_ast_node_set_operator_kind(current) != first_operator) {
            set_unsupported_error(database, "mixed set operations are not supported");
            return MYLITE_ERROR;
        }
    }

    *out_operator_kind = first_operator;
    return MYLITE_OK;
}

static int append_compound_result_columns(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
) {
    size_t column_count = mylite_result_column_count(source);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const struct mylite_result_column *column =
            mylite_result_column_metadata_at(source, column_index);
        int rc = MYLITE_OK;

        if (column == NULL) {
            rc = mylite_result_append_column(
                target,
                mylite_result_column_name(source, column_index)
            );
        } else {
            const struct mylite_result_column_descriptor descriptor = {
                .label = column->label,
                .schema_name = column->schema_name,
                .table_name = column->table_name,
                .origin_schema_name = column->origin_schema_name,
                .origin_table_name = column->origin_table_name,
                .origin_column_name = column->origin_column_name,
                .logical_type = column->logical_type,
                .flags = column->flags,
                .charset_id = column->charset_id,
                .collation_id = column->collation_id,
                .display_length = column->display_length,
                .decimals = column->decimals,
                .nullable = column->nullable,
            };

            rc = mylite_result_append_column_descriptor(target, &descriptor);
        }
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
    }

    return MYLITE_OK;
}

static int append_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
) {
    if (!distinct) {
        return append_all_compound_rows(database, *in_out_result, branch_result);
    }

    int rc = deduplicate_compound_result(database, in_out_result, string_collation_columns);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_distinct_compound_rows(
        database,
        *in_out_result,
        branch_result,
        string_collation_columns
    );
}

static int apply_intersect_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
) {
    mylite_result *old_result = NULL;
    mylite_result *intersected = NULL;
    size_t row_count = 0U;
    int rc = MYLITE_OK;

    if (!distinct) {
        return apply_intersect_all_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            string_collation_columns
        );
    }

    rc = deduplicate_compound_result(database, in_out_result, string_collation_columns);
    if (rc != MYLITE_OK) {
        return rc;
    }

    old_result = *in_out_result;
    row_count = mylite_result_row_count(old_result);
    rc = create_empty_compound_result_like(database, old_result, &intersected);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (compound_result_contains_row(
                branch_result,
                old_result,
                row_index,
                string_collation_columns
            )) {
            rc = append_compound_row(database, intersected, old_result, row_index);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(intersected);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = intersected;
    return MYLITE_OK;
}

static int apply_intersect_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
) {
    mylite_result *old_result = *in_out_result;
    mylite_result *intersected = NULL;
    bool *matched_rows = NULL;
    size_t branch_row_count = mylite_result_row_count(branch_result);
    size_t row_count = mylite_result_row_count(old_result);
    int rc = MYLITE_OK;

    if (branch_row_count == 0U) {
        rc = create_empty_compound_result_like(database, old_result, &intersected);
        if (rc != MYLITE_OK) {
            return rc;
        }
        mylite_result_free(old_result);
        *in_out_result = intersected;
        return MYLITE_OK;
    }

    matched_rows = calloc(branch_row_count, sizeof(*matched_rows));
    if (matched_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = create_empty_compound_result_like(database, old_result, &intersected);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        size_t branch_row_index = 0U;

        if (compound_result_find_unmatched_row(
                branch_result,
                old_result,
                row_index,
                matched_rows,
                &branch_row_index,
                string_collation_columns
            )) {
            matched_rows[branch_row_index] = true;
            rc = append_compound_row(database, intersected, old_result, row_index);
        }
    }
    free(matched_rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(intersected);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = intersected;
    return MYLITE_OK;
}

static int apply_except_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    bool distinct,
    const bool *string_collation_columns
) {
    mylite_result *old_result = NULL;
    mylite_result *difference = NULL;
    size_t row_count = 0U;
    int rc = MYLITE_OK;

    if (!distinct) {
        return apply_except_all_compound_branch_rows(
            database,
            in_out_result,
            branch_result,
            string_collation_columns
        );
    }

    rc = deduplicate_compound_result(database, in_out_result, string_collation_columns);
    if (rc != MYLITE_OK) {
        return rc;
    }

    old_result = *in_out_result;
    row_count = mylite_result_row_count(old_result);
    rc = create_empty_compound_result_like(database, old_result, &difference);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (!compound_result_contains_row(
                branch_result,
                old_result,
                row_index,
                string_collation_columns
            )) {
            rc = append_compound_row(database, difference, old_result, row_index);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(difference);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = difference;
    return MYLITE_OK;
}

static int apply_except_all_compound_branch_rows(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const mylite_result *branch_result,
    const bool *string_collation_columns
) {
    mylite_result *old_result = *in_out_result;
    mylite_result *difference = NULL;
    bool *matched_rows = NULL;
    size_t branch_row_count = mylite_result_row_count(branch_result);
    size_t row_count = mylite_result_row_count(old_result);
    int rc = MYLITE_OK;

    if (branch_row_count == 0U) {
        return MYLITE_OK;
    }

    matched_rows = calloc(branch_row_count, sizeof(*matched_rows));
    if (matched_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = create_empty_compound_result_like(database, old_result, &difference);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        size_t branch_row_index = 0U;

        if (compound_result_find_unmatched_row(
                branch_result,
                old_result,
                row_index,
                matched_rows,
                &branch_row_index,
                string_collation_columns
            )) {
            matched_rows[branch_row_index] = true;
        } else {
            rc = append_compound_row(database, difference, old_result, row_index);
        }
    }
    free(matched_rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(difference);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = difference;
    return MYLITE_OK;
}

static int create_empty_compound_result_like(
    struct mylite_db *database,
    const mylite_result *source,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_compound_result_columns(database, result, source);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    *out_result = result;
    return MYLITE_OK;
}

static int deduplicate_compound_result(
    struct mylite_db *database,
    mylite_result **in_out_result,
    const bool *string_collation_columns
) {
    mylite_result *old_result = *in_out_result;
    mylite_result *deduplicated = NULL;
    size_t row_count = mylite_result_row_count(old_result);
    int rc = MYLITE_OK;

    if (row_count <= 1U) {
        return MYLITE_OK;
    }

    rc = mylite_result_create(&deduplicated);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_compound_result_columns(database, deduplicated, old_result);
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (!compound_result_contains_row(
                deduplicated,
                old_result,
                row_index,
                string_collation_columns
            )) {
            rc = append_compound_row(database, deduplicated, old_result, row_index);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(deduplicated);
        return rc;
    }

    mylite_result_free(old_result);
    *in_out_result = deduplicated;
    return MYLITE_OK;
}

static int append_distinct_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    const bool *string_collation_columns
) {
    size_t row_count = mylite_result_row_count(source);
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        if (!compound_result_contains_row(target, source, row_index, string_collation_columns)) {
            rc = append_compound_row(database, target, source, row_index);
        }
    }

    return rc;
}

static int append_all_compound_rows(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source
) {
    size_t row_count = mylite_result_row_count(source);
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < row_count; ++row_index) {
        rc = append_compound_row(database, target, source, row_index);
    }

    return rc;
}

static int append_compound_row(
    struct mylite_db *database,
    mylite_result *target,
    const mylite_result *source,
    size_t row_index
) {
    size_t column_count = mylite_result_column_count(source);
    struct mylite_result_cell *cells = calloc(column_count, sizeof(*cells));
    int rc = MYLITE_OK;

    if (cells == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const void *bytes = mylite_result_value_bytes(source, row_index, column_index);

        cells[column_index] = (struct mylite_result_cell){
            .bytes = bytes,
            .size = mylite_result_value_size(source, row_index, column_index),
            .is_null = bytes == NULL,
        };
    }

    rc = mylite_result_append_bytes_row(target, cells);
    free(cells);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int merge_compound_string_collation_columns(
    struct mylite_db *database,
    const mylite_result *branch_result,
    struct compound_row_comparison_columns *columns
) {
    (void)database;
    if (columns == NULL) {
        return MYLITE_ERROR;
    }
    if (columns->column_count == 0U) {
        return MYLITE_OK;
    }
    if (branch_result == NULL || columns->string_collation_columns == NULL ||
        columns->bytewise_collation_columns == NULL ||
        mylite_result_column_count(branch_result) != columns->column_count) {
        return MYLITE_ERROR;
    }

    for (size_t column_index = 0U; column_index < columns->column_count; ++column_index) {
        if (compound_result_column_uses_bytewise_collation(branch_result, column_index)) {
            columns->bytewise_collation_columns[column_index] = true;
            columns->string_collation_columns[column_index] = false;
        } else if (
            !columns->bytewise_collation_columns[column_index] &&
            compound_result_column_uses_string_collation(branch_result, column_index)
        ) {
            columns->string_collation_columns[column_index] = true;
        }
    }

    return MYLITE_OK;
}

static bool compound_result_column_uses_string_collation(
    const mylite_result *result,
    size_t column_index
) {
    return result_column_uses_string_collation(
        mylite_result_column_metadata_at(result, column_index)
    );
}

static bool result_column_uses_string_collation(const struct mylite_result_column *column) {
    if (column == NULL) {
        return false;
    }
    if (result_column_uses_bytewise_collation(column) || column->collation_id == 0U) {
        return false;
    }

    switch (column->logical_type) {
    case MYLITE_RESULT_LOGICAL_TYPE_STRING:
    case MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING:
    case MYLITE_RESULT_LOGICAL_TYPE_BLOB:
        return true;
    default:
        break;
    }
    return false;
}

static bool compound_result_column_uses_bytewise_collation(
    const mylite_result *result,
    size_t column_index
) {
    return result_column_uses_bytewise_collation(
        mylite_result_column_metadata_at(result, column_index)
    );
}

static bool result_column_uses_bytewise_collation(const struct mylite_result_column *column) {
    if (column == NULL) {
        return false;
    }

    if ((column->flags & MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U) {
        return true;
    }
    return column->collation_id == mysql_collation_binary_id;
}

static bool compound_statement_column_uses_string_collation(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;

    if (compound_select_item_uses_string_collation(child_at(statement, 0U), column_index)) {
        return true;
    }

    term = child_at(terms, 0U);
    while (term != NULL) {
        if (compound_select_item_uses_string_collation(child_at(term, 0U), column_index)) {
            return true;
        }
        term = term->next_sibling;
    }
    return false;
}

static bool compound_select_item_uses_string_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
) {
    const struct mylite_sql_ast_node *select_list = child_at(branch, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (branch == NULL || branch->kind != MYLITE_SQL_AST_SELECT_STATEMENT || select_list == NULL ||
        select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    for (size_t index = 0U; select_item != NULL && index < column_index; ++index) {
        select_item = select_item->next_sibling;
    }
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return false;
    }

    return compound_expression_uses_string_collation(child_at(select_item, 0U));
}

static bool compound_expression_uses_string_collation(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING;
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_TIME_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
    case MYLITE_SQL_AST_LEFT_FUNCTION:
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
    case MYLITE_SQL_AST_LPAD_FUNCTION:
    case MYLITE_SQL_AST_RPAD_FUNCTION:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_SPACE_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_INSERT_STRING_FUNCTION:
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_OCT_FUNCTION:
    case MYLITE_SQL_AST_CONV_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
        return true;
    default:
        break;
    }
    return false;
}

static bool compound_statement_column_has_binary_expression(
    const struct mylite_sql_ast_node *statement,
    size_t column_index
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;

    if (compound_select_item_uses_binary_collation(child_at(statement, 0U), column_index)) {
        return true;
    }

    term = child_at(terms, 0U);
    while (term != NULL) {
        if (compound_select_item_uses_binary_collation(child_at(term, 0U), column_index)) {
            return true;
        }
        term = term->next_sibling;
    }
    return false;
}

static bool compound_select_item_uses_binary_collation(
    const struct mylite_sql_ast_node *branch,
    size_t column_index
) {
    const struct mylite_sql_ast_node *select_list = child_at(branch, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (branch == NULL || branch->kind != MYLITE_SQL_AST_SELECT_STATEMENT || select_list == NULL ||
        select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    for (size_t index = 0U; select_item != NULL && index < column_index; ++index) {
        select_item = select_item->next_sibling;
    }
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return false;
    }

    return compound_expression_uses_binary_collation(child_at(select_item, 0U));
}

static bool compound_expression_uses_binary_collation(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_CHAR_FUNCTION:
        return true;
    default:
        break;
    }
    return false;
}

static bool compound_result_contains_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *string_collation_columns
) {
    size_t row_count = mylite_result_row_count(target);

    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        if (compound_result_rows_equal(
                target,
                row_index,
                source,
                source_row_index,
                string_collation_columns
            )) {
            return true;
        }
    }

    return false;
}

static bool compound_result_find_unmatched_row(
    const mylite_result *target,
    const mylite_result *source,
    size_t source_row_index,
    const bool *matched_rows,
    size_t *out_row_index,
    const bool *string_collation_columns
) {
    size_t row_count = mylite_result_row_count(target);

    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        if (matched_rows != NULL && matched_rows[row_index]) {
            continue;
        }
        if (compound_result_rows_equal(
                target,
                row_index,
                source,
                source_row_index,
                string_collation_columns
            )) {
            *out_row_index = row_index;
            return true;
        }
    }

    return false;
}

static const char *compound_select_non_select_branch_error(
    enum mylite_sql_ast_set_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT supports only SELECT query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT supports only SELECT query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION supports only SELECT query blocks";
}

static const char *compound_select_calc_found_rows_error(
    enum mylite_sql_ast_set_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT does not support SQL_CALC_FOUND_ROWS";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT does not support SQL_CALC_FOUND_ROWS";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION does not support SQL_CALC_FOUND_ROWS";
}

static const char *compound_select_options_error(enum mylite_sql_ast_set_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT does not support SELECT options in query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT does not support SELECT options in query blocks";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION does not support SELECT options in query blocks";
}

static const char *compound_select_order_by_error(enum mylite_sql_ast_set_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT branch ORDER BY is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT branch ORDER BY is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION branch ORDER BY is not supported";
}

static const char *compound_select_limit_error(enum mylite_sql_ast_set_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT branch LIMIT is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT branch LIMIT is not supported";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION branch LIMIT is not supported";
}

static const char *compound_select_locking_clause_error(
    enum mylite_sql_ast_set_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_SET_OPERATOR_INTERSECT:
        return "INTERSECT does not support locking clauses";
    case MYLITE_SQL_AST_SET_OPERATOR_EXCEPT:
        return "EXCEPT does not support locking clauses";
    case MYLITE_SQL_AST_SET_OPERATOR_UNION:
        break;
    }
    return "UNION does not support locking clauses";
}

static bool compound_result_rows_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    const bool *string_collation_columns
) {
    size_t column_count = mylite_result_column_count(left);

    if (column_count != mylite_result_column_count(right)) {
        return false;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (!compound_result_cells_equal(
                left,
                left_row_index,
                right,
                right_row_index,
                column_index,
                string_collation_columns
            )) {
            return false;
        }
    }

    return true;
}

static bool compound_result_cells_equal(
    const mylite_result *left,
    size_t left_row_index,
    const mylite_result *right,
    size_t right_row_index,
    size_t column_index,
    const bool *string_collation_columns
) {
    const void *left_bytes = mylite_result_value_bytes(left, left_row_index, column_index);
    const void *right_bytes = mylite_result_value_bytes(right, right_row_index, column_index);
    size_t left_size = mylite_result_value_size(left, left_row_index, column_index);
    size_t right_size = mylite_result_value_size(right, right_row_index, column_index);

    if (left_bytes == NULL || right_bytes == NULL) {
        if (left_bytes == NULL && right_bytes == NULL) {
            return true;
        }
        return false;
    }
    if (left_size != right_size) {
        return false;
    }
    if (string_collation_columns != NULL && string_collation_columns[column_index]) {
        return compound_result_cells_equal_ascii_ci(left_bytes, left_size, right_bytes, right_size);
    }
    return memcmp(left_bytes, right_bytes, left_size) == 0;
}

static bool compound_result_cells_equal_ascii_ci(
    const void *left_bytes,
    size_t left_size,
    const void *right_bytes,
    size_t right_size
) {
    const unsigned char *left = left_bytes;
    const unsigned char *right = right_bytes;

    if (left_size != right_size) {
        return false;
    }
    for (size_t index = 0U; index < left_size; ++index) {
        if (ascii_lower(left[index]) != ascii_lower(right[index])) {
            return false;
        }
    }
    return true;
}

static int execute_scalar_or_row_scalar_select_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
) {
    *out_handled = true;
    if (select_statement_has_group_by_clause(statement)) {
        *out_handled = false;
        return MYLITE_OK;
    }
    if (select_statement_is_row_function_scalar_projection(statement)) {
        return execute_scalar_projection_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_is_row_scalar_projection_attempt(statement)) {
        return execute_row_scalar_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }
    if (select_statement_is_scalar_projection(statement)) {
        return execute_scalar_projection_select_statement(
            database,
            statement,
            apply_sql_select_limit,
            out_result
        );
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int reject_select_modifier_usage_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    bool has_descriptor_table_source = false;

    if (from_clause != NULL && (from_clause->kind == MYLITE_SQL_AST_FROM_TABLE ||
                                from_clause->kind == MYLITE_SQL_AST_FROM_JOIN)) {
        has_descriptor_table_source = true;
    }

    if (mylite_sql_ast_node_select_calc_found_rows(statement) && !has_descriptor_table_source) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS supports only descriptor-backed column-list and wildcard SELECT"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT &&
        !has_descriptor_table_source) {
        set_unsupported_error(
            database,
            "SELECT DISTINCT supports only descriptor-backed table reads"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_calc_found_rows(statement) &&
        (select_statement_has_group_by_clause(statement) ||
         select_statement_has_count_aggregate(statement) ||
         select_statement_has_column_aggregate(statement))) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS supports only descriptor-backed column-list and wildcard SELECT"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_information_schema_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_query query = {0};
    int rc = resolve_information_schema_query(database, statement, &query);

    if (rc == MYLITE_OK) {
        rc = execute_information_schema_query(
            database,
            context,
            statement,
            &query,
            apply_sql_select_limit,
            out_result
        );
    }
    information_schema_query_deinit(&query);
    return rc;
}

static int select_statement_targets_information_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U && schema_name_is_information_schema(parts[0])) {
        *out_matches = true;
        return MYLITE_OK;
    }
    if (part_count == 1U && selected_schema_is_information_schema(database)) {
        *out_matches = true;
    }

    return MYLITE_OK;
}

static int execute_mysql_system_table_select_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_query query = {0};
    const struct mylite_execution_catalog_mysql_system_table *definition = NULL;
    int rc = resolve_mysql_system_table_query(database, statement, &query, &definition);

    if (rc == MYLITE_OK) {
        rc = execute_mysql_system_table_query(
            database,
            context,
            statement,
            &query,
            definition,
            apply_sql_select_limit,
            out_result
        );
    }
    information_schema_query_deinit(&query);
    return rc;
}

static int select_statement_targets_mysql_data_dictionary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *schema_name = NULL;
    const char *table_name = NULL;
    size_t part_count = 0U;
    int written = 0;
    int rc = MYLITE_OK;

    if (out_table_name == NULL || table_name_size == 0U || out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    out_table_name[0] = '\0';
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        schema_name = parts[0];
        table_name = parts[1];
    } else if (part_count == 1U && selected_schema_is_mysql_system_schema(database)) {
        schema_name = database->session.selected_schema;
        table_name = parts[0];
    } else {
        return MYLITE_OK;
    }
    if (!schema_name_is_mysql_system_schema(schema_name) ||
        !mysql_data_dictionary_table_is_hidden(table_name)) {
        return MYLITE_OK;
    }

    *out_matches = true;
    written = snprintf(out_table_name, table_name_size, "%s", table_name);
    if (written < 0 || (size_t)written >= table_name_size) {
        set_runtime_error(database, "failed to format mysql data dictionary table name");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int select_statement_targets_absent_mysql_enterprise_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    char *out_table_name,
    size_t table_name_size,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *schema_name = NULL;
    const char *table_name = NULL;
    size_t part_count = 0U;
    int written = 0;
    int rc = MYLITE_OK;

    if (out_table_name == NULL || table_name_size == 0U || out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    out_table_name[0] = '\0';
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        schema_name = parts[0];
        table_name = parts[1];
    } else if (part_count == 1U && selected_schema_is_mysql_system_schema(database)) {
        schema_name = database->session.selected_schema;
        table_name = parts[0];
    } else {
        return MYLITE_OK;
    }
    if (!schema_name_is_mysql_system_schema(schema_name) ||
        !mysql_enterprise_table_is_target_absent(table_name)) {
        return MYLITE_OK;
    }

    *out_matches = true;
    written = snprintf(out_table_name, table_name_size, "%s", table_name);
    if (written < 0 || (size_t)written >= table_name_size) {
        set_runtime_error(database, "failed to format absent mysql Enterprise table name");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int select_statement_targets_mysql_system_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_matches
) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U && find_mysql_system_table_definition(parts[0], parts[1]) != NULL) {
        *out_matches = true;
        return MYLITE_OK;
    }
    if (part_count == 1U && database != NULL && database->session.has_selected_schema &&
        find_mysql_system_table_definition(database->session.selected_schema, parts[0]) != NULL) {
        *out_matches = true;
    }

    return MYLITE_OK;
}

static int resolve_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int rc = MYLITE_OK;

    if (out_definition == NULL) {
        return MYLITE_MISUSE;
    }
    *out_query = (struct information_schema_query){0};
    *out_definition = NULL;
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "mysql system table SELECT supports only non-distinct metadata queries"
        );
        return MYLITE_ERROR;
    }

    rc = mysql_system_table_resolve_source(database, from_clause, out_query, out_definition);
    while (rc == MYLITE_OK && optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "mysql system table SELECT supports only WHERE, ORDER BY, and LIMIT"
            );
            rc = MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_projection(database, select_list, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_where(database, where_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        out_query->limit = (struct planned_select_limit){
            .has_limit = false,
            .row_count = 0,
            .has_offset = false,
            .offset = 0,
        };
        rc = information_schema_plan_order(database, order_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_limit(database, limit_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }

    information_schema_query_deinit(out_query);
    *out_definition = NULL;
    return rc;
}

static int mysql_system_table_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query,
    const struct mylite_execution_catalog_mysql_system_table **out_definition
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_mysql_system_table *definition = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "mysql system table SELECT requires a supported mysql table"
        );
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U) {
        definition = find_mysql_system_table_definition(parts[0], parts[1]);
    } else if (part_count == 1U && database != NULL && database->session.has_selected_schema) {
        definition =
            find_mysql_system_table_definition(database->session.selected_schema, parts[0]);
    }
    if (definition == NULL) {
        set_unsupported_error(
            database,
            "mysql system table SELECT requires a supported mysql table"
        );
        return MYLITE_ERROR;
    }
    if (from_table_index_hint_list_node(from_clause) != NULL) {
        set_unsupported_error(database, "mysql system table SELECT does not support index hints");
        return MYLITE_ERROR;
    }
    out_query->definition = &definition->query_definition;
    *out_definition = definition;
    if (from_table_alias_node(from_clause) != NULL) {
        rc = copy_identifier_text(
            from_table_alias_node(from_clause),
            out_query->alias,
            sizeof(out_query->alias),
            database
        );
        if (rc == MYLITE_OK) {
            out_query->has_alias = true;
        }
    }
    return rc;
}

static const struct mylite_execution_catalog_mysql_system_table *find_mysql_system_table_definition(
    const char *schema_name,
    const char *table_name
) {
    return mylite_execution_catalog_mysql_system_table_definition_by_name(schema_name, table_name);
}

static bool mysql_data_dictionary_table_is_hidden(const char *table_name) {
    static const char *const hidden_table_names[] = {
        "catalogs",
        "character_sets",
        "check_constraints",
        "collations",
        "column_statistics",
        "column_type_elements",
        "columns",
        "dd_properties",
        "events",
        "foreign_keys",
        "foreign_key_column_usage",
        "index_column_usage",
        "index_partitions",
        "index_stats",
        "indexes",
        "innodb_ddl_log",
        "innodb_dynamic_metadata",
        "parameter_type_elements",
        "parameters",
        "resource_groups",
        "routines",
        "schemata",
        "st_spatial_reference_systems",
        "table_partition_values",
        "table_partitions",
        "table_stats",
        "tables",
        "tablespace_files",
        "tablespaces",
        "triggers",
        "view_routine_usage",
        "view_table_usage",
    };

    if (table_name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(hidden_table_names) / sizeof(hidden_table_names[0]);
         ++index) {
        if (strcmp(table_name, hidden_table_names[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool mysql_enterprise_table_is_target_absent(const char *table_name) {
    static const char *const absent_table_names[] = {
        "audit_log_filter",
        "audit_log_user",
        "firewall_group_allowlist",
        "firewall_groups",
        "firewall_membership",
        "firewall_users",
        "firewall_whitelist",
    };

    if (table_name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(absent_table_names) / sizeof(absent_table_names[0]);
         ++index) {
        if (strcmp(table_name, absent_table_names[index]) == 0) {
            return true;
        }
    }
    return false;
}

static int execute_mysql_system_table_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_row_set rows = {0};
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    int rc = build_mysql_system_table_rows(database, context, definition, &rows);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_result_columns(database, result, query);
    }
    if (rc == MYLITE_OK && query->is_count_star) {
        rc = information_schema_append_count_result(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    } else if (rc == MYLITE_OK) {
        rc = information_schema_append_result_rows(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    }
    information_schema_row_set_deinit(&rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    if (apply_sql_select_limit && !query->limit.has_limit) {
        apply_sql_select_limit_to_result(database, result);
    }
    return finish_successful_result(database, result, out_result);
}

static int build_mysql_system_table_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *out_rows
) {
    bool has_no_rows = mysql_system_table_definition_has_no_rows(definition);
    bool has_catalog_rows = mysql_system_table_definition_has_catalog_rows(definition);
    int rc = MYLITE_OK;

    *out_rows = (struct information_schema_row_set){.definition = &definition->query_definition};
    rc = append_mysql_system_table_system_rows(database, context, definition, out_rows);
    if (rc == MYLITE_OK && !has_no_rows && has_catalog_rows) {
        rc = append_mysql_system_table_catalog_rows(database, definition, out_rows);
    }
    if (rc != MYLITE_OK) {
        information_schema_row_set_deinit(out_rows);
    }
    return rc;
}

static int append_mysql_system_table_system_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    (void)context;

    if (mysql_system_table_definition_has_no_rows(definition)) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") == 0) {
        return append_mysql_schema_system_table_rows(database, definition, rows);
    }
    if (strcmp(definition->schema_name, "sys") == 0) {
        return append_sys_schema_system_table_rows(database, definition, rows);
    }

    set_runtime_error(database, "invalid mysql system table");
    return MYLITE_ERROR;
}

static int append_mysql_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    if (strcmp(definition->query_definition.name, "engine_cost") == 0) {
        return append_mysql_engine_cost_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "innodb_index_stats") == 0) {
        return append_mysql_innodb_index_stats_builtin_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "innodb_table_stats") == 0) {
        return append_mysql_innodb_table_stats_builtin_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "plugin") == 0) {
        return append_mysql_plugin_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "server_cost") == 0) {
        return append_mysql_server_cost_system_rows(database, rows);
    }

    set_runtime_error(database, "invalid mysql system table");
    return MYLITE_ERROR;
}

static int append_sys_schema_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    int status = MYLITE_OK;

    if (strcmp(definition->query_definition.name, "sys_config") == 0) {
        return append_sys_sys_config_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "version") == 0) {
        return append_sys_version_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "innodb_lock_waits") == 0) {
        return append_sys_innodb_lock_waits_system_rows(database, rows);
    }
    if (append_sys_schema_summary_system_table_rows(database, definition, rows, &status)) {
        return status;
    }
    if (strcmp(definition->query_definition.name, "io_by_thread_by_latency") == 0) {
        return append_sys_io_by_thread_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_file_by_bytes") == 0) {
        return append_sys_io_global_by_file_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_file_by_latency") == 0) {
        return append_sys_io_global_by_file_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_wait_by_bytes") == 0) {
        return append_sys_io_global_by_wait_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "io_global_by_wait_by_latency") == 0) {
        return append_sys_io_global_by_wait_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "latest_file_io") == 0) {
        return append_sys_latest_file_io_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "ps_check_lost_instrumentation") == 0) {
        return append_sys_ps_check_lost_instrumentation_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_auto_increment_columns") == 0) {
        return append_sys_schema_auto_increment_columns_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_index_statistics") == 0) {
        return append_sys_schema_index_statistics_system_rows(database, rows, true);
    }
    if (strcmp(definition->query_definition.name, "schema_object_overview") == 0) {
        return append_sys_schema_object_overview_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_redundant_indexes") == 0) {
        return append_sys_schema_redundant_indexes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_table_lock_waits") == 0) {
        return append_sys_schema_table_lock_waits_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_table_statistics") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, true, false);
    }
    if (strcmp(definition->query_definition.name, "schema_table_statistics_with_buffer") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, true, true);
    }
    if (strcmp(definition->query_definition.name, "schema_tables_with_full_table_scans") == 0) {
        return append_sys_schema_tables_with_full_table_scans_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "schema_unused_indexes") == 0) {
        return append_sys_schema_unused_indexes_system_rows(database, rows);
    }

    return append_sys_schema_x_system_table_rows(database, definition, rows);
}

static bool append_sys_schema_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
) {
    if (strcmp(definition->query_definition.name, "host_summary") == 0) {
        *out_status = append_sys_host_summary_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_file_io") == 0) {
        *out_status = append_sys_host_summary_by_file_io_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_file_io_type") == 0) {
        *out_status = append_sys_host_summary_by_file_io_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_stages") == 0) {
        *out_status = append_sys_host_summary_by_stages_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_statement_latency") == 0) {
        *out_status = append_sys_host_summary_by_statement_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "host_summary_by_statement_type") == 0) {
        *out_status = append_sys_host_summary_by_statement_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_by_host_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_host_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_by_thread_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_thread_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "memory_by_user_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_user_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "innodb_buffer_stats_by_schema") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_schema_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "innodb_buffer_stats_by_table") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_table_system_rows(database, rows);
        return true;
    }

    return false;
}

static int append_sys_schema_x_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    int status = MYLITE_OK;

    if (strcmp(definition->query_definition.name, "x$schema_flattened_keys") == 0) {
        return append_sys_x_schema_flattened_keys_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$innodb_lock_waits") == 0) {
        return append_sys_innodb_lock_waits_system_rows(database, rows);
    }
    if (append_sys_schema_x_summary_system_table_rows(database, definition, rows, &status)) {
        return status;
    }
    if (strcmp(definition->query_definition.name, "x$io_by_thread_by_latency") == 0) {
        return append_sys_io_by_thread_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_file_by_bytes") == 0) {
        return append_sys_io_global_by_file_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_file_by_latency") == 0) {
        return append_sys_io_global_by_file_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_wait_by_bytes") == 0) {
        return append_sys_io_global_by_wait_by_bytes_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$io_global_by_wait_by_latency") == 0) {
        return append_sys_io_global_by_wait_by_latency_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$latest_file_io") == 0) {
        return append_sys_latest_file_io_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$ps_schema_table_statistics_io") == 0) {
        return append_sys_x_ps_schema_table_statistics_io_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$schema_index_statistics") == 0) {
        return append_sys_schema_index_statistics_system_rows(database, rows, false);
    }
    if (strcmp(definition->query_definition.name, "x$schema_table_lock_waits") == 0) {
        return append_sys_schema_table_lock_waits_system_rows(database, rows);
    }
    if (strcmp(definition->query_definition.name, "x$schema_table_statistics") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, false, false);
    }
    if (strcmp(definition->query_definition.name, "x$schema_table_statistics_with_buffer") == 0) {
        return append_sys_schema_table_statistics_system_rows(database, rows, false, true);
    }
    if (strcmp(definition->query_definition.name, "x$schema_tables_with_full_table_scans") == 0) {
        return append_sys_schema_tables_with_full_table_scans_system_rows(database, rows);
    }

    set_runtime_error(database, "invalid mysql system table");
    return MYLITE_ERROR;
}

static bool append_sys_schema_x_summary_system_table_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows,
    int *out_status
) {
    if (strcmp(definition->query_definition.name, "x$host_summary") == 0) {
        *out_status = append_sys_host_summary_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_file_io") == 0) {
        *out_status = append_sys_host_summary_by_file_io_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_file_io_type") == 0) {
        *out_status = append_sys_host_summary_by_file_io_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_stages") == 0) {
        *out_status = append_sys_host_summary_by_stages_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_statement_latency") == 0) {
        *out_status = append_sys_host_summary_by_statement_latency_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$host_summary_by_statement_type") == 0) {
        *out_status = append_sys_host_summary_by_statement_type_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_by_host_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_host_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_by_thread_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_thread_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$memory_by_user_by_current_bytes") == 0) {
        *out_status = append_sys_memory_by_user_by_current_bytes_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$innodb_buffer_stats_by_schema") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_schema_system_rows(database, rows);
        return true;
    }
    if (strcmp(definition->query_definition.name, "x$innodb_buffer_stats_by_table") == 0) {
        *out_status = append_sys_innodb_buffer_stats_by_table_system_rows(database, rows);
        return true;
    }

    return false;
}

static bool mysql_system_table_definition_has_no_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    return definition != NULL && strcmp(definition->schema_name, "mysql") == 0 &&
           (strcmp(definition->query_definition.name, "component") == 0 ||
            strcmp(definition->query_definition.name, "columns_priv") == 0 ||
            strcmp(definition->query_definition.name, "db") == 0 ||
            strcmp(definition->query_definition.name, "default_roles") == 0 ||
            strcmp(definition->query_definition.name, "func") == 0 ||
            strcmp(definition->query_definition.name, "general_log") == 0 ||
            strcmp(definition->query_definition.name, "global_grants") == 0 ||
            strcmp(definition->query_definition.name, "gtid_executed") == 0 ||
            strcmp(definition->query_definition.name, "help_category") == 0 ||
            strcmp(definition->query_definition.name, "help_keyword") == 0 ||
            strcmp(definition->query_definition.name, "help_relation") == 0 ||
            strcmp(definition->query_definition.name, "help_topic") == 0 ||
            strcmp(definition->query_definition.name, "ndb_binlog_index") == 0 ||
            strcmp(definition->query_definition.name, "password_history") == 0 ||
            strcmp(definition->query_definition.name, "procs_priv") == 0 ||
            strcmp(definition->query_definition.name, "proxies_priv") == 0 ||
            strcmp(definition->query_definition.name, "role_edges") == 0 ||
            strcmp(definition->query_definition.name, "servers") == 0 ||
            strcmp(definition->query_definition.name, "slave_master_info") == 0 ||
            strcmp(definition->query_definition.name, "slave_relay_log_info") == 0 ||
            strcmp(definition->query_definition.name, "slave_worker_info") == 0 ||
            strcmp(definition->query_definition.name, "slow_log") == 0 ||
            strcmp(definition->query_definition.name, "tables_priv") == 0 ||
            strcmp(definition->query_definition.name, "time_zone") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_leap_second") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_name") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_transition") == 0 ||
            strcmp(definition->query_definition.name, "time_zone_transition_type") == 0 ||
            strcmp(definition->query_definition.name, "user") == 0);
}

static bool mysql_system_table_definition_has_catalog_rows(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    return definition != NULL && strcmp(definition->schema_name, "mysql") == 0 &&
           (strcmp(definition->query_definition.name, "innodb_index_stats") == 0 ||
            strcmp(definition->query_definition.name, "innodb_table_stats") == 0);
}

static int append_mysql_engine_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct mysql_engine_cost_row {
        const char *engine_name;
        const char *device_type;
        const char *cost_name;
        const char *cost_value;
        const char *comment;
        const char *default_value;
    };
    static const struct mysql_engine_cost_row cost_rows[] = {
        {"default", "0", "io_block_read_cost", NULL, NULL, "1"},
        {"default", "0", "memory_block_read_cost", NULL, NULL, "0.25"},
    };
    char last_update[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_engine_cost_column_count) {
        set_runtime_error(database, "invalid mysql.engine_cost columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(cost_rows) / sizeof(cost_rows[0]);
         ++row_index) {
        const struct mysql_engine_cost_row *cost_row = &cost_rows[row_index];
        const char *values[mysql_engine_cost_column_count] = {
            cost_row->engine_name,
            cost_row->device_type,
            cost_row->cost_name,
            cost_row->cost_value,
            last_update,
            cost_row->comment,
            cost_row->default_value,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_mysql_plugin_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    static const char *const plugin_rows[][mysql_plugin_column_count] = {
        {"CONNECTION_CONTROL", "connection_control.so"},
        {"CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS", "connection_control.so"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_plugin_column_count) {
        set_runtime_error(database, "invalid mysql.plugin columns");
        return MYLITE_ERROR;
    }

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(plugin_rows) / sizeof(plugin_rows[0]);
         ++row_index) {
        rc = append_information_schema_row(database, rows, plugin_rows[row_index]);
    }
    return rc;
}

static int append_mysql_server_cost_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct mysql_server_cost_row {
        const char *cost_name;
        const char *cost_value;
        const char *comment;
        const char *default_value;
    };
    static const struct mysql_server_cost_row cost_rows[] = {
        {"disk_temptable_create_cost", NULL, NULL, "20"},
        {"disk_temptable_row_cost", NULL, NULL, "0.5"},
        {"key_compare_cost", NULL, NULL, "0.05"},
        {"memory_temptable_create_cost", NULL, NULL, "1"},
        {"memory_temptable_row_cost", NULL, NULL, "0.1"},
        {"row_evaluate_cost", NULL, NULL, "0.1"},
    };
    char last_update[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_server_cost_column_count) {
        set_runtime_error(database, "invalid mysql.server_cost columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(cost_rows) / sizeof(cost_rows[0]);
         ++row_index) {
        const struct mysql_server_cost_row *cost_row = &cost_rows[row_index];
        const char *values[mysql_server_cost_column_count] = {
            cost_row->cost_name,
            cost_row->cost_value,
            last_update,
            cost_row->comment,
            cost_row->default_value,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_sys_sys_config_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_config_row {
        const char *variable;
        const char *value;
    };
    static const struct sys_config_row config_rows[] = {
        {"diagnostics.allow_i_s_tables", "OFF"},
        {"diagnostics.include_raw", "OFF"},
        {"ps_thread_trx_info.max_length", "65535"},
        {"statement_performance_analyzer.limit", "100"},
        {"statement_performance_analyzer.view", NULL},
        {"statement_truncate_len", "64"},
    };
    char set_time[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_sys_config_column_count) {
        set_runtime_error(database, "invalid sys.sys_config columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, set_time, sizeof(set_time));
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(config_rows) / sizeof(config_rows[0]);
         ++row_index) {
        const struct sys_config_row *config_row = &config_rows[row_index];
        const char *values[sys_sys_config_column_count] = {
            config_row->variable,
            config_row->value,
            set_time,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_sys_version_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[sys_version_column_count] = {
        "2.1.3",
        MYLITE_MYSQL_SERVER_VERSION_STRING,
    };

    if (rows->definition->column_count != sys_version_column_count) {
        set_runtime_error(database, "invalid sys.version columns");
        return MYLITE_ERROR;
    }

    return append_information_schema_row(database, rows, values);
}

static int append_sys_innodb_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_innodb_lock_waits_column_count) {
        set_runtime_error(database, "invalid sys.innodb_lock_waits columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_column_count) {
        set_runtime_error(database, "invalid sys.host_summary columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_file_io_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_file_io columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_file_io_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_file_io_type_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_file_io_type columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_stages_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_stages_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_stages columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_statement_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_statement_latency_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_statement_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_host_summary_by_statement_type_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_host_summary_by_statement_type_column_count) {
        set_runtime_error(database, "invalid sys.host_summary_by_statement_type columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_by_host_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_by_host_by_current_bytes_column_count) {
        set_runtime_error(database, "invalid sys.memory_by_host_by_current_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_by_thread_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_by_thread_by_current_bytes_column_count) {
        set_runtime_error(database, "invalid sys.memory_by_thread_by_current_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_memory_by_user_by_current_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_memory_by_user_by_current_bytes_column_count) {
        set_runtime_error(database, "invalid sys.memory_by_user_by_current_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_innodb_buffer_stats_by_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_innodb_buffer_stats_by_schema_column_count) {
        set_runtime_error(database, "invalid sys.innodb_buffer_stats_by_schema columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_innodb_buffer_stats_by_table_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_innodb_buffer_stats_by_table_column_count) {
        set_runtime_error(database, "invalid sys.innodb_buffer_stats_by_table columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_by_thread_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_by_thread_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.io_by_thread_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_file_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_file_by_bytes_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_file_by_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_file_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_file_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_file_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_wait_by_bytes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_wait_by_bytes_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_wait_by_bytes columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_io_global_by_wait_by_latency_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_io_global_by_wait_by_latency_column_count) {
        set_runtime_error(database, "invalid sys.io_global_by_wait_by_latency columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_latest_file_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_latest_file_io_column_count) {
        set_runtime_error(database, "invalid sys.latest_file_io columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_ps_check_lost_instrumentation_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_ps_check_lost_instrumentation_column_count) {
        set_runtime_error(database, "invalid sys.ps_check_lost_instrumentation columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

enum sys_schema_auto_increment_columns_column_index {
    sys_schema_auto_increment_columns_table_schema_column = 0,
    sys_schema_auto_increment_columns_table_name_column = 1,
    sys_schema_auto_increment_columns_column_name_column = 2,
    sys_schema_auto_increment_columns_max_value_column = 7,
    sys_schema_auto_increment_columns_auto_increment_ratio_column = 9,
};

static int append_sys_schema_auto_increment_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_auto_increment_columns_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_auto_increment_columns_column_count) {
        set_runtime_error(database, "invalid sys.schema_auto_increment_columns columns");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_for_each_schema(
        database,
        append_sys_schema_auto_increment_columns_schema_rows,
        &context
    );
    if (rc == MYLITE_OK) {
        sort_sys_schema_auto_increment_columns_default_order(rows);
    }
    return rc;
}

static int append_sys_schema_auto_increment_columns_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_auto_increment_columns_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (strcmp(schema->name, "mysql") == 0 || strcmp(schema->name, "sys") == 0 ||
        strcmp(schema->name, "performance_schema") == 0 ||
        schema_name_is_information_schema(schema->name)) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_auto_increment_columns_table_rows,
        context
    );
}

static int append_sys_schema_auto_increment_columns_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_auto_increment_columns_context *context = user_data;
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = load_table_columns(context->database, table->table_id, &columns, &column_count);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (!columns[column_index].is_auto_increment) {
            continue;
        }
        rc = append_sys_schema_auto_increment_columns_column_row(
            context,
            table,
            &columns[column_index]
        );
    }
    free(columns);
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(context->database)) == MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else {
            set_runtime_error(
                context->database,
                "failed to build sys.schema_auto_increment_columns rows"
            );
        }
    }
    return rc;
}

static int append_sys_schema_auto_increment_columns_column_row(
    struct sys_schema_auto_increment_columns_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    char column_type_text[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    char max_value_text[integer_text_capacity];
    char auto_increment_text[integer_text_capacity];
    char ratio_text[integer_text_capacity];
    const char *column_type = NULL;
    const char *max_value_cell = NULL;
    const char *auto_increment_cell = NULL;
    const char *auto_increment_ratio_cell = NULL;
    const char *data_type = information_schema_data_type_for_descriptor(column);
    bool is_unsigned =
        information_schema_innodb_column_logical_type_is_unsigned(column->logical_type);
    uint64_t max_value = 0U;
    int64_t row_count = 0;
    int rc = show_column_type_text(
        context->database,
        column->logical_type,
        column_type_text,
        sizeof(column_type_text),
        &column_type
    );

    if (rc == MYLITE_OK &&
        sys_schema_auto_increment_columns_max_value(data_type, is_unsigned, &max_value)) {
        rc = format_uint64(context->database, max_value, max_value_text, sizeof(max_value_text));
        if (rc == MYLITE_OK) {
            max_value_cell = max_value_text;
        }
    }
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(context->database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(
                context->database,
                "failed to read sys.schema_auto_increment_columns row count"
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && max_value_cell != NULL &&
        !(row_count == 0 && table->auto_increment_next <= 1)) {
        rc = information_schema_format_i64(
            context->database,
            table->auto_increment_next,
            auto_increment_text,
            sizeof(auto_increment_text)
        );
        if (rc == MYLITE_OK) {
            auto_increment_cell = auto_increment_text;
            rc = format_sys_schema_auto_increment_columns_ratio(
                context->database,
                table->auto_increment_next,
                max_value,
                ratio_text,
                sizeof(ratio_text)
            );
        }
        if (rc == MYLITE_OK) {
            auto_increment_ratio_cell = ratio_text;
        }
    }
    if (rc == MYLITE_OK) {
        const char *values[sys_schema_auto_increment_columns_column_count] = {
            context->schema->name,
            table->name,
            column->name,
            data_type,
            column_type,
            is_unsigned ? "0" : "1",
            is_unsigned ? "1" : "0",
            max_value_cell,
            auto_increment_cell,
            auto_increment_ratio_cell,
        };

        rc = append_information_schema_row(context->database, context->rows, values);
    }
    return rc;
}

static bool sys_schema_auto_increment_columns_max_value(
    const char *data_type,
    bool is_unsigned,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (out_value == NULL) {
        return false;
    }
    *out_value = 0U;
    if (data_type == NULL) {
        return false;
    }
    if (strcmp(data_type, "tinyint") == 0) {
        value = UINT64_C(255);
    } else if (strcmp(data_type, "smallint") == 0) {
        value = UINT64_C(65535);
    } else if (strcmp(data_type, "mediumint") == 0) {
        value = UINT64_C(16777215);
    } else if (strcmp(data_type, "int") == 0) {
        value = UINT64_C(4294967295);
    } else if (strcmp(data_type, "bigint") == 0) {
        value = UINT64_MAX;
    } else {
        return false;
    }

    *out_value = is_unsigned ? value : value >> 1U;
    return true;
}

static int format_sys_schema_auto_increment_columns_ratio(
    struct mylite_db *database,
    int64_t auto_increment,
    uint64_t max_value,
    char *buffer,
    size_t buffer_size
) {
    int written = 0;

    if (auto_increment < 0 || max_value == 0U) {
        set_runtime_error(database, "invalid sys.schema_auto_increment_columns ratio");
        return MYLITE_ERROR;
    }

    written =
        snprintf(buffer, buffer_size, "%.4f", (double)(uint64_t)auto_increment / (double)max_value);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format sys.schema_auto_increment_columns ratio");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void sort_sys_schema_auto_increment_columns_default_order(
    struct information_schema_row_set *rows
) {
    if (rows == NULL || rows->row_count < 2U) {
        return;
    }

    for (size_t index = 1U; index < rows->row_count; ++index) {
        char **row = rows->rows[index];
        size_t insert = index;

        while (insert > 0U &&
               compare_sys_schema_auto_increment_columns_rows(row, rows->rows[insert - 1U]) < 0) {
            rows->rows[insert] = rows->rows[insert - 1U];
            --insert;
        }
        rows->rows[insert] = row;
    }
}

static int compare_sys_schema_auto_increment_columns_rows(char *const *left, char *const *right) {
    const char *left_ratio = left[sys_schema_auto_increment_columns_auto_increment_ratio_column];
    const char *right_ratio = right[sys_schema_auto_increment_columns_auto_increment_ratio_column];
    int text_compare = 0;

    if (left_ratio == NULL && right_ratio != NULL) {
        return 1;
    }
    if (left_ratio != NULL && right_ratio == NULL) {
        return -1;
    }
    if (left_ratio != NULL && right_ratio != NULL) {
        double left_value = strtod(left_ratio, NULL);
        double right_value = strtod(right_ratio, NULL);

        if (left_value > right_value) {
            return -1;
        }
        if (left_value < right_value) {
            return 1;
        }
    }

    if (left[sys_schema_auto_increment_columns_max_value_column] != NULL &&
        right[sys_schema_auto_increment_columns_max_value_column] != NULL) {
        uint64_t left_max =
            strtoull(left[sys_schema_auto_increment_columns_max_value_column], NULL, decimal_base);
        uint64_t right_max =
            strtoull(right[sys_schema_auto_increment_columns_max_value_column], NULL, decimal_base);

        if (left_max < right_max) {
            return -1;
        }
        if (left_max > right_max) {
            return 1;
        }
    }

    text_compare = compare_optional_text(
        left[sys_schema_auto_increment_columns_table_schema_column],
        right[sys_schema_auto_increment_columns_table_schema_column]
    );
    if (text_compare != 0) {
        return text_compare;
    }
    text_compare = compare_optional_text(
        left[sys_schema_auto_increment_columns_table_name_column],
        right[sys_schema_auto_increment_columns_table_name_column]
    );
    if (text_compare != 0) {
        return text_compare;
    }
    return compare_optional_text(
        left[sys_schema_auto_increment_columns_column_name_column],
        right[sys_schema_auto_increment_columns_column_name_column]
    );
}

static int compare_optional_text(const char *left, const char *right) {
    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        return 1;
    }
    if (right == NULL) {
        return -1;
    }
    return strcmp(left, right);
}

static int append_sys_schema_index_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_latency
) {
    struct sys_schema_index_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_latency = formatted_latency,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_index_statistics_column_count) {
        set_runtime_error(database, "invalid sys.schema_index_statistics columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_schema_index_statistics_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_index_statistics_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_mysql_system_rows(
    struct sys_schema_index_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_schema_index_statistics_mysql_system_table_rows(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_mysql_system_table_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            definition->schema_name,
            definition->query_definition.name,
            "PRIMARY"
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            definition->schema_name,
            definition->query_definition.name,
            definition->secondary_indexes[index].name
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_index_statistics_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_index_statistics_table_rows,
        context
    );
}

static int append_sys_schema_index_statistics_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_index_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_index_statistics_base_index_rows(context, table);
}

static int append_sys_schema_index_statistics_base_index_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            context->schema->name,
            table->name,
            indexes[index].index.name
        );
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_schema_index_statistics_index_row(
    struct sys_schema_index_statistics_context *context,
    const char *schema_name,
    const char *table_name,
    const char *index_name
) {
    static const char formatted_zero_latency[] = "  0 ps";
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL || index_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *latency = context->formatted_latency ? formatted_zero_latency : zero;
    const char *values[sys_schema_index_statistics_column_count] = {
        schema_name,
        table_name,
        index_name,
        zero,
        latency,
        zero,
        latency,
        zero,
        latency,
        zero,
        latency,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_x_ps_schema_table_statistics_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_table_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_metrics = false,
        .include_buffer_metrics = false,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_x_ps_schema_table_statistics_io_column_count) {
        set_runtime_error(database, "invalid sys.x$ps_schema_table_statistics_io columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_x_ps_schema_table_statistics_io_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (definition == NULL) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") != 0 &&
        (strcmp(definition->schema_name, "sys") != 0 ||
         strcmp(definition->query_definition.name, "sys_config") != 0)) {
        return MYLITE_OK;
    }
    return append_sys_x_ps_schema_table_statistics_io_table_row(
        context,
        definition->schema_name,
        definition->query_definition.name
    );
}

static int append_sys_x_ps_schema_table_statistics_io_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog,
        context
    );
}

static int append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_x_ps_schema_table_statistics_io_table_row(
        context,
        context->schema->name,
        table->name
    );
}

static int append_sys_x_ps_schema_table_statistics_io_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
) {
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *values[sys_x_ps_schema_table_statistics_io_column_count] = {
        schema_name,
        table_name,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_table_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_metrics,
    bool include_buffer_metrics
) {
    struct sys_schema_table_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_metrics = formatted_metrics,
        .include_buffer_metrics = include_buffer_metrics,
        .schema = NULL,
    };
    size_t expected_column_count = include_buffer_metrics
                                       ? sys_schema_table_statistics_with_buffer_column_count
                                       : sys_schema_table_statistics_column_count;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != expected_column_count) {
        set_runtime_error(database, "invalid sys.schema_table_statistics columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_schema_table_statistics_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_table_statistics_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_schema_table_statistics_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_schema_table_statistics_mysql_system_table_row(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_schema_table_statistics_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (definition == NULL) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") != 0 &&
        (strcmp(definition->schema_name, "sys") != 0 ||
         strcmp(definition->query_definition.name, "sys_config") != 0)) {
        return MYLITE_OK;
    }
    return append_sys_schema_table_statistics_table_row(
        context,
        definition->schema_name,
        definition->query_definition.name
    );
}

static int append_sys_schema_table_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_table_statistics_table_row_from_catalog,
        context
    );
}

static int append_sys_schema_table_statistics_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_table_statistics_table_row(
        context,
        context->schema->name,
        table->name
    );
}

static int append_sys_schema_table_statistics_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
) {
    static const char formatted_zero_bytes[] = "   0 bytes";
    static const char formatted_zero_latency[] = "  0 ps";
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *latency = context->formatted_metrics ? formatted_zero_latency : zero;
    const char *bytes = context->formatted_metrics ? formatted_zero_bytes : zero;
    if (context->include_buffer_metrics) {
        const char *values[sys_schema_table_statistics_with_buffer_column_count] = {
            schema_name, table_name, zero,  latency, zero, latency, zero,    latency, zero,
            latency,     zero,       bytes, latency, zero, bytes,   latency, zero,    latency,
            bytes,       bytes,      bytes, zero,    zero, zero,    zero,
        };

        return append_information_schema_row(context->database, context->rows, values);
    }

    const char *values[sys_schema_table_statistics_column_count] = {
        schema_name, table_name, latency, zero,    latency, zero,  latency, zero, latency, zero,
        latency,     zero,       bytes,   latency, zero,    bytes, latency, zero, latency,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_redundant_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_redundant_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_schema_redundant_indexes_column_count) {
        set_runtime_error(database, "invalid sys.schema_redundant_indexes columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_schema_redundant_indexes_schema_rows,
        &context
    );
}

static int append_sys_x_schema_flattened_keys_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_redundant_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_x_schema_flattened_keys_column_count) {
        set_runtime_error(database, "invalid sys.x$schema_flattened_keys columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_x_schema_flattened_keys_schema_rows,
        &context
    );
}

static int append_sys_schema_redundant_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_redundant_indexes_table_rows,
        context
    );
}

static int append_sys_x_schema_flattened_keys_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_x_schema_flattened_keys_table_rows,
        context
    );
}

static int append_sys_schema_redundant_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;
    struct sys_flattened_key_row_set key_rows = {0};
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = build_sys_flattened_key_rows(context, table, &key_rows);
    if (rc == MYLITE_OK) {
        rc = append_sys_schema_redundant_index_rows(context, &key_rows);
    }
    sys_flattened_key_rows_deinit(&key_rows);
    return rc;
}

static int append_sys_x_schema_flattened_keys_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;
    struct sys_flattened_key_row_set key_rows = {0};
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = build_sys_flattened_key_rows(context, table, &key_rows);
    if (rc == MYLITE_OK) {
        rc = append_sys_x_schema_flattened_key_rows(context, &key_rows);
    }
    sys_flattened_key_rows_deinit(&key_rows);
    return rc;
}

static int build_sys_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    struct sys_flattened_key_row_set *out_rows
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    *out_rows = (struct sys_flattened_key_row_set){0};
    rc = load_table_columns(context->database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (sys_flattened_key_index_supported(&indexes[index])) {
            rc = append_sys_flattened_key_row_from_index(context, table, &indexes[index], out_rows);
        }
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_x_schema_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < key_rows->count; ++index) {
        rc = append_sys_x_schema_flattened_key_row(context, &key_rows->rows[index]);
    }
    return rc;
}

static int append_sys_schema_redundant_index_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
) {
    int rc = MYLITE_OK;

    for (size_t redundant_index = 0U; rc == MYLITE_OK && redundant_index < key_rows->count;
         ++redundant_index) {
        const struct sys_flattened_key_row *redundant = &key_rows->rows[redundant_index];

        for (size_t dominant_index = 0U; rc == MYLITE_OK && dominant_index < key_rows->count;
             ++dominant_index) {
            const struct sys_flattened_key_row *dominant = &key_rows->rows[dominant_index];

            if (sys_schema_redundant_index_pair_matches(redundant, dominant)) {
                rc = append_sys_schema_redundant_index_row(context, redundant, dominant);
            }
        }
    }
    return rc;
}

static int append_sys_x_schema_flattened_key_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *key_row
) {
    const char *non_unique = key_row->non_unique ? "1" : "0";
    const char *subpart_exists = key_row->subpart_exists ? "1" : "0";
    const char *values[sys_x_schema_flattened_keys_column_count] = {
        key_row->schema_name,
        key_row->table_name,
        key_row->index_name,
        non_unique,
        subpart_exists,
        key_row->index_columns,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_redundant_index_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
) {
    char *drop_index_sql = NULL;
    const char *redundant_non_unique = redundant->non_unique ? "1" : "0";
    const char *dominant_non_unique = dominant->non_unique ? "1" : "0";
    const char *subpart_exists = redundant->subpart_exists || dominant->subpart_exists ? "1" : "0";
    int rc = build_sys_redundant_drop_index_sql(redundant, &drop_index_sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        }
        return rc;
    }

    const char *values[sys_schema_redundant_indexes_column_count] = {
        redundant->schema_name,
        redundant->table_name,
        redundant->index_name,
        redundant->index_columns,
        redundant_non_unique,
        dominant->index_name,
        dominant->index_columns,
        dominant_non_unique,
        subpart_exists,
        drop_index_sql,
    };

    rc = append_information_schema_row(context->database, context->rows, values);
    free(drop_index_sql);
    return rc;
}

static bool sys_schema_redundant_index_pair_matches(
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
) {
    const char *redundant_sort_name = NULL;
    const char *dominant_sort_name = NULL;

    if (redundant == NULL || dominant == NULL ||
        strcmp(redundant->index_name, dominant->index_name) == 0) {
        return false;
    }
    if (strcmp(redundant->index_columns, dominant->index_columns) == 0) {
        if (redundant->non_unique && !dominant->non_unique) {
            return true;
        }
        redundant_sort_name =
            strcmp(redundant->index_name, "PRIMARY") == 0 ? "" : redundant->index_name;
        dominant_sort_name =
            strcmp(dominant->index_name, "PRIMARY") == 0 ? "" : dominant->index_name;
        if (redundant->non_unique == dominant->non_unique &&
            strcmp(redundant_sort_name, dominant_sort_name) > 0) {
            return true;
        }
    }
    if (redundant->non_unique && sys_schema_redundant_columns_left_prefix(
                                     redundant->index_columns,
                                     dominant->index_columns
                                 )) {
        return true;
    }
    return !dominant->non_unique && sys_schema_redundant_columns_left_prefix(
                                        dominant->index_columns,
                                        redundant->index_columns
                                    );
}

static bool sys_schema_redundant_columns_left_prefix(const char *left, const char *right) {
    size_t left_length = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    left_length = strlen(left);
    if (left_length == 0U || strncmp(left, right, left_length) != 0) {
        return false;
    }
    return right[left_length] == ',';
}

static int append_sys_flattened_key_row_from_index(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index,
    struct sys_flattened_key_row_set *rows
) {
    struct sys_flattened_key_row row = {0};
    int written = snprintf(row.schema_name, sizeof(row.schema_name), "%s", context->schema->name);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(row.schema_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys schema name");
        return MYLITE_ERROR;
    }
    written = snprintf(row.table_name, sizeof(row.table_name), "%s", table->name);
    if (written < 0 || (size_t)written >= sizeof(row.table_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys table name");
        return MYLITE_ERROR;
    }
    written = snprintf(row.index_name, sizeof(row.index_name), "%s", index->index.name);
    if (written < 0 || (size_t)written >= sizeof(row.index_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys index name");
        return MYLITE_ERROR;
    }

    row.non_unique = !index->index.is_unique;
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        row.subpart_exists =
            row.subpart_exists || index->parts[part_index].index_column.has_prefix_length;
    }

    rc = build_sys_flattened_key_index_columns(index, &row.index_columns);
    if (rc == MYLITE_OK) {
        rc = reserve_sys_flattened_key_rows(rows, rows->count + 1U);
    }
    if (rc != MYLITE_OK) {
        free(row.index_columns);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        }
        return rc;
    }

    rows->rows[rows->count] = row;
    ++rows->count;
    return MYLITE_OK;
}

static bool sys_flattened_key_index_supported(const struct loaded_index_info *index) {
    if (index == NULL || index->part_count == 0U) {
        return false;
    }
    return index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY;
}

static int build_sys_flattened_key_index_columns(
    const struct loaded_index_info *index,
    char **out_columns
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_columns = NULL;
    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, index->parts[part_index].column.name);
        }
    }
    if (rc == MYLITE_OK) {
        *out_columns = mylite_dynamic_string_take(&string);
        if (*out_columns == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int reserve_sys_flattened_key_rows(
    struct sys_flattened_key_row_set *rows,
    size_t required_capacity
) {
    struct sys_flattened_key_row *resized = NULL;
    size_t capacity =
        rows->capacity == 0U ? sys_flattened_key_row_initial_capacity : rows->capacity;

    if (required_capacity <= rows->capacity) {
        return MYLITE_OK;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(rows->rows[0])) {
        return MYLITE_NOMEM;
    }

    resized = realloc(rows->rows, capacity * sizeof(rows->rows[0]));
    if (resized == NULL) {
        return MYLITE_NOMEM;
    }
    rows->rows = resized;
    rows->capacity = capacity;
    return MYLITE_OK;
}

static void sys_flattened_key_rows_deinit(struct sys_flattened_key_row_set *rows) {
    if (rows == NULL) {
        return;
    }
    for (size_t index = 0U; index < rows->count; ++index) {
        free(rows->rows[index].index_columns);
    }
    free(rows->rows);
    *rows = (struct sys_flattened_key_row_set){0};
}

static int build_sys_redundant_drop_index_sql(
    const struct sys_flattened_key_row *redundant,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->schema_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " DROP INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->index_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_sys_schema_table_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_schema_table_lock_waits_column_count) {
        set_runtime_error(database, "invalid sys.schema_table_lock_waits columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_schema_tables_with_full_table_scans_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_schema_tables_with_full_table_scans_column_count) {
        set_runtime_error(database, "invalid sys.schema_tables_with_full_table_scans columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_schema_unused_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_unused_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_schema_unused_indexes_column_count) {
        set_runtime_error(database, "invalid sys.schema_unused_indexes columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_schema_unused_indexes_schema_rows,
        &context
    );
}

static int append_sys_schema_unused_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_unused_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_unused_indexes_table_rows,
        context
    );
}

static int append_sys_schema_unused_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_unused_indexes_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_unused_indexes_index_rows(context, table);
}

static int append_sys_schema_unused_indexes_index_rows(
    struct sys_schema_unused_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (sys_schema_unused_indexes_index_supported(&indexes[index])) {
            rc = append_sys_schema_unused_indexes_index_row(context, table->name, &indexes[index]);
        }
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_schema_unused_indexes_index_row(
    struct sys_schema_unused_indexes_context *context,
    const char *table_name,
    const struct loaded_index_info *index
) {
    const char *values[sys_schema_unused_indexes_column_count] = {
        context->schema->name,
        table_name,
        index->index.name,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static bool sys_schema_unused_indexes_index_supported(const struct loaded_index_info *index) {
    if (index == NULL || index->part_count == 0U || index->index.is_unique) {
        return false;
    }
    return index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL;
}

static int append_sys_schema_object_overview_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_object_overview_context context = {
        .database = database,
        .rows = rows,
        .groups = NULL,
        .group_count = 0U,
        .group_capacity = 0U,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_object_overview_column_count) {
        set_runtime_error(database, "invalid sys.schema_object_overview columns");
        return MYLITE_ERROR;
    }

    rc = collect_sys_schema_object_overview_builtin_groups(&context);
    if (rc == MYLITE_OK) {
        rc = collect_sys_schema_object_overview_mysql_system_index_groups(&context);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_object_overview_schema_groups,
            &context
        );
    }
    if (rc == MYLITE_OK) {
        rc = increment_sys_schema_object_overview_group(
            &context,
            "sys",
            "TRIGGER",
            mylite_execution_catalog_sys_sys_config_trigger_count()
        );
    }
    if (rc == MYLITE_OK) {
        sort_sys_schema_object_overview_groups(&context);
        rc = append_sys_schema_object_overview_group_rows(&context);
    }

    free(context.groups);
    return rc;
}

static int collect_sys_schema_object_overview_builtin_groups(
    struct sys_schema_object_overview_context *context
) {
    int rc = MYLITE_OK;

    for (size_t directory_index = 0U;
         rc == MYLITE_OK &&
         directory_index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++directory_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_at(directory_index);

        for (size_t table_index = 0U; rc == MYLITE_OK && table_index < directory->table_count;
             ++table_index) {
            const char *table_type =
                builtin_schema_table_type(directory, directory->table_names[table_index]);

            rc = increment_sys_schema_object_overview_group(
                context,
                directory->schema_name,
                table_type,
                1U
            );
        }
    }
    return rc;
}

static int collect_sys_schema_object_overview_mysql_system_index_groups(
    struct sys_schema_object_overview_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        const struct mylite_execution_catalog_mysql_system_table *definition =
            mylite_execution_catalog_mysql_system_table_definition_at(index);
        size_t count = mysql_system_table_primary_key_column_count(definition);

        if (SIZE_MAX - count < definition->secondary_index_count) {
            set_runtime_error(context->database, "sys.schema_object_overview count overflow");
            return MYLITE_ERROR;
        }
        count += definition->secondary_index_count;
        if (count > 0U) {
            rc = increment_sys_schema_object_overview_group(
                context,
                definition->schema_name,
                "INDEX (BTREE)",
                (uint64_t)count
            );
        }
    }
    return rc;
}

static int append_sys_schema_object_overview_schema_groups(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_object_overview_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_object_overview_table_groups,
        context
    );
}

static int append_sys_schema_object_overview_table_groups(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_object_overview_context *context = user_data;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }

    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        rc = increment_sys_schema_object_overview_group(
            context,
            context->schema->name,
            "BASE TABLE",
            1U
        );
        if (rc == MYLITE_OK) {
            rc = append_sys_schema_object_overview_base_table_index_groups(context, table);
        }
    } else if (table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        rc = increment_sys_schema_object_overview_group(context, context->schema->name, "VIEW", 1U);
    }

    return rc;
}

static int append_sys_schema_object_overview_base_table_index_groups(
    struct sys_schema_object_overview_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        char object_type[sizeof("INDEX (FULLTEXT)")];
        int written = snprintf(
            object_type,
            sizeof(object_type),
            "INDEX (%s)",
            show_index_index_type_text(&indexes[index])
        );

        if (written < 0 || (size_t)written >= sizeof(object_type)) {
            set_runtime_error(context->database, "failed to format sys.schema_object_overview row");
            rc = MYLITE_ERROR;
        } else {
            rc = increment_sys_schema_object_overview_group(
                context,
                context->schema->name,
                object_type,
                indexes[index].part_count
            );
        }
    }

    free(columns);
    loaded_index_infos_deinit(&indexes, &index_count);
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(context->database)) == MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else {
            set_runtime_error(context->database, "failed to build sys.schema_object_overview rows");
        }
    }
    return rc;
}

static int increment_sys_schema_object_overview_group(
    struct sys_schema_object_overview_context *context,
    const char *schema_name,
    const char *object_type,
    uint64_t count
) {
    if (context == NULL || context->database == NULL || schema_name == NULL ||
        object_type == NULL) {
        return MYLITE_MISUSE;
    }
    if (count == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < context->group_count; ++index) {
        struct sys_schema_object_overview_group *group = &context->groups[index];

        if (strcmp(group->db, schema_name) == 0 && strcmp(group->object_type, object_type) == 0) {
            if (UINT64_MAX - group->count < count) {
                set_runtime_error(context->database, "sys.schema_object_overview count overflow");
                return MYLITE_ERROR;
            }
            group->count += count;
            return MYLITE_OK;
        }
    }

    if (context->group_count == context->group_capacity) {
        int rc = grow_sys_schema_object_overview_groups(context);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    struct sys_schema_object_overview_group *group = &context->groups[context->group_count++];
    int db_written = snprintf(group->db, sizeof(group->db), "%s", schema_name);
    int object_type_written =
        snprintf(group->object_type, sizeof(group->object_type), "%s", object_type);

    if (db_written < 0 || (size_t)db_written >= sizeof(group->db) || object_type_written < 0 ||
        (size_t)object_type_written >= sizeof(group->object_type)) {
        set_runtime_error(context->database, "failed to format sys.schema_object_overview group");
        --context->group_count;
        return MYLITE_ERROR;
    }
    group->count = count;
    return MYLITE_OK;
}

static int grow_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
) {
    size_t new_capacity = context->group_capacity == 0U
                              ? sys_schema_object_overview_initial_group_capacity
                              : context->group_capacity * 2U;
    struct sys_schema_object_overview_group *groups = NULL;

    if (new_capacity < context->group_capacity ||
        new_capacity > SIZE_MAX / sizeof(*context->groups)) {
        set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    groups = realloc(context->groups, new_capacity * sizeof(*groups));
    if (groups == NULL) {
        set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->groups = groups;
    context->group_capacity = new_capacity;
    return MYLITE_OK;
}

static void sort_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
) {
    if (context == NULL || context->group_count < 2U) {
        return;
    }

    for (size_t index = 1U; index < context->group_count; ++index) {
        struct sys_schema_object_overview_group group = context->groups[index];
        size_t insert = index;

        while (insert > 0U &&
               compare_sys_schema_object_overview_groups(&group, &context->groups[insert - 1U]) <
                   0) {
            context->groups[insert] = context->groups[insert - 1U];
            --insert;
        }
        context->groups[insert] = group;
    }
}

static int compare_sys_schema_object_overview_groups(
    const struct sys_schema_object_overview_group *left,
    const struct sys_schema_object_overview_group *right
) {
    int db_compare = strcmp(left->db, right->db);

    if (db_compare != 0) {
        return db_compare;
    }
    return strcmp(left->object_type, right->object_type);
}

static int append_sys_schema_object_overview_group_rows(
    struct sys_schema_object_overview_context *context
) {
    char count_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < context->group_count; ++index) {
        const struct sys_schema_object_overview_group *group = &context->groups[index];
        const char *values[sys_schema_object_overview_column_count] = {
            group->db,
            group->object_type,
            count_text,
        };

        rc = format_uint64(context->database, group->count, count_text, sizeof(count_text));
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(context->database, context->rows, values);
        }
    }
    return rc;
}

static const char *mysql_system_table_column_comment(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t column_index
) {
    if (definition == NULL || definition->column_comments == NULL ||
        column_index >= definition->query_definition.column_count) {
        return "";
    }
    return definition->column_comments[column_index];
}

static const char *mysql_system_table_column_generation_expression(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t column_index
) {
    if (definition == NULL || definition->column_generation_expressions == NULL ||
        column_index >= definition->query_definition.column_count) {
        return "";
    }
    return definition->column_generation_expressions[column_index];
}

static size_t mysql_system_table_primary_key_column_count(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    size_t count = 0U;

    if (definition == NULL || definition->column_keys == NULL) {
        return 0U;
    }
    if (definition->primary_key_column_count > 0U) {
        return definition->primary_key_column_count;
    }
    for (size_t column_index = 0U; column_index < definition->query_definition.column_count;
         ++column_index) {
        if (strcmp(definition->column_keys[column_index], "PRI") == 0) {
            ++count;
        }
    }
    return count;
}

static const struct mylite_execution_catalog_column_definition *mysql_system_table_primary_key_column(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t key_index
) {
    size_t sequence = 0U;

    if (definition == NULL ||
        key_index >= mysql_system_table_primary_key_column_count(definition)) {
        return NULL;
    }
    if (definition->primary_key_column_count > 0U) {
        size_t column_index = definition->primary_key_column_indexes[key_index];

        if (column_index >= definition->query_definition.column_count) {
            return NULL;
        }
        return &definition->query_definition.columns[column_index];
    }
    for (size_t column_index = 0U; column_index < definition->query_definition.column_count;
         ++column_index) {
        if (strcmp(definition->column_keys[column_index], "PRI") != 0) {
            continue;
        }
        if (sequence == key_index) {
            return &definition->query_definition.columns[column_index];
        }
        ++sequence;
    }
    return NULL;
}

static int append_mysql_innodb_index_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = append_mysql_innodb_index_stats_builtin_index_rows(
        database,
        rows,
        "mysql",
        "component",
        "PRIMARY",
        "0",
        "component_id"
    );

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_builtin_index_rows(
            database,
            rows,
            "sys",
            "sys_config",
            "PRIMARY",
            "6",
            "variable"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_builtin_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *distinct_count,
    const char *stat_description
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = append_mysql_innodb_index_stats_row(
        database,
        rows,
        schema_name,
        table_name,
        index_name,
        "n_diff_pfx01",
        distinct_count,
        "1",
        stat_description
    );
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema_name,
            table_name,
            index_name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema_name,
            table_name,
            index_name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_table_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = append_mysql_innodb_table_stats_builtin_row(database, rows, "mysql", "component", "0");

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_table_stats_builtin_row(database, rows, "sys", "sys_config", "6");
    }
    return rc;
}

static int append_mysql_innodb_table_stats_builtin_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *row_count
) {
    char last_update[datetime_text_length + 1U];
    int rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));

    if (rows->definition->column_count != mysql_innodb_table_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_table_stats columns");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_table_stats_column_count] = {
            schema_name,
            table_name,
            last_update,
            row_count,
            "1",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_mysql_system_table_catalog_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    struct mysql_system_table_catalog_context context = {
        .database = database,
        .rows = rows,
        .definition = definition,
        .schema = NULL,
    };

    return mylite_catalog_for_each_schema(
        database,
        append_mysql_system_table_catalog_schema,
        &context
    );
}

static int append_mysql_system_table_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct mysql_system_table_catalog_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->definition == NULL) {
        return MYLITE_MISUSE;
    }
    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_mysql_system_table_catalog_table,
        context
    );
}

static int append_mysql_system_table_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct mysql_system_table_catalog_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->definition == NULL || context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    if (strcmp(context->definition->schema_name, "mysql") == 0 &&
        strcmp(context->definition->query_definition.name, "innodb_index_stats") == 0) {
        return append_mysql_innodb_index_stats_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    }
    if (strcmp(context->definition->schema_name, "mysql") == 0 &&
        strcmp(context->definition->query_definition.name, "innodb_table_stats") == 0) {
        return append_mysql_innodb_table_stats_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    }

    set_runtime_error(context->database, "invalid mysql system table catalog row");
    return MYLITE_ERROR;
}

static int append_mysql_innodb_index_stats_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    const struct loaded_index_info *clustered_index = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        clustered_index = information_schema_innodb_clustered_index(indexes, index_count);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_mysql_innodb_index_stats_index_rows(
            database,
            rows,
            schema,
            table,
            clustered_index,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK && clustered_index == NULL) {
        rc = append_mysql_innodb_index_stats_generated_cluster_rows(database, rows, schema, table);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_mysql_innodb_index_stats_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
) {
    size_t prefix_count = 0U;
    int rc = mysql_innodb_index_stats_prefix_count(database, clustered_index, index, &prefix_count);

    if (rc == MYLITE_OK && prefix_count == 0U) {
        return MYLITE_OK;
    }
    for (size_t prefix = 1U; rc == MYLITE_OK && prefix <= prefix_count; ++prefix) {
        rc = append_mysql_innodb_index_stats_diff_row(
            database,
            rows,
            schema,
            table,
            clustered_index,
            index,
            prefix
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_generated_cluster_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    int rc = append_mysql_innodb_index_stats_generated_diff_row(database, rows, schema, table);

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count
) {
    char stat_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char stat_value[integer_text_capacity];
    char *stat_description = NULL;
    int64_t distinct_count = 0;
    int written = snprintf(stat_name, sizeof(stat_name), "n_diff_pfx%02zu", prefix_count);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(stat_name)) {
        set_runtime_error(database, "failed to format mysql.innodb_index_stats stat name");
        return MYLITE_ERROR;
    }

    rc = read_mysql_innodb_index_stats_distinct_count(
        database,
        table,
        clustered_index,
        index,
        prefix_count,
        &distinct_count
    );
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, distinct_count, stat_value, sizeof(stat_value));
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_prefix_description(
            database,
            clustered_index,
            index,
            prefix_count,
            &stat_description
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            stat_name,
            stat_value,
            "1",
            stat_description
        );
    }

    free(stat_description);
    return rc;
}

static int append_mysql_innodb_index_stats_generated_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char stat_value[integer_text_capacity];
    int64_t row_count = 0;
    int rc = read_show_table_status_row_count(database, table, &row_count);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read mysql.innodb_index_stats rows");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, row_count, stat_value, sizeof(stat_value));
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "n_diff_pfx01",
            stat_value,
            "1",
            "DB_ROW_ID"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_page_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_description
) {
    return append_mysql_innodb_index_stats_row(
        database,
        rows,
        schema_name,
        table_name,
        index_name,
        stat_name,
        "1",
        NULL,
        stat_description
    );
}

static int append_mysql_innodb_index_stats_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_value,
    const char *sample_size,
    const char *stat_description
) {
    char last_update[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_index_stats_column_count] = {
            schema_name,
            table_name,
            index_name,
            last_update,
            stat_name,
            stat_value,
            sample_size,
            stat_description,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int mysql_innodb_index_stats_prefix_count(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t *out_count
) {
    size_t clustered_part_count = 0U;
    size_t prefix_count = 0U;
    int rc = MYLITE_OK;

    if (out_count == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0U;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
        index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return MYLITE_OK;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY || index == clustered_index ||
        index->index.is_unique) {
        *out_count = index->part_count;
        return MYLITE_OK;
    }
    if (index->index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats index type");
        return MYLITE_ERROR;
    }

    prefix_count = index->part_count;
    if (clustered_index == NULL) {
        if (prefix_count == SIZE_MAX) {
            set_runtime_error(database, "invalid mysql.innodb_index_stats prefix count");
            return MYLITE_ERROR;
        }
        *out_count = prefix_count + 1U;
        return MYLITE_OK;
    }

    rc = information_schema_innodb_secondary_clustered_part_count(
        database,
        index,
        clustered_index,
        &clustered_part_count
    );
    if (rc == MYLITE_OK) {
        if (prefix_count > SIZE_MAX - clustered_part_count) {
            set_runtime_error(database, "invalid mysql.innodb_index_stats prefix count");
            return MYLITE_ERROR;
        }
        *out_count = prefix_count + clustered_part_count;
    }
    return rc;
}

static int read_mysql_innodb_index_stats_distinct_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_mysql_innodb_index_stats_distinct_sql(
        database,
        table,
        clustered_index,
        index,
        prefix_count,
        &sql
    );

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    return rc;
}

static int build_mysql_innodb_index_stats_distinct_sql(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (table == NULL || index == NULL || prefix_count == 0U || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM (SELECT DISTINCT ");
    for (size_t prefix_index = 0U; rc == MYLITE_OK && prefix_index < prefix_count; ++prefix_index) {
        if (prefix_index > 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_mysql_innodb_index_stats_prefix_expression(
                database,
                &string,
                clustered_index,
                index,
                prefix_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, table->physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ")");
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_mysql_innodb_index_stats_prefix_expression(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
) {
    const struct loaded_index_part *part = NULL;

    if (prefix_index < index->part_count) {
        return append_loaded_key_part_sql(string, &index->parts[prefix_index], NULL);
    }
    if (clustered_index == NULL) {
        return mylite_dynamic_string_append(string, "rowid");
    }

    part = mysql_innodb_index_stats_missing_clustered_part(
        clustered_index,
        index,
        prefix_index - index->part_count
    );
    if (part == NULL) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats prefix column");
        return MYLITE_ERROR;
    }
    return append_loaded_key_part_sql(string, part, NULL);
}

static int append_mysql_innodb_index_stats_prefix_description(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_description
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (index == NULL || prefix_count == 0U || out_description == NULL) {
        return MYLITE_MISUSE;
    }
    *out_description = NULL;
    mylite_dynamic_string_init(&string);

    for (size_t prefix_index = 0U; rc == MYLITE_OK && prefix_index < prefix_count; ++prefix_index) {
        if (prefix_index > 0U) {
            rc = mylite_dynamic_string_append(&string, ",");
        }
        if (rc == MYLITE_OK) {
            rc = append_mysql_innodb_index_stats_prefix_name(
                database,
                &string,
                clustered_index,
                index,
                prefix_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        *out_description = mylite_dynamic_string_take(&string);
        if (*out_description == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_mysql_innodb_index_stats_prefix_name(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
) {
    const struct loaded_index_part *part = NULL;

    if (prefix_index < index->part_count) {
        return mylite_dynamic_string_append(string, index->parts[prefix_index].column.name);
    }
    if (clustered_index == NULL) {
        return mylite_dynamic_string_append(string, "DB_ROW_ID");
    }

    part = mysql_innodb_index_stats_missing_clustered_part(
        clustered_index,
        index,
        prefix_index - index->part_count
    );
    if (part == NULL) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats prefix description");
        return MYLITE_ERROR;
    }
    return mylite_dynamic_string_append(string, part->column.name);
}

static const struct loaded_index_part *mysql_innodb_index_stats_missing_clustered_part(
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t missing_index
) {
    size_t current = 0U;

    if (clustered_index == NULL || index == NULL) {
        return NULL;
    }
    for (size_t part_index = 0U; part_index < clustered_index->part_count; ++part_index) {
        if (information_schema_innodb_index_contains_column(
                index,
                clustered_index->parts[part_index].column.column_id
            )) {
            continue;
        }
        if (current == missing_index) {
            return &clustered_index->parts[part_index];
        }
        ++current;
    }
    return NULL;
}

static int append_mysql_innodb_table_stats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char last_update[datetime_text_length + 1U];
    char row_count_text[integer_text_capacity];
    char other_index_size_text[integer_text_capacity];
    int64_t row_count = 0;
    int64_t other_index_size = 0;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_table_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_table_stats columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read mysql.innodb_table_stats rows");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_count_nonprimary_indexes(
            database,
            table->table_id,
            &other_index_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            row_count,
            row_count_text,
            sizeof(row_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            other_index_size,
            other_index_size_text,
            sizeof(other_index_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_table_stats_column_count] = {
            schema->name,
            table->name,
            last_update,
            row_count_text,
            "1",
            other_index_size_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int mysql_system_current_timestamp(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    return format_session_timestamp_epoch_text(
        database,
        current_wall_clock_epoch(),
        buffer,
        buffer_size
    );
}

static int mysql_system_current_timestamp2(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    char timestamp[datetime_text_length + 1U];
    int rc = mysql_system_current_timestamp(database, timestamp, sizeof(timestamp));

    if (rc == MYLITE_OK) {
        int written = snprintf(buffer, buffer_size, "%s.00", timestamp);

        if (written < 0 || (size_t)written >= buffer_size) {
            set_runtime_error(database, "failed to format timestamp(2) metadata");
            rc = MYLITE_ERROR;
        }
    }
    return rc;
}

static int resolve_information_schema_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int rc = MYLITE_OK;

    *out_query = (struct information_schema_query){0};
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT supports only non-distinct metadata queries"
        );
        return MYLITE_ERROR;
    }

    rc = information_schema_resolve_source(database, from_clause, out_query);
    while (rc == MYLITE_OK && optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports only WHERE, ORDER BY, and LIMIT"
            );
            rc = MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_projection(database, select_list, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_where(database, where_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        out_query->limit = (struct planned_select_limit){
            .has_limit = false,
            .row_count = 0,
            .has_offset = false,
            .offset = 0,
        };
        rc = information_schema_plan_order(database, order_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_limit(database, limit_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }

    information_schema_query_deinit(out_query);
    return rc;
}

static void information_schema_query_deinit(struct information_schema_query *query) {
    if (query == NULL) {
        return;
    }
    for (size_t index = 0U; index < query->projection_count; ++index) {
        free(query->projection_names[index]);
    }
    free((void *)query->projection_names);
    free(query->projection_indexes);
    *query = (struct information_schema_query){0};
}

static int execute_information_schema_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_row_set rows = {0};
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    int rc = build_information_schema_rows(database, context, query->definition, &rows);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_result_columns(database, result, query);
    }
    if (rc == MYLITE_OK && query->is_count_star) {
        rc = information_schema_append_count_result(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    } else if (rc == MYLITE_OK) {
        rc = information_schema_append_result_rows(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    }
    if (rc == MYLITE_OK && query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST &&
        read_row_count > 0U) {
        rc = append_show_processlist_warning(database);
    }
    if (rc == MYLITE_OK && query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROFILING &&
        !(query->limit.has_limit && query->limit.row_count == 0)) {
        rc = append_information_schema_profiling_warning(database);
    }
    information_schema_row_set_deinit(&rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    if (apply_sql_select_limit && !query->limit.has_limit) {
        apply_sql_select_limit_to_result(database, result);
    }
    return finish_successful_result(database, result, out_result);
}

static int build_information_schema_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    struct information_schema_row_set *out_rows
) {
    int rc = MYLITE_OK;

    *out_rows = (struct information_schema_row_set){.definition = definition};
    if (definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST) {
        rc = append_information_schema_processlist_system_row(database, context, out_rows);
    } else {
        rc = append_information_schema_system_rows(database, out_rows);
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_catalog_rows(database, out_rows);
    }
    if (rc != MYLITE_OK) {
        information_schema_row_set_deinit(out_rows);
    }
    return rc;
}

static void information_schema_row_set_deinit(struct information_schema_row_set *rows) {
    if (rows == NULL) {
        return;
    }
    for (size_t row_index = 0U; row_index < rows->row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < rows->definition->column_count;
             ++column_index) {
            free(rows->rows[row_index][column_index]);
        }
        free((void *)rows->rows[row_index]);
    }
    free((void *)rows->rows);
    *rows = (struct information_schema_row_set){0};
}

static int append_information_schema_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    switch (rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA:
        return append_information_schema_schemata_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA_EXTENSIONS:
        return append_information_schema_schemata_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
        return append_information_schema_tables_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLESPACES_EXTENSIONS:
        return append_information_schema_tablespaces_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_FILES:
        return append_information_schema_files_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_DATAFILES:
        return append_information_schema_innodb_datafiles_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES:
        return append_information_schema_innodb_tablespaces_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_SESSION_TEMP_TABLESPACES:
        return append_information_schema_innodb_session_temp_tablespaces_system_rows(
            database,
            rows
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_RESET:
        return append_information_schema_innodb_cmp_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_POOL_STATS:
        return append_information_schema_innodb_buffer_pool_stats_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM_RESET:
        return append_information_schema_innodb_cmpmem_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DEFAULT_STOPWORD:
        return append_information_schema_innodb_ft_default_stopword_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES_BRIEF:
        return append_information_schema_innodb_tablespaces_brief_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
        return append_information_schema_tables_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
        return append_information_schema_columns_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
        return append_information_schema_columns_extensions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_CHARACTER_SETS:
        return append_information_schema_character_sets_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATIONS:
        return append_information_schema_collations_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATION_CHARACTER_SET_APPLICABILITY:
        return append_information_schema_collation_applicability_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_ENGINES:
        return append_information_schema_engines_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_PLUGINS:
        return append_information_schema_plugins_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_KEYWORDS:
        return append_information_schema_keywords_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_PRIVILEGES:
        return append_information_schema_user_privileges_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_ATTRIBUTES:
        return append_information_schema_user_attributes_system_row(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_RESOURCE_GROUPS:
        return append_information_schema_resource_groups_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_UNITS_OF_MEASURE:
        return append_information_schema_st_units_of_measure_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_PARTITIONS:
        return append_information_schema_partitions_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS:
        return append_information_schema_table_constraints_mysql_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS_EXTENSIONS:
        return append_information_schema_table_constraints_extensions_mysql_system_rows(
            database,
            rows
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_KEY_COLUMN_USAGE:
        return append_information_schema_key_column_usage_mysql_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS:
        return append_information_schema_statistics_mysql_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEWS:
        return append_information_schema_views_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE:
        return append_information_schema_view_table_usage_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_ROUTINE_USAGE:
        return append_information_schema_view_routine_usage_system_rows(database, rows);
    case MYLITE_EXECUTION_CATALOG_TABLE_EVENTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ADMINISTRABLE_ROLE_AUTHORIZATIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_APPLICABLE_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_ENABLED_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PARAMETERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROFILING:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROUTINES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FIELDS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN_COLS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_BEING_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_CACHE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_METRICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_VIRTUAL:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TRX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE_LRU:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CACHED_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TEMP_TABLE_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_GEOMETRY_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_SPATIAL_REFERENCE_SYSTEMS:
    case MYLITE_EXECUTION_CATALOG_TABLE_CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_COLUMN_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_ROUTINE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_TABLE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_REFERENTIAL_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_CHECK_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMA_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COMPONENT:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DEFAULT_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DB:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ENGINE_COST:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_FUNC:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GENERAL_LOG:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GLOBAL_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GTID_EXECUTED:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_CATEGORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_KEYWORD:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_RELATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_TOPIC:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COLUMNS_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_NDB_BINLOG_INDEX:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PLUGIN:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROCS_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROXIES_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ROLE_EDGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVER_COST:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_MASTER_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_WORKER_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLOW_LOG:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TABLES_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_LEAP_SECOND:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_USER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_SCHEMA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_BY_THREAD_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_LATEST_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_USER_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_PS_CHECK_LOST_INSTRUMENTATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_INDEX_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_OBJECT_OVERVIEW:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_REDUNDANT_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_FLATTENED_KEYS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_UNUSED_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_BY_THREAD_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_LATEST_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_INDEX_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS:
        return MYLITE_OK;
    case MYLITE_EXECUTION_CATALOG_TABLE_TRIGGERS:
        return append_information_schema_triggers_system_rows(database, rows);
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA table");
    return MYLITE_ERROR;
}

static int append_information_schema_triggers_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char created[timestamp_2_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_triggers_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TRIGGERS columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp2(database, created, sizeof(created));
    for (size_t trigger_index = 0U;
         rc == MYLITE_OK && trigger_index < mylite_execution_catalog_sys_sys_config_trigger_count();
         ++trigger_index) {
        const struct mylite_execution_catalog_sys_config_trigger *trigger =
            mylite_execution_catalog_sys_sys_config_trigger_at(trigger_index);
        const char *values[information_schema_triggers_column_count] = {
            "def",
            "sys",
            trigger->name,
            trigger->event,
            "def",
            "sys",
            "sys_config",
            "1",
            NULL,
            mylite_execution_catalog_sys_sys_config_trigger_action_statement(),
            "ROW",
            "BEFORE",
            NULL,
            NULL,
            "OLD",
            "NEW",
            created,
            mylite_execution_catalog_sys_sys_config_trigger_sql_mode(),
            "mysql.sys@localhost",
            "utf8mb4",
            "utf8mb4_0900_ai_ci",
            "utf8mb4_0900_ai_ci",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_views_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_views_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.VIEWS columns");
        return MYLITE_ERROR;
    }

    for (size_t view_index = 0U;
         rc == MYLITE_OK &&
         view_index < mylite_execution_catalog_builtin_sys_view_definition_count();
         ++view_index) {
        const struct mylite_execution_catalog_builtin_sys_view *view =
            mylite_execution_catalog_builtin_sys_view_definition_at(view_index);
        const char *is_updatable =
            strcmp(view->name, "ps_check_lost_instrumentation") == 0 ||
                    strcmp(view->name, "io_global_by_file_by_bytes") == 0 ||
                    strcmp(view->name, "io_global_by_file_by_latency") == 0 ||
                    strcmp(view->name, "io_global_by_wait_by_bytes") == 0 ||
                    strcmp(view->name, "io_global_by_wait_by_latency") == 0 ||
                    strcmp(view->name, "host_summary_by_file_io_type") == 0 ||
                    strcmp(view->name, "x$host_summary_by_file_io_type") == 0 ||
                    strcmp(view->name, "host_summary_by_stages") == 0 ||
                    strcmp(view->name, "x$host_summary_by_stages") == 0 ||
                    strcmp(view->name, "host_summary_by_statement_type") == 0 ||
                    strcmp(view->name, "x$host_summary_by_statement_type") == 0 ||
                    strcmp(view->name, "x$io_global_by_file_by_bytes") == 0 ||
                    strcmp(view->name, "x$io_global_by_file_by_latency") == 0 ||
                    strcmp(view->name, "x$io_global_by_wait_by_bytes") == 0 ||
                    strcmp(view->name, "x$io_global_by_wait_by_latency") == 0 ||
                    strcmp(view->name, "schema_index_statistics") == 0 ||
                    strcmp(view->name, "x$schema_index_statistics") == 0 ||
                    strcmp(view->name, "schema_tables_with_full_table_scans") == 0 ||
                    strcmp(view->name, "x$schema_tables_with_full_table_scans") == 0 ||
                    strcmp(view->name, "schema_unused_indexes") == 0
                ? "YES"
                : "NO";
        const char *values[information_schema_views_column_count] = {
            "def",
            "sys",
            view->name,
            view->view_definition,
            "NONE",
            is_updatable,
            "mysql.sys@localhost",
            "INVOKER",
            "utf8mb4",
            "utf8mb4_0900_ai_ci",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_view_table_usage_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct builtin_view_dependency_row {
        const char *view_name;
        const char *table_schema;
        const char *table_name;
    };
    static const struct builtin_view_dependency_row dependency_rows[] = {
        {"host_summary", "performance_schema", "accounts"},
        {"host_summary", "sys", "x$host_summary_by_file_io"},
        {"host_summary", "sys", "x$host_summary_by_statement_latency"},
        {"host_summary", "sys", "x$memory_by_host_by_current_bytes"},
        {"host_summary_by_file_io",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"host_summary_by_file_io_type",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"host_summary_by_stages",
         "performance_schema",
         "events_stages_summary_by_host_by_event_name"},
        {"host_summary_by_statement_latency",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"host_summary_by_statement_type",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"innodb_buffer_stats_by_schema", "information_schema", "INNODB_BUFFER_PAGE"},
        {"innodb_buffer_stats_by_table", "information_schema", "INNODB_BUFFER_PAGE"},
        {"memory_by_host_by_current_bytes",
         "performance_schema",
         "memory_summary_by_host_by_event_name"},
        {"memory_by_thread_by_current_bytes",
         "performance_schema",
         "memory_summary_by_thread_by_event_name"},
        {"memory_by_thread_by_current_bytes", "performance_schema", "threads"},
        {"memory_by_user_by_current_bytes",
         "performance_schema",
         "memory_summary_by_user_by_event_name"},
        {"innodb_lock_waits", "information_schema", "INNODB_TRX"},
        {"innodb_lock_waits", "performance_schema", "data_lock_waits"},
        {"innodb_lock_waits", "performance_schema", "data_locks"},
        {"innodb_lock_waits", "sys", "sys_config"},
        {"io_by_thread_by_latency",
         "performance_schema",
         "events_waits_summary_by_thread_by_event_name"},
        {"io_by_thread_by_latency", "performance_schema", "threads"},
        {"io_global_by_file_by_bytes", "performance_schema", "file_summary_by_instance"},
        {"io_global_by_file_by_bytes", "performance_schema", "global_variables"},
        {"io_global_by_file_by_latency", "performance_schema", "file_summary_by_instance"},
        {"io_global_by_file_by_latency", "performance_schema", "global_variables"},
        {"io_global_by_wait_by_bytes", "performance_schema", "file_summary_by_event_name"},
        {"io_global_by_wait_by_latency", "performance_schema", "file_summary_by_event_name"},
        {"latest_file_io", "information_schema", "PROCESSLIST"},
        {"latest_file_io", "performance_schema", "events_waits_history_long"},
        {"latest_file_io", "performance_schema", "global_variables"},
        {"latest_file_io", "performance_schema", "threads"},
        {"ps_check_lost_instrumentation", "performance_schema", "global_status"},
        {"schema_auto_increment_columns", "information_schema", "COLUMNS"},
        {"schema_auto_increment_columns", "information_schema", "TABLES"},
        {"schema_index_statistics", "performance_schema", "table_io_waits_summary_by_index_usage"},
        {"schema_object_overview", "information_schema", "EVENTS"},
        {"schema_object_overview", "information_schema", "ROUTINES"},
        {"schema_object_overview", "information_schema", "STATISTICS"},
        {"schema_object_overview", "information_schema", "TABLES"},
        {"schema_object_overview", "information_schema", "TRIGGERS"},
        {"schema_redundant_indexes", "sys", "x$schema_flattened_keys"},
        {"schema_table_lock_waits", "performance_schema", "events_statements_current"},
        {"schema_table_lock_waits", "performance_schema", "metadata_locks"},
        {"schema_table_lock_waits", "performance_schema", "threads"},
        {"schema_table_lock_waits", "sys", "sys_config"},
        {"schema_table_statistics", "performance_schema", "table_io_waits_summary_by_table"},
        {"schema_table_statistics", "sys", "x$ps_schema_table_statistics_io"},
        {"schema_table_statistics_with_buffer",
         "performance_schema",
         "table_io_waits_summary_by_table"},
        {"schema_table_statistics_with_buffer", "sys", "x$innodb_buffer_stats_by_table"},
        {"schema_table_statistics_with_buffer", "sys", "x$ps_schema_table_statistics_io"},
        {"schema_tables_with_full_table_scans",
         "performance_schema",
         "table_io_waits_summary_by_index_usage"},
        {"schema_unused_indexes", "information_schema", "STATISTICS"},
        {"schema_unused_indexes", "performance_schema", "table_io_waits_summary_by_index_usage"},
        {"x$host_summary", "performance_schema", "accounts"},
        {"x$host_summary", "sys", "x$host_summary_by_file_io"},
        {"x$host_summary", "sys", "x$host_summary_by_statement_latency"},
        {"x$host_summary", "sys", "x$memory_by_host_by_current_bytes"},
        {"x$host_summary_by_file_io",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"x$host_summary_by_file_io_type",
         "performance_schema",
         "events_waits_summary_by_host_by_event_name"},
        {"x$host_summary_by_stages",
         "performance_schema",
         "events_stages_summary_by_host_by_event_name"},
        {"x$host_summary_by_statement_latency",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"x$host_summary_by_statement_type",
         "performance_schema",
         "events_statements_summary_by_host_by_event_name"},
        {"x$innodb_buffer_stats_by_schema", "information_schema", "INNODB_BUFFER_PAGE"},
        {"x$innodb_buffer_stats_by_table", "information_schema", "INNODB_BUFFER_PAGE"},
        {"x$memory_by_host_by_current_bytes",
         "performance_schema",
         "memory_summary_by_host_by_event_name"},
        {"x$memory_by_thread_by_current_bytes",
         "performance_schema",
         "memory_summary_by_thread_by_event_name"},
        {"x$memory_by_thread_by_current_bytes", "performance_schema", "threads"},
        {"x$memory_by_user_by_current_bytes",
         "performance_schema",
         "memory_summary_by_user_by_event_name"},
        {"x$innodb_lock_waits", "information_schema", "INNODB_TRX"},
        {"x$innodb_lock_waits", "performance_schema", "data_lock_waits"},
        {"x$innodb_lock_waits", "performance_schema", "data_locks"},
        {"x$io_by_thread_by_latency",
         "performance_schema",
         "events_waits_summary_by_thread_by_event_name"},
        {"x$io_by_thread_by_latency", "performance_schema", "threads"},
        {"x$io_global_by_file_by_bytes", "performance_schema", "file_summary_by_instance"},
        {"x$io_global_by_file_by_latency", "performance_schema", "file_summary_by_instance"},
        {"x$io_global_by_wait_by_bytes", "performance_schema", "file_summary_by_event_name"},
        {"x$io_global_by_wait_by_latency", "performance_schema", "file_summary_by_event_name"},
        {"x$latest_file_io", "information_schema", "PROCESSLIST"},
        {"x$latest_file_io", "performance_schema", "events_waits_history_long"},
        {"x$latest_file_io", "performance_schema", "threads"},
        {"x$ps_schema_table_statistics_io", "performance_schema", "file_summary_by_instance"},
        {"x$schema_flattened_keys", "information_schema", "STATISTICS"},
        {"x$schema_index_statistics",
         "performance_schema",
         "table_io_waits_summary_by_index_usage"},
        {"x$schema_table_lock_waits", "performance_schema", "events_statements_current"},
        {"x$schema_table_lock_waits", "performance_schema", "metadata_locks"},
        {"x$schema_table_lock_waits", "performance_schema", "threads"},
        {"x$schema_table_statistics", "performance_schema", "table_io_waits_summary_by_table"},
        {"x$schema_table_statistics", "sys", "x$ps_schema_table_statistics_io"},
        {"x$schema_table_statistics_with_buffer",
         "performance_schema",
         "table_io_waits_summary_by_table"},
        {"x$schema_table_statistics_with_buffer", "sys", "x$innodb_buffer_stats_by_table"},
        {"x$schema_table_statistics_with_buffer", "sys", "x$ps_schema_table_statistics_io"},
        {"x$schema_tables_with_full_table_scans",
         "performance_schema",
         "table_io_waits_summary_by_index_usage"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_view_table_usage_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.VIEW_TABLE_USAGE columns");
        return MYLITE_ERROR;
    }

    for (size_t dependency_index = 0U;
         rc == MYLITE_OK && dependency_index < sizeof(dependency_rows) / sizeof(dependency_rows[0]);
         ++dependency_index) {
        const struct builtin_view_dependency_row *dependency = &dependency_rows[dependency_index];
        const char *values[information_schema_view_table_usage_column_count] = {
            "def",
            "sys",
            dependency->view_name,
            "def",
            dependency->table_schema,
            dependency->table_name,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_view_routine_usage_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct builtin_view_routine_dependency_row {
        const char *view_name;
        const char *routine_name;
    };
    static const struct builtin_view_routine_dependency_row dependency_rows[] = {
        {"innodb_lock_waits", "format_statement"},
        {"innodb_lock_waits", "quote_identifier"},
        {"io_global_by_file_by_bytes", "format_path"},
        {"io_global_by_file_by_latency", "format_path"},
        {"latest_file_io", "format_path"},
        {"schema_table_lock_waits", "format_statement"},
        {"schema_table_lock_waits", "ps_thread_account"},
        {"x$innodb_lock_waits", "quote_identifier"},
        {"x$ps_schema_table_statistics_io", "extract_schema_from_file_name"},
        {"x$ps_schema_table_statistics_io", "extract_table_from_file_name"},
        {"x$schema_table_lock_waits", "ps_thread_account"},
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_view_routine_usage_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE columns");
        return MYLITE_ERROR;
    }

    for (size_t dependency_index = 0U;
         rc == MYLITE_OK && dependency_index < sizeof(dependency_rows) / sizeof(dependency_rows[0]);
         ++dependency_index) {
        const struct builtin_view_routine_dependency_row *dependency =
            &dependency_rows[dependency_index];
        const char *values[information_schema_view_routine_usage_column_count] = {
            "def",
            "sys",
            dependency->view_name,
            "def",
            "sys",
            dependency->routine_name,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_catalog_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct information_schema_catalog_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    switch (rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_CHARACTER_SETS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ADMINISTRABLE_ROLE_AUTHORIZATIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_APPLICABLE_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLLATION_CHARACTER_SET_APPLICABILITY:
    case MYLITE_EXECUTION_CATALOG_TABLE_ENABLED_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_ENGINES:
    case MYLITE_EXECUTION_CATALOG_TABLE_EVENTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_KEYWORDS:
    case MYLITE_EXECUTION_CATALOG_TABLE_OPTIMIZER_TRACE:
    case MYLITE_EXECUTION_CATALOG_TABLE_PARAMETERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PLUGINS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST:
    case MYLITE_EXECUTION_CATALOG_TABLE_PROFILING:
    case MYLITE_EXECUTION_CATALOG_TABLE_RESOURCE_GROUPS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROUTINES:
    case MYLITE_EXECUTION_CATALOG_TABLE_FILES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMPMEM_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CMP_PER_INDEX_RESET:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_DATAFILES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_BEING_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DELETED:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_CACHE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_INDEX_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FT_DEFAULT_STOPWORD:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES_BRIEF:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESPACES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_SESSION_TEMP_TABLESPACES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TRX:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_PAGE_LRU:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_BUFFER_POOL_STATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_CACHED_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_METRICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TEMP_TABLE_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_SPATIAL_REFERENCE_SYSTEMS:
    case MYLITE_EXECUTION_CATALOG_TABLE_CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_UNITS_OF_MEASURE:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMN_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_COLUMN_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_ROUTINE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_ROLE_TABLE_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMA_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TRIGGERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_ATTRIBUTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_USER_PRIVILEGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_ROUTINE_USAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COMPONENT:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DEFAULT_ROLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_DB:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ENGINE_COST:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_FUNC:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GENERAL_LOG:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GLOBAL_GRANTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_GTID_EXECUTED:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_CATEGORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_KEYWORD:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_RELATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_HELP_TOPIC:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_COLUMNS_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_NDB_BINLOG_INDEX:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PLUGIN:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROCS_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_PROXIES_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_ROLE_EDGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVER_COST:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SERVERS:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_MASTER_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_RELAY_LOG_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLAVE_WORKER_INFO:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_SLOW_LOG:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TABLES_PRIV:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_LEAP_SECOND:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_NAME:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_TIME_ZONE_TRANSITION_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_MYSQL_USER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_HOST_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_SCHEMA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_BUFFER_STATS_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_INNODB_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_BY_THREAD_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_FILE_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_LATEST_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_MEMORY_BY_USER_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_PS_CHECK_LOST_INSTRUMENTATION:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_INDEX_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_OBJECT_OVERVIEW:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_REDUNDANT_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_FLATTENED_KEYS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_UNUSED_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STAGES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_BUFFER_STATS_BY_TABLE:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_INNODB_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_BY_THREAD_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_LATEST_FILE_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_LOCK_WAITS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_INDEX_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER:
    case MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS:
        return MYLITE_OK;
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA:
    case MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLESPACES_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEWS:
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_CHECK_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS_EXTENSIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_KEY_COLUMN_USAGE:
    case MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS:
    case MYLITE_EXECUTION_CATALOG_TABLE_PARTITIONS:
    case MYLITE_EXECUTION_CATALOG_TABLE_REFERENTIAL_CONSTRAINTS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FIELDS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN_COLS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_COLUMNS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLES:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS:
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_VIRTUAL:
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_GEOMETRY_COLUMNS:
        break;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_information_schema_catalog_schema,
        &context
    );
}

static int append_information_schema_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct information_schema_catalog_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (context->rows->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA) {
        return append_information_schema_schemata_schema_row(
            context->database,
            context->rows,
            schema
        );
    }
    if (context->rows->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_SCHEMATA_EXTENSIONS) {
        return append_information_schema_schemata_extensions_schema_row(
            context->database,
            context->rows,
            schema
        );
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_information_schema_catalog_table,
        context
    );
}

static int append_information_schema_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct information_schema_catalog_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        return append_information_schema_catalog_view_table(context, table);
    }
    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        return append_information_schema_catalog_base_table(context, table);
    }

    return MYLITE_OK;
}

static int append_information_schema_catalog_view_table(
    struct information_schema_catalog_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    switch (context->rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
        return append_information_schema_tables_view_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
        return append_information_schema_tables_extensions_table_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
        return append_information_schema_columns_view_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
        return append_information_schema_columns_extensions_table_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEWS:
        return append_information_schema_views_view_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE:
        return append_information_schema_view_table_usage_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    default:
        return MYLITE_OK;
    }
}

static int append_information_schema_catalog_base_table(
    struct information_schema_catalog_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    switch (context->rows->definition->kind) {
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES:
        return append_information_schema_tables_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLES_EXTENSIONS:
        return append_information_schema_tables_extensions_table_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS:
        return append_information_schema_columns_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_COLUMNS_EXTENSIONS:
        return append_information_schema_columns_extensions_table_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLESPACES_EXTENSIONS:
        return append_information_schema_tablespaces_extensions_table_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS:
        return append_information_schema_table_constraints_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_TABLE_CONSTRAINTS_EXTENSIONS:
        return append_information_schema_table_constraints_extensions_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FIELDS:
        return append_information_schema_innodb_fields_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN:
        return append_information_schema_innodb_foreign_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_FOREIGN_COLS:
        return append_information_schema_innodb_foreign_cols_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_COLUMNS:
        return append_information_schema_innodb_columns_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_INDEXES:
        return append_information_schema_innodb_indexes_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLES:
        return append_information_schema_innodb_tables_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_TABLESTATS:
        return append_information_schema_innodb_tablestats_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_INNODB_VIRTUAL:
        return append_information_schema_innodb_virtual_base_rows(
            context->database,
            context->rows,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_CHECK_CONSTRAINTS:
        return append_information_schema_check_constraints_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_KEY_COLUMN_USAGE:
        return append_information_schema_key_column_usage_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS:
        return append_information_schema_statistics_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_PARTITIONS:
        return append_information_schema_partitions_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_REFERENTIAL_CONSTRAINTS:
        return append_information_schema_referential_constraints_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    case MYLITE_EXECUTION_CATALOG_TABLE_ST_GEOMETRY_COLUMNS:
        return append_information_schema_st_geometry_columns_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    default:
        return MYLITE_OK;
    }
}

static int append_information_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *const *values
) {
    char ***grown_rows = NULL;
    char **row = NULL;
    size_t required_count = rows->row_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_rows)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    grown_rows = (char ***)realloc((void *)rows->rows, required_count * sizeof(*grown_rows));
    if (grown_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    rows->rows = grown_rows;

    row = (char **)calloc(rows->definition->column_count, sizeof(*row));
    if (row == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < rows->definition->column_count; ++column_index) {
        if (values[column_index] == NULL) {
            continue;
        }
        int rc = duplicate_text(database, values[column_index], &row[column_index]);

        if (rc != MYLITE_OK) {
            for (size_t rollback_index = 0U; rollback_index < column_index; ++rollback_index) {
                free(row[rollback_index]);
            }
            free((void *)row);
            return rc;
        }
    }

    rows->rows[rows->row_count] = row;
    rows->row_count = required_count;
    return MYLITE_OK;
}

static int append_information_schema_schemata_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_builtin_schema_count();
         ++index) {
        const struct mylite_execution_catalog_builtin_schema *schema =
            mylite_execution_catalog_builtin_schema_at(index);
        const char *values[information_schema_schemata_column_count] = {
            "def",
            schema->name,
            schema->default_charset,
            schema->default_collation,
            NULL,
            "NO",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_schemata_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
) {
    const char *values[information_schema_schemata_column_count] = {
        "def",
        schema->name,
        schema->default_charset,
        schema->default_collation,
        NULL,
        "NO",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_schemata_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_builtin_schema_count();
         ++index) {
        const struct mylite_execution_catalog_builtin_schema *schema =
            mylite_execution_catalog_builtin_schema_at(index);
        const char *values[information_schema_schemata_extensions_column_count] = {
            "def",
            schema->name,
            "",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_schemata_extensions_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
) {
    const char *values[information_schema_schemata_extensions_column_count] = {
        "def",
        schema->name,
        "",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_character_sets_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_character_set_count();
         ++row_index) {
        const struct mylite_execution_catalog_character_set *charset =
            mylite_execution_catalog_mysql_character_set_at(row_index);
        const char *values[information_schema_character_sets_column_count] = {
            charset->name,
            charset->default_collation,
            charset->description,
            charset->maxlen,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_collations_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *values[information_schema_collations_column_count] = {
            collation->name,
            collation->character_set,
            collation->id,
            collation->is_default,
            collation->compiled,
            collation->sortlen,
            collation->pad_attribute,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_collation_applicability_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *values[information_schema_collation_applicability_column_count] = {
            collation->name,
            collation->character_set,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_engines_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_engines_column_count] = {
        "InnoDB",
        "DEFAULT",
        "Supports transactions, row-level locking, and foreign keys",
        "YES",
        "YES",
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_plugins_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_plugins_column_count] = {
        "InnoDB",
        "8.4",
        "ACTIVE",
        "STORAGE ENGINE",
        "80409.0",
        NULL,
        NULL,
        "Oracle Corporation",
        "Supports transactions, row-level locking, and foreign keys",
        "GPL",
        "FORCE",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_keywords_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_keyword_count();
         ++row_index) {
        const struct mylite_execution_catalog_keyword *keyword =
            mylite_execution_catalog_keyword_at(row_index);
        const char *values[information_schema_keywords_column_count] = {
            keyword->word,
            keyword->reserved,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_user_privileges_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(embedded_root_global_privileges) /
                                            sizeof(embedded_root_global_privileges[0]);
         ++row_index) {
        const char *values[information_schema_user_privileges_column_count] = {
            "'root'@'%'",
            "def",
            embedded_root_global_privileges[row_index],
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_user_attributes_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_user_attributes_column_count] = {
        "root",
        "%",
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_resource_groups_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char vcpu_ids[integer_text_capacity];
    const char *user_values[information_schema_resource_groups_column_count] = {
        "USR_default",
        "USER",
        "1",
        vcpu_ids,
        "0",
    };
    const char *system_values[information_schema_resource_groups_column_count] = {
        "SYS_default",
        "SYSTEM",
        "1",
        vcpu_ids,
        "0",
    };
    int rc =
        format_information_schema_resource_group_vcpu_ids(database, vcpu_ids, sizeof(vcpu_ids));

    if (rc == MYLITE_OK) {
        rc = append_information_schema_row(database, rows, user_values);
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_row(database, rows, system_values);
    }
    return rc;
}

static int append_information_schema_st_units_of_measure_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_st_unit_of_measure_row_count();
         ++row_index) {
        const struct mylite_execution_catalog_st_unit_of_measure_row *unit =
            mylite_execution_catalog_st_unit_of_measure_row_at(row_index);
        const char *values[information_schema_st_units_of_measure_column_count] = {
            unit->unit_name,
            "LINEAR",
            unit->conversion_factor,
            "",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int format_information_schema_resource_group_vcpu_ids(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    long cpu_count = information_schema_online_processor_count();
    int written = 0;

    if (cpu_count <= 1) {
        written = snprintf(buffer, buffer_size, "0");
    } else {
        written = snprintf(buffer, buffer_size, "0-%ld", cpu_count - 1);
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format resource group vcpu ids");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static long information_schema_online_processor_count(void) {
#ifdef _WIN32
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors > 0U) {
        return (long)info.dwNumberOfProcessors;
    }
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    if (count > 0) {
        return count;
    }
#endif
    return 1;
}

static int append_information_schema_processlist_system_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
) {
    char connection_id_text[integer_text_capacity];
    char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char *info = NULL;
    const char *selected_schema = NULL;
    const char *values[information_schema_processlist_column_count] = {
        connection_id_text,
        user,
        host,
        NULL,
        "Query",
        "0",
        "executing",
        NULL,
    };
    int written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        database->session.connection_id
    );
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA.PROCESSLIST ID");
        return MYLITE_ERROR;
    }
    if (database->session.has_selected_schema) {
        selected_schema = database->session.selected_schema;
    }
    values[information_schema_processlist_db_column] = selected_schema;

    rc = format_show_processlist_user_host(database, user, sizeof(user), host, sizeof(host));
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_processlist_info(database, context, &info);
    }
    if (rc == MYLITE_OK) {
        values[information_schema_processlist_info_column] = info;
        rc = append_information_schema_row(database, rows, values);
    }

    free(info);
    return rc;
}

static int copy_information_schema_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    char **out_info
) {
    const char *sql = NULL;
    size_t sql_size = 0U;
    size_t length = 0U;
    char *info = NULL;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;
    if (context == NULL) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    sql = mylite_statement_context_sql(context);
    sql_size = mylite_statement_context_sql_size(context);
    if (sql == NULL) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    length = statement_info_length_without_terminator(sql, sql_size);
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    info = malloc(length + 1U);
    if (info == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length > 0U) {
        memcpy(info, sql, length);
    }
    info[length] = '\0';
    *out_info = info;
    return MYLITE_OK;
}

static int append_information_schema_partitions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        const struct mylite_execution_catalog_table_definition *definition =
            mylite_execution_catalog_information_schema_table_definition_at(table_index);
        const char *values[information_schema_partitions_column_count] = {
            "def",
            "information_schema",
            definition->name,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            "0",
            "0",
            "0",
            "0",
            "0",
            "0",
            NULL,
            NULL,
            NULL,
            NULL,
            "",
            "",
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_tables_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++table_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_at(table_index);

        for (size_t name_index = 0U; rc == MYLITE_OK && name_index < directory->table_count;
             ++name_index) {
            rc = append_information_schema_builtin_table_row(
                database,
                rows,
                directory,
                directory->table_names[name_index]
            );
        }
    }

    return rc;
}

static int append_information_schema_tables_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++table_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_at(table_index);

        for (size_t name_index = 0U; rc == MYLITE_OK && name_index < directory->table_count;
             ++name_index) {
            rc = append_information_schema_builtin_table_extension_row(
                database,
                rows,
                directory,
                directory->table_names[name_index]
            );
        }
    }

    return rc;
}

static int append_information_schema_builtin_table_extension_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *values[information_schema_tables_extensions_column_count] = {
        "def",
        directory->schema_name,
        table_name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tables_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    const char *values[information_schema_tables_extensions_column_count] = {
        "def",
        schema->name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tablespaces_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_tablespaces_extensions_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK &&
         index < mylite_execution_catalog_builtin_tablespace_extension_name_count();
         ++index) {
        const char *values[information_schema_tablespaces_extensions_column_count] = {
            mylite_execution_catalog_builtin_tablespace_extension_name_at(index),
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_tablespaces_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char *tablespace_name = NULL;
    const char *values[information_schema_tablespaces_extensions_column_count] = {
        NULL,
        NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_tablespaces_extensions_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS columns");
        return MYLITE_ERROR;
    }
    rc = copy_information_schema_tablespace_name(database, schema, table, &tablespace_name);
    if (rc == MYLITE_OK) {
        values[0] = tablespace_name;
        rc = append_information_schema_row(database, rows, values);
    }

    free(tablespace_name);
    return rc;
}

static int append_information_schema_files_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_files_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.FILES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_information_schema_files_row_count();
         ++index) {
        const struct mylite_execution_catalog_files_row *file =
            mylite_execution_catalog_information_schema_files_row_at(index);
        const char *values[information_schema_files_column_count] = {
            file->file_id,
            file->file_name,
            file->file_type,
            file->tablespace_name,
            "",
            NULL,
            NULL,
            NULL,
            NULL,
            "InnoDB",
            NULL,
            NULL,
            NULL,
            file->free_extents,
            file->total_extents,
            "1048576",
            file->initial_size,
            NULL,
            file->autoextend_size,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            file->data_free,
            NULL,
            NULL,
            NULL,
            NULL,
            "NORMAL",
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_datafiles_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_datafiles_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_DATAFILES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_row_at(index);
        const char *values[information_schema_innodb_datafiles_column_count] = {
            tablespace->space,
            tablespace->path,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tablespaces_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_full_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_full_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_full_row_at(index);
        const char *values[information_schema_innodb_tablespaces_column_count] = {
            tablespace->space,
            tablespace->name,
            tablespace->flag,
            tablespace->row_format,
            tablespace->page_size,
            tablespace->zip_page_size,
            tablespace->space_type,
            tablespace->fs_block_size,
            tablespace->file_size,
            tablespace->allocated_size,
            tablespace->autoextend_size,
            tablespace->server_version,
            tablespace->space_version,
            tablespace->encryption,
            tablespace->state,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_session_temp_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char connection_id_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_session_temp_tablespaces_column_count) {
        set_runtime_error(
            database,
            "invalid INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES columns"
        );
        return MYLITE_ERROR;
    }
    rc = format_uint64(
        database,
        database->session.connection_id,
        connection_id_text,
        sizeof(connection_id_text)
    );
    for (size_t index = 0U;
         rc == MYLITE_OK &&
         index < mylite_execution_catalog_innodb_session_temp_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_session_temp_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_session_temp_tablespace_row_at(index);
        const char *values[information_schema_innodb_session_temp_tablespaces_column_count] = {
            strcmp(tablespace->state, "ACTIVE") == 0 ? connection_id_text : "0",
            tablespace->space,
            tablespace->path,
            "81920",
            tablespace->state,
            tablespace->purpose,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_cmp_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_cmp_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_CMP columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_compressed_page_size_count();
         ++index) {
        const char *values[information_schema_innodb_cmp_column_count] = {
            mylite_execution_catalog_innodb_compressed_page_size_at(index),
            "0",
            "0",
            "0",
            "0",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_buffer_pool_stats_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_innodb_buffer_pool_stats_column_count] = {
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
    };

    if (rows->definition->column_count !=
        information_schema_innodb_buffer_pool_stats_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS columns");
        return MYLITE_ERROR;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_innodb_cmpmem_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_cmpmem_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_CMPMEM columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_compressed_page_size_count();
         ++index) {
        const char *values[information_schema_innodb_cmpmem_column_count] = {
            mylite_execution_catalog_innodb_compressed_page_size_at(index),
            "0",
            "0",
            "0",
            "0",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_ft_default_stopword_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_ft_default_stopword_column_count) {
        set_runtime_error(
            database,
            "invalid INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD columns"
        );
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_ft_default_stopword_count();
         ++index) {
        const char *values[information_schema_innodb_ft_default_stopword_column_count] = {
            mylite_execution_catalog_innodb_ft_default_stopword_at(index),
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_tablespaces_brief_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_tablespaces_brief_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_row_at(index);
        const char *values[information_schema_innodb_tablespaces_brief_column_count] = {
            tablespace->space,
            tablespace->name,
            tablespace->path,
            tablespace->flag,
            tablespace->space_type,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int copy_information_schema_tablespace_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
) {
    size_t schema_length = strlen(schema->name);
    size_t table_length = strlen(table->name);
    size_t required_length = 0U;
    char *name = NULL;

    *out_name = NULL;
    if (schema_length > SIZE_MAX - table_length - tablespace_name_separator_size -
                            tablespace_name_terminator_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    required_length = schema_length + table_length + tablespace_name_separator_size +
                      tablespace_name_terminator_size;
    name = (char *)malloc(required_length);
    if (name == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(name, schema->name, schema_length);
    name[schema_length] = '/';
    memcpy(&name[schema_length + tablespace_name_separator_size], table->name, table_length);
    name[required_length - tablespace_name_terminator_size] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int append_information_schema_builtin_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    struct table_status_values status = {0};
    const char *table_type = builtin_schema_table_type(directory, table_name);
    int rc = load_builtin_table_status_values(database, directory, table_name, &status);

    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *values[information_schema_tables_column_count] = {
        "def",
        directory->schema_name,
        table_name,
        table_type,
        builtin_schema_table_engine(directory, table_name),
        builtin_schema_table_version(directory, table_name),
        builtin_schema_table_row_format(directory, table_name),
        builtin_schema_table_rows(directory, table_name),
        builtin_schema_table_average_row_length(directory, table_name),
        builtin_schema_table_data_length(directory, table_name),
        table_type == NULL || strcmp(table_type, "VIEW") == 0 ? NULL : "0",
        builtin_schema_table_index_length(directory, table_name),
        builtin_schema_table_data_free(directory, table_name),
        builtin_schema_table_auto_increment(directory, table_name),
        status.create_time,
        status.update_time,
        NULL,
        builtin_schema_table_collation(directory, table_name),
        NULL,
        builtin_schema_table_create_options(directory, table_name),
        builtin_schema_table_comment(directory, table_name),
    };

    return append_information_schema_row(database, rows, values);
}

static int load_builtin_table_status_values(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct table_status_values *status
) {
    int rc = format_builtin_table_status_timestamp(database, status);

    if (rc == MYLITE_OK && builtin_schema_table_has_update_time(directory, table_name)) {
        memcpy(
            status->update_time_text,
            status->create_time_text,
            sizeof(status->update_time_text)
        );
        status->update_time = status->update_time_text;
    }
    return rc;
}

static const struct mylite_execution_catalog_builtin_schema_table_directory *find_builtin_schema_table_directory(
    const char *schema_name
) {
    return mylite_execution_catalog_builtin_schema_table_directory_by_name(schema_name);
}

static const char *builtin_schema_table_type(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return "SYSTEM VIEW";
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") != 0) {
        return "VIEW";
    }
    return "BASE TABLE";
}

static const char *builtin_schema_table_engine(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return strcmp(table_name, "sys_config") == 0 ? "InnoDB" : NULL;
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        return "PERFORMANCE_SCHEMA";
    }
    if (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0) {
        return "CSV";
    }
    return "InnoDB";
}

static const char *builtin_schema_table_version(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    return "10";
}

static const char *builtin_schema_table_row_format(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return strcmp(table_name, "sys_config") == 0 ? "Dynamic" : NULL;
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        if (strcmp(table_name, "accounts") == 0 ||
            strcmp(table_name, "events_statements_histogram_global") == 0 ||
            strcmp(table_name, "hosts") == 0 || strcmp(table_name, "performance_timers") == 0 ||
            strcmp(table_name, "replication_applier_status") == 0 ||
            strcmp(table_name, "replication_asynchronous_connection_failover") == 0 ||
            strcmp(table_name, "replication_group_members") == 0 ||
            strcmp(table_name, "setup_actors") == 0 || strcmp(table_name, "users") == 0) {
            return "Fixed";
        }
    }
    return "Dynamic";
}

static const char *builtin_schema_table_rows(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        const char *rows = builtin_mysql_table_rows(table_name);

        if (rows != NULL) {
            return rows;
        }
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 &&
        strcmp(table_name, "setup_actors") == 0) {
        return "128";
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") == 0) {
        return "6";
    }
    return "0";
}

static const char *builtin_mysql_table_rows(const char *table_name) {
    static const struct mysql_table_row_count {
        const char *table_name;
        const char *rows;
    } row_counts[] = {
        {"db", "2"},
        {"engine_cost", "2"},
        {"general_log", "2"},
        {"global_grants", "103"},
        {"help_category", "53"},
        {"help_keyword", "1142"},
        {"help_relation", "1608"},
        {"help_topic", "902"},
        {"innodb_index_stats", "6"},
        {"innodb_table_stats", "2"},
        {"plugin", "2"},
        {"proxies_priv", "1"},
        {"server_cost", "6"},
        {"slow_log", "2"},
        {"tables_priv", "2"},
        {"time_zone", "1457"},
        {"time_zone_leap_second", "0"},
        {"time_zone_name", "1712"},
        {"time_zone_transition", "119074"},
        {"time_zone_transition_type", "10529"},
        {"user", "5"},
    };

    for (size_t index = 0U; index < sizeof(row_counts) / sizeof(row_counts[0]); ++index) {
        if (strcmp(table_name, row_counts[index].table_name) == 0) {
            return row_counts[index].rows;
        }
    }
    return NULL;
}

static const char *builtin_schema_table_average_row_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (builtin_schema_table_is_mysql_stats(directory, table_name)) {
        return strcmp(table_name, "innodb_index_stats") == 0 ? "2730" : "8192";
    }
    if (builtin_schema_table_is_mysql_cost(directory, table_name)) {
        return strcmp(table_name, "server_cost") == 0 ? "2730" : "8192";
    }
    if (builtin_schema_table_is_mysql_plugin(directory, table_name)) {
        return "8192";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 &&
        (strcmp(table_name, "db") == 0 || strcmp(table_name, "tables_priv") == 0)) {
        return "8192";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "proxies_priv") == 0) {
        return "16384";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "global_grants") == 0) {
        return "795";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "user") == 0) {
        return "3276";
    }
    if (builtin_schema_table_is_mysql_help(directory, table_name)) {
        return builtin_schema_mysql_help_average_row_length(table_name);
    }
    if (builtin_schema_table_is_mysql_time_zone(directory, table_name)) {
        return builtin_schema_mysql_time_zone_average_row_length(table_name);
    }
    if (strcmp(directory->schema_name, "sys") == 0 && strcmp(table_name, "sys_config") == 0) {
        return "2730";
    }
    return "0";
}

static const char *builtin_schema_mysql_help_average_row_length(const char *table_name) {
    if (strcmp(table_name, "help_category") == 0) {
        return "309";
    }
    if (strcmp(table_name, "help_keyword") == 0) {
        return "114";
    }
    if (strcmp(table_name, "help_relation") == 0) {
        return "50";
    }
    return "1761";
}

static const char *builtin_schema_mysql_time_zone_average_row_length(const char *table_name) {
    if (strcmp(table_name, "time_zone") == 0) {
        return "56";
    }
    if (strcmp(table_name, "time_zone_name") == 0) {
        return "153";
    }
    if (strcmp(table_name, "time_zone_transition") == 0) {
        return "39";
    }
    if (strcmp(table_name, "time_zone_transition_type") == 0) {
        return "45";
    }
    return "0";
}

static const char *builtin_schema_table_data_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0 &&
        (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0)) {
        return "0";
    }
    if (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "global_grants") == 0) {
        return "81920";
    }
    if (builtin_schema_table_is_mysql_time_zone(directory, table_name)) {
        if (strcmp(table_name, "time_zone") == 0) {
            return "81920";
        }
        if (strcmp(table_name, "time_zone_name") == 0) {
            return "262144";
        }
        if (strcmp(table_name, "time_zone_transition") == 0) {
            return "4734976";
        }
        if (strcmp(table_name, "time_zone_transition_type") == 0) {
            return "475136";
        }
    }
    if (builtin_schema_table_is_mysql_help(directory, table_name)) {
        if (strcmp(table_name, "help_category") == 0) {
            return "16384";
        }
        if (strcmp(table_name, "help_keyword") == 0) {
            return "131072";
        }
        if (strcmp(table_name, "help_relation") == 0) {
            return "81920";
        }
        return "1589248";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0 ||
        strcmp(directory->schema_name, "information_schema") == 0) {
        return "0";
    }
    return "16384";
}

static const char *builtin_schema_table_index_length(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    if (!builtin_schema_table_is_mysql_help(directory, table_name)) {
        return strcmp(directory->schema_name, "mysql") == 0 &&
                       (strcmp(table_name, "db") == 0 || strcmp(table_name, "procs_priv") == 0 ||
                        strcmp(table_name, "proxies_priv") == 0 ||
                        strcmp(table_name, "tables_priv") == 0)
                   ? "16384"
                   : "0";
    }
    if (strcmp(table_name, "help_category") == 0) {
        return "16384";
    }
    if (strcmp(table_name, "help_keyword") == 0) {
        return "147456";
    }
    if (strcmp(table_name, "help_topic") == 0) {
        return "98304";
    }
    return "0";
}

static const char *builtin_schema_table_data_free(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0) {
        return NULL;
    }
    return builtin_schema_table_is_mysql_stats(directory, table_name) ||
                   builtin_schema_table_is_mysql_cost(directory, table_name) ||
                   builtin_schema_table_is_mysql_component(directory, table_name) ||
                   builtin_schema_table_is_mysql_func(directory, table_name) ||
                   builtin_schema_table_is_mysql_gtid_executed(directory, table_name) ||
                   builtin_schema_table_is_mysql_help(directory, table_name) ||
                   builtin_schema_table_is_mysql_ndb_binlog_index(directory, table_name) ||
                   builtin_schema_table_is_mysql_plugin(directory, table_name) ||
                   builtin_schema_table_is_mysql_replication_metadata(directory, table_name) ||
                   builtin_schema_table_is_mysql_servers(directory, table_name) ||
                   builtin_schema_table_is_mysql_time_zone(directory, table_name) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "columns_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "default_roles") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "password_history") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "role_edges") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "procs_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "proxies_priv") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    (strcmp(table_name, "db") == 0 || strcmp(table_name, "tables_priv") == 0)) ||
                   (strcmp(directory->schema_name, "mysql") == 0 &&
                    strcmp(table_name, "global_grants") == 0) ||
                   (strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "user") == 0)
               ? "4194304"
               : "0";
}

static const char *builtin_schema_table_auto_increment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (builtin_schema_table_is_mysql_component(directory, table_name)) {
        return "1";
    }
    return builtin_schema_table_is_mysql_time_zone(directory, table_name) &&
                   strcmp(table_name, "time_zone") == 0
               ? "1796"
               : NULL;
}

static const char *builtin_schema_table_collation(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *table_type = builtin_schema_table_type(directory, table_name);

    if (table_type == NULL || strcmp(table_type, "VIEW") == 0 ||
        strcmp(directory->schema_name, "information_schema") == 0) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "sys") == 0) {
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(directory->schema_name, "performance_schema") == 0) {
        if (strcmp(table_name, "keyring_keys") == 0 ||
            strcmp(table_name, "session_account_connect_attrs") == 0 ||
            strcmp(table_name, "session_connect_attrs") == 0) {
            return "utf8mb4_bin";
        }
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(table_name, "ndb_binlog_index") == 0) {
        return "latin1_swedish_ci";
    }
    if (strcmp(table_name, "gtid_executed") == 0 ||
        strcmp(table_name, "replication_group_configuration_version") == 0 ||
        strcmp(table_name, "replication_group_member_actions") == 0) {
        return "utf8mb4_0900_ai_ci";
    }
    if (strcmp(table_name, "columns_priv") == 0 || strcmp(table_name, "db") == 0 ||
        strcmp(table_name, "default_roles") == 0 || strcmp(table_name, "func") == 0 ||
        strcmp(table_name, "global_grants") == 0 || strcmp(table_name, "innodb_index_stats") == 0 ||
        strcmp(table_name, "innodb_table_stats") == 0 ||
        strcmp(table_name, "password_history") == 0 || strcmp(table_name, "procs_priv") == 0 ||
        strcmp(table_name, "proxies_priv") == 0 || strcmp(table_name, "role_edges") == 0 ||
        strcmp(table_name, "tables_priv") == 0 || strcmp(table_name, "user") == 0) {
        return "utf8mb3_bin";
    }
    return "utf8mb3_general_ci";
}

static const char *builtin_schema_table_create_options(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        if (strcmp(table_name, "general_log") == 0 || strcmp(table_name, "slow_log") == 0) {
            return "";
        }
        if (strcmp(table_name, "component") == 0) {
            return "row_format=DYNAMIC";
        }
        return "row_format=DYNAMIC stats_persistent=0";
    }
    return strcmp(builtin_schema_table_type(directory, table_name), "VIEW") == 0 ? NULL : "";
}

static const char *builtin_schema_table_comment(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    if (directory == NULL || table_name == NULL) {
        return NULL;
    }
    if (strcmp(builtin_schema_table_type(directory, table_name), "VIEW") == 0) {
        return "VIEW";
    }
    if (strcmp(directory->schema_name, "mysql") == 0) {
        const char *comment = builtin_mysql_table_comment(table_name);

        if (comment != NULL) {
            return comment;
        }
    }
    return "";
}

static const char *builtin_mysql_table_comment(const char *table_name) {
    static const struct mysql_table_comment {
        const char *table_name;
        const char *comment;
    } comments[] = {
        {"columns_priv", "Column privileges"},
        {"component", "Components"},
        {"db", "Database privileges"},
        {"default_roles", "Default roles"},
        {"func", "User defined functions"},
        {"general_log", "General log"},
        {"global_grants", "Extended global grants"},
        {"help_category", "help categories"},
        {"help_keyword", "help keywords"},
        {"help_relation", "keyword-topic relation"},
        {"help_topic", "help topics"},
        {"password_history", "Password history for user accounts"},
        {"plugin", "MySQL plugins"},
        {"procs_priv", "Procedure privileges"},
        {"proxies_priv", "User proxy privileges"},
        {"role_edges", "Role hierarchy and role grants"},
        {"servers", "MySQL Foreign Servers table"},
        {"slave_master_info", "Master Information"},
        {"slave_relay_log_info", "Relay Log Information"},
        {"slave_worker_info", "Worker Information"},
        {"slow_log", "Slow log"},
        {"tables_priv", "Table privileges"},
        {"time_zone", "Time zones"},
        {"time_zone_leap_second", "Leap seconds information for time zones"},
        {"time_zone_name", "Time zone names"},
        {"time_zone_transition", "Time zone transitions"},
        {"time_zone_transition_type", "Time zone transition types"},
        {"user", "Users and global privileges"},
    };

    for (size_t index = 0U; index < sizeof(comments) / sizeof(comments[0]); ++index) {
        if (strcmp(table_name, comments[index].table_name) == 0) {
            return comments[index].comment;
        }
    }
    return NULL;
}

static bool builtin_schema_table_has_update_time(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return builtin_schema_table_is_mysql_stats(directory, table_name) ||
           builtin_schema_table_is_mysql_plugin(directory, table_name);
}

static bool builtin_schema_table_is_mysql_stats(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "innodb_index_stats") == 0 ||
            strcmp(table_name, "innodb_table_stats") == 0);
}

static bool builtin_schema_table_is_mysql_cost(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "engine_cost") == 0 || strcmp(table_name, "server_cost") == 0);
}

static bool builtin_schema_table_is_mysql_component(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "component") == 0;
}

static bool builtin_schema_table_is_mysql_func(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "func") == 0;
}

static bool builtin_schema_table_is_mysql_gtid_executed(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "gtid_executed") == 0;
}

static bool builtin_schema_table_is_mysql_help(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "help_category") == 0 || strcmp(table_name, "help_keyword") == 0 ||
            strcmp(table_name, "help_relation") == 0 || strcmp(table_name, "help_topic") == 0);
}

static bool builtin_schema_table_is_mysql_ndb_binlog_index(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           strcmp(table_name, "ndb_binlog_index") == 0;
}

static bool builtin_schema_table_is_mysql_replication_metadata(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "slave_master_info") == 0 ||
            strcmp(table_name, "slave_relay_log_info") == 0 ||
            strcmp(table_name, "slave_worker_info") == 0);
}

static bool builtin_schema_table_is_mysql_plugin(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "plugin") == 0;
}

static bool builtin_schema_table_is_mysql_servers(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 && strcmp(table_name, "servers") == 0;
}

static bool builtin_schema_table_is_mysql_time_zone(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    return directory != NULL && table_name != NULL &&
           strcmp(directory->schema_name, "mysql") == 0 &&
           (strcmp(table_name, "time_zone") == 0 ||
            strcmp(table_name, "time_zone_leap_second") == 0 ||
            strcmp(table_name, "time_zone_name") == 0 ||
            strcmp(table_name, "time_zone_transition") == 0 ||
            strcmp(table_name, "time_zone_transition_type") == 0);
}

static int append_information_schema_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = load_table_status_values(database, table, &status);

    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *values[information_schema_tables_column_count] = {
        "def",
        schema->name,
        table->name,
        "BASE TABLE",
        "InnoDB",
        "10",
        status.row_format,
        status.row_count_text,
        status.average_row_length_text,
        "16384",
        "0",
        status.index_length,
        "0",
        status.auto_increment,
        status.create_time,
        status.update_time,
        NULL,
        table->default_collation,
        NULL,
        status.create_options,
        table->comment,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tables_view_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = format_table_status_timestamp(
        database,
        table->created_time_utc_epoch,
        status.create_time_text,
        sizeof(status.create_time_text),
        &status.create_time
    );
    const char *values[information_schema_tables_column_count] = {
        "def",  schema->name, table->name, "VIEW", NULL,
        NULL,   NULL,         NULL,        NULL,   NULL,
        NULL,   NULL,         NULL,        NULL,   status.create_time,
        NULL,   NULL,         NULL,        NULL,   NULL,
        "VIEW",
    };

    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_partitions_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = load_table_status_values(database, table, &status);
    const char *values[information_schema_partitions_column_count] = {
        "def",
        schema->name,
        table->name,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        status.row_count_text,
        status.average_row_length_text,
        "16384",
        "0",
        status.index_length,
        "0",
        status.create_time,
        status.update_time,
        NULL,
        NULL,
        "",
        "",
        NULL,
    };

    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_information_schema_row(database, rows, values);
}

static int load_table_status_values(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    int64_t row_count = 0;
    int64_t average_row_length = 0;
    int64_t index_length = 0;
    bool has_auto_increment = false;
    int rc = MYLITE_OK;

    if (out_values == NULL) {
        return MYLITE_MISUSE;
    }
    *out_values = (struct table_status_values){0};

    rc = read_show_table_status_row_count(database, table, &row_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read table status row count");
        return MYLITE_ERROR;
    }
    if (row_count > 0) {
        average_row_length = show_table_status_data_length / row_count;
    }
    rc = format_show_table_status_integer(
        database,
        row_count,
        out_values->row_count_text,
        sizeof(out_values->row_count_text)
    );
    if (rc == MYLITE_OK) {
        rc = format_show_table_status_integer(
            database,
            average_row_length,
            out_values->average_row_length_text,
            sizeof(out_values->average_row_length_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = table_status_index_length(database, table, &index_length);
    }
    if (rc == MYLITE_OK) {
        rc = format_show_table_status_integer(
            database,
            index_length,
            out_values->index_length_text,
            sizeof(out_values->index_length_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_values->index_length = out_values->index_length_text;
        rc = table_status_has_auto_increment(database, table, &has_auto_increment);
    }
    if (rc == MYLITE_OK && has_auto_increment) {
        rc = format_show_table_status_integer(
            database,
            table->auto_increment_next,
            out_values->auto_increment_text,
            sizeof(out_values->auto_increment_text)
        );
        if (rc == MYLITE_OK) {
            out_values->auto_increment = out_values->auto_increment_text;
        }
    }
    if (rc == MYLITE_OK) {
        rc = format_table_status_timestamp(
            database,
            table->created_time_utc_epoch,
            out_values->create_time_text,
            sizeof(out_values->create_time_text),
            &out_values->create_time
        );
    }
    if (rc == MYLITE_OK) {
        rc = format_table_status_timestamp(
            database,
            table->updated_time_utc_epoch,
            out_values->update_time_text,
            sizeof(out_values->update_time_text),
            &out_values->update_time
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_status_row_format(database, table, out_values);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_status_create_options(database, table, out_values);
    }

    return rc;
}

static int load_table_status_row_format(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    const char *row_format = "Dynamic";

    if (table == NULL || out_values == NULL) {
        return MYLITE_MISUSE;
    }
    if (table_status_row_format_is_compressed(table)) {
        row_format = "Compressed";
    } else if (text_equals_ascii_case_insensitive(table->row_format_option, "COMPACT")) {
        row_format = "Compact";
    } else if (text_equals_ascii_case_insensitive(table->row_format_option, "REDUNDANT")) {
        row_format = "Redundant";
    } else if (
        table->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(table->row_format_option, "DYNAMIC")
    ) {
        set_runtime_error(database, "invalid table row format descriptor");
        return MYLITE_ERROR;
    }
    snprintf(out_values->row_format_text, sizeof(out_values->row_format_text), "%s", row_format);
    out_values->row_format = out_values->row_format_text;
    return MYLITE_OK;
}

static bool table_status_row_format_is_compressed(
    const struct mylite_catalog_table_descriptor *table
) {
    if (table == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(table->row_format_option, "COMPRESSED")) {
        return true;
    }
    if (table->row_format_option[0] != '\0') {
        return false;
    }
    if (table->key_block_size > 0) {
        return true;
    }
    return false;
}

static int load_table_status_create_options(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    bool has_option = false;
    int rc = MYLITE_OK;

    if (table == NULL || out_values == NULL) {
        return MYLITE_MISUSE;
    }
    out_values->create_options_text[0] = '\0';
    out_values->create_options = out_values->create_options_text;

    if (table->row_format_option[0] != '\0') {
        char text[sizeof("row_format=") + MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int written = snprintf(text, sizeof(text), "row_format=%s", table->row_format_option);

        if (written < 0 || (size_t)written >= sizeof(text)) {
            set_runtime_error(database, "failed to format table create option");
            return MYLITE_ERROR;
        }
        rc = append_table_status_create_option(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            text
        );
    }
    if (rc == MYLITE_OK && table->stats_sample_pages > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_sample_pages=",
            table->stats_sample_pages
        );
    }
    if (rc == MYLITE_OK && table->stats_auto_recalc != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_auto_recalc=",
            table->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK && table->key_block_size > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "KEY_BLOCK_SIZE=",
            table->key_block_size
        );
    }
    if (rc == MYLITE_OK && table->stats_persistent != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_persistent=",
            table->stats_persistent
        );
    }
    if (rc == MYLITE_OK && table->pack_keys != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "pack_keys=",
            table->pack_keys
        );
    }
    if (rc == MYLITE_OK && table->checksum != 0) {
        rc = append_table_status_create_option(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "checksum=1"
        );
    }
    return rc;
}

static int append_table_status_create_option(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *text
) {
    size_t current_length = 0U;
    size_t separator_length = 0U;
    size_t text_length = 0U;

    if (buffer == NULL || buffer_size == 0U || has_option == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    current_length = strlen(buffer);
    text_length = strlen(text);
    if (*has_option) {
        separator_length = 1U;
    }
    if (current_length + separator_length + text_length >= buffer_size) {
        set_runtime_error(database, "failed to format table create options");
        return MYLITE_ERROR;
    }
    if (*has_option) {
        buffer[current_length] = ' ';
        ++current_length;
    }
    memcpy(buffer + current_length, text, text_length + 1U);
    *has_option = true;
    return MYLITE_OK;
}

static int append_table_status_create_option_i64(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *prefix,
    int64_t value
) {
    char text[integer_text_capacity + sizeof("stats_sample_pages=")];
    int written = snprintf(text, sizeof(text), "%s%" PRId64, prefix, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        set_runtime_error(database, "failed to format table create option");
        return MYLITE_ERROR;
    }
    return append_table_status_create_option(database, buffer, buffer_size, has_option, text);
}

static int table_status_has_auto_increment(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_has_auto_increment
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (out_has_auto_increment == NULL) {
        return MYLITE_MISUSE;
    }
    *out_has_auto_increment = false;

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (column_descriptor_is_auto_increment(&columns[column_index])) {
            *out_has_auto_increment = true;
            break;
        }
    }
    free(columns);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read table status columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int table_status_index_length(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_index_length
) {
    struct nonprimary_index_presence_context context = {.has_nonprimary_index = false};
    int rc = MYLITE_OK;

    if (out_index_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index_length = 0;
    rc = mylite_catalog_for_each_index_in_table(
        database,
        table->table_id,
        note_nonprimary_index_presence,
        &context
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read table status index descriptors");
        return MYLITE_ERROR;
    }
    if (context.has_nonprimary_index) {
        *out_index_length = show_table_status_data_length;
    }
    return MYLITE_OK;
}

static int note_nonprimary_index_presence(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct nonprimary_index_presence_context *context = user_data;

    if (context == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        context->has_nonprimary_index = true;
    }

    return MYLITE_OK;
}

static int format_table_status_timestamp(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size,
    const char **out_timestamp
) {
    if (out_timestamp == NULL) {
        return MYLITE_MISUSE;
    }
    *out_timestamp = NULL;
    if (epoch <= 0) {
        return MYLITE_OK;
    }
    int rc = format_session_timestamp_epoch_text(database, epoch, buffer, buffer_size);

    if (rc == MYLITE_OK) {
        *out_timestamp = buffer;
    }
    return rc;
}

static int append_information_schema_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_information_schema_table_definition_at(table_index)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_columns_mysql_system_rows(database, rows);
    }
    return rc;
}

static int append_information_schema_columns_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < definition->column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->columns[column_index];
        char ordinal_text[integer_text_capacity];
        const char *values[information_schema_columns_column_count] = {
            "def",
            "information_schema",
            definition->name,
            column->name,
            ordinal_text,
            column->column_default,
            column->is_nullable,
            column->data_type,
            column->character_maximum_length,
            column->character_octet_length,
            column->numeric_precision,
            column->numeric_scale,
            column->datetime_precision,
            column->character_set_name,
            column->collation_name,
            column->column_type,
            "",
            "",
            "select",
            "",
            "",
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)column_index + 1,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_information_schema_columns_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(table_index)
        );
    }
    return rc;
}

static int append_information_schema_columns_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < definition->query_definition.column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[column_index];
        char ordinal_text[integer_text_capacity];
        const char *values[information_schema_columns_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            ordinal_text,
            column->column_default,
            column->is_nullable,
            column->data_type,
            column->character_maximum_length,
            column->character_octet_length,
            column->numeric_precision,
            column->numeric_scale,
            column->datetime_precision,
            column->character_set_name,
            column->collation_name,
            column->column_type,
            definition->column_keys[column_index],
            definition->column_extras[column_index],
            definition->column_privileges[column_index],
            mysql_system_table_column_comment(definition, column_index),
            mysql_system_table_column_generation_expression(definition, column_index),
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)column_index + 1,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_information_schema_columns_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_extensions_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_information_schema_table_definition_at(table_index)
        );
    }
    return rc;
}

static int append_information_schema_columns_extensions_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < definition->column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->columns[column_index];
        const char *values[information_schema_columns_extensions_column_count] = {
            "def",
            "information_schema",
            definition->name,
            column->name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    struct primary_key_info primary_key = primary_key_info_init();
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(database, table->table_id, columns, column_count, &primary_key);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_columns_base_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index],
            &primary_key,
            indexes,
            index_count
        );
    }
    loaded_index_infos_deinit(&indexes, &index_count);
    primary_key_info_deinit(&primary_key);
    free(columns);
    return rc;
}

static int append_information_schema_st_geometry_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_st_geometry_columns_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index]
        );
    }
    free(columns);
    return rc;
}

static int append_information_schema_columns_extensions_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_columns_extensions_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index]
        );
    }
    free(columns);
    return rc;
}

static int append_information_schema_columns_view_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    struct primary_key_info primary_key = primary_key_info_init();
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = append_information_schema_columns_base_column_row(
            database,
            rows,
            schema,
            table,
            &columns[column_index],
            &primary_key,
            NULL,
            0U
        );
    }
    primary_key_info_deinit(&primary_key);
    free(columns);
    return rc;
}

static int append_information_schema_columns_base_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    const struct primary_key_info *primary_key,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    char ordinal_text[integer_text_capacity];
    char default_text[generated_default_display_text_capacity];
    char character_length_text[integer_text_capacity];
    char character_octet_text[integer_text_capacity];
    char numeric_precision_text[integer_text_capacity];
    char numeric_scale_text[integer_text_capacity];
    char column_type_text[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    struct information_schema_character_metadata character_metadata = {0};
    const char *column_type = NULL;
    const char *column_default = NULL;
    const char *extra = NULL;
    const char *is_nullable = "NO";
    const char *values[information_schema_columns_column_count] = {
        "def",
        schema->name,
        table->name,
        column->name,
        ordinal_text,
        NULL,
        is_nullable,
        information_schema_data_type_for_descriptor(column),
        NULL,
        NULL,
        information_schema_numeric_precision_for_descriptor(column),
        information_schema_numeric_scale_for_descriptor(column),
        NULL,
        NULL,
        NULL,
        NULL,
        "",
        "",
        "select,insert,update,references",
        "",
        "",
        NULL,
    };
    int rc = information_schema_format_i64(
        database,
        column->ordinal_position,
        ordinal_text,
        sizeof(ordinal_text)
    );

    if (column->is_nullable) {
        is_nullable = "YES";
        values[information_schema_columns_is_nullable_column] = is_nullable;
    }
    if (rc == MYLITE_OK) {
        rc = column_default_display_text(
            database,
            column,
            default_text,
            sizeof(default_text),
            &column_default
        );
    }
    if (rc == MYLITE_OK) {
        rc = show_column_type_text(
            database,
            column->logical_type,
            column_type_text,
            sizeof(column_type_text),
            &column_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_character_metadata_for_descriptor(
            database,
            table,
            column,
            character_length_text,
            sizeof(character_length_text),
            character_octet_text,
            sizeof(character_octet_text),
            &character_metadata
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_columns_numeric_metadata(
            database,
            column,
            numeric_precision_text,
            sizeof(numeric_precision_text),
            numeric_scale_text,
            sizeof(numeric_scale_text),
            values
        );
    }
    if (rc == MYLITE_OK &&
        (column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
         column_descriptor_is_timestamp(column))) {
        values[information_schema_columns_datetime_precision_column] = "0";
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    extra = column_extra_text(column);

    values[information_schema_columns_default_column] = column_default;
    values[information_schema_columns_character_maximum_length_column] =
        character_metadata.character_maximum_length;
    values[information_schema_columns_character_octet_length_column] =
        character_metadata.character_octet_length;
    values[information_schema_columns_character_set_name_column] =
        character_metadata.character_set_name;
    values[information_schema_columns_collation_name_column] = character_metadata.collation_name;
    values[information_schema_columns_column_type_column] = column_type;
    values[information_schema_columns_column_key_column] = column_key_text(
        (struct loaded_index_info_span){.indexes = indexes, .count = index_count},
        primary_key,
        column
    );
    values[information_schema_columns_extra_column] = extra;
    values[information_schema_columns_column_comment_column] = column->comment;
    if (column->is_generated) {
        values[information_schema_columns_generation_expression_column] =
            column->generation_expression;
    }

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_st_geometry_columns_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *geometry_type_name = NULL;
    const char *values[information_schema_st_geometry_columns_column_count] = {
        "def",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };

    if (!column_descriptor_is_spatial(column)) {
        return MYLITE_OK;
    }
    if (!spatial_logical_type_display_text(column->logical_type, &geometry_type_name)) {
        set_runtime_error(database, "unsupported spatial metadata descriptor");
        return MYLITE_ERROR;
    }

    values[information_schema_st_geometry_columns_schema_column] = schema->name;
    values[information_schema_st_geometry_columns_table_column] = table->name;
    values[information_schema_st_geometry_columns_column_name_column] = column->name;
    values[information_schema_st_geometry_columns_type_name_column] = geometry_type_name;

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_columns_extensions_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    const char *values[information_schema_columns_extensions_column_count] = {
        "def",
        schema->name,
        table->name,
        column->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_views_view_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_view_descriptor view = {0};
    int rc = mylite_catalog_read_view_by_table_id(database, table->table_id, &view);
    const char *values[information_schema_views_column_count] = {
        "def",
        schema->name,
        table->name,
        view.view_definition,
        view.check_option,
        view.is_updatable,
        view.definer,
        view.security_type,
        view.character_set_client,
        view.collation_connection,
    };

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        return rc;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_view_table_usage_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_view_descriptor view = {0};
    int rc = mylite_catalog_read_view_by_table_id(database, table->table_id, &view);
    const char *values[information_schema_view_table_usage_column_count] = {
        "def",
        schema->name,
        table->name,
        "def",
        view.source_schema_name,
        view.source_table_name,
    };

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        return rc;
    }
    return append_information_schema_row(database, rows, values);
}

static int column_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    *out_default_text = NULL;
    if (column->is_generated) {
        return MYLITE_OK;
    }
    if (column->is_auto_increment) {
        return MYLITE_OK;
    }

    switch (column->default_kind) {
    case MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER:
        *out_default_text = default_text;
        return information_schema_format_i64(
            database,
            column->default_integer,
            default_text,
            default_text_size
        );
    case MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL:
    case MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION:
    case MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION:
        *out_default_text = column->default_text;
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_TEXT:
        if (column_descriptor_is_text_family(column)) {
            int rc = format_mysql_utf8mb4_expression_text(
                column->default_text,
                default_text,
                default_text_size
            );

            if (rc == MYLITE_OK) {
                *out_default_text = default_text;
            }
            return rc;
        }
        *out_default_text = column->default_text;
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION: {
        int rc = format_mysql_utf8mb4_expression_text(
            column->default_text,
            default_text,
            default_text_size
        );

        if (rc == MYLITE_OK) {
            *out_default_text = default_text;
        }
        return rc;
    }
    case MYLITE_CATALOG_COLUMN_DEFAULT_BINARY:
        if (column_descriptor_is_binary_blob_family(column)) {
            return format_binary_blob_expression_default_display_text(
                database,
                column,
                default_text,
                default_text_size,
                out_default_text
            );
        }
        return format_binary_default_display_text(
            database,
            column,
            default_text,
            default_text_size,
            out_default_text
        );
    case MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP:
        *out_default_text = "CURRENT_TIMESTAMP";
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE:
        *out_default_text = "curdate()";
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME:
        *out_default_text = "curtime()";
        return MYLITE_OK;
    case MYLITE_CATALOG_COLUMN_DEFAULT_NONE:
    case MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int format_binary_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    size_t display_byte_count = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    display_byte_count = binary_default_display_byte_count(bytes, byte_count);
    if (byte_count == 0U) {
        if (default_text_size == 0U) {
            free(bytes);
            return MYLITE_NOMEM;
        }
        default_text[0] = '\0';
    } else if (display_byte_count == 0U) {
        if (default_text_size < sizeof("0x")) {
            free(bytes);
            return MYLITE_NOMEM;
        }
        memcpy(default_text, "0x", sizeof("0x"));
    } else {
        char *hex_text = NULL;
        size_t hex_length = 0U;

        rc =
            format_hex_bytes(database, (const unsigned char *)bytes, display_byte_count, &hex_text);
        if (rc == MYLITE_OK) {
            hex_length = strlen(hex_text);
            if ((hex_length + sizeof("0x")) > default_text_size) {
                rc = MYLITE_NOMEM;
            } else {
                default_text[0] = '0';
                default_text[1] = 'x';
                memcpy(default_text + 2U, hex_text, hex_length + 1U);
            }
        }
        free(hex_text);
    }
    if (rc == MYLITE_OK) {
        *out_default_text = default_text;
    }
    free(bytes);
    return rc;
}

static int format_binary_blob_expression_default_display_text(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *default_text,
    size_t default_text_size,
    const char **out_default_text
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    char *hex_text = NULL;
    size_t hex_length = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (byte_count == 0U) {
        if (sizeof("X''") > default_text_size) {
            rc = MYLITE_NOMEM;
        } else {
            memcpy(default_text, "X''", sizeof("X''"));
        }
    } else {
        rc = format_hex_bytes(database, (const unsigned char *)bytes, byte_count, &hex_text);
        if (rc == MYLITE_OK) {
            hex_length = strlen(hex_text);
            if ((hex_length + sizeof("0x")) > default_text_size) {
                rc = MYLITE_NOMEM;
            } else {
                default_text[0] = '0';
                default_text[1] = 'x';
                memcpy(default_text + 2U, hex_text, hex_length + 1U);
            }
        }
    }
    if (rc == MYLITE_OK) {
        *out_default_text = default_text;
    }
    free(hex_text);
    free(bytes);
    return rc;
}

static int decode_binary_default_text(
    struct mylite_db *database,
    const char *default_text,
    char **out_bytes,
    size_t *out_byte_count
) {
    size_t hex_length = default_text == NULL ? 0U : strlen(default_text);
    size_t byte_count = hex_length / 2U;
    char *bytes = NULL;

    if (default_text == NULL || out_bytes == NULL || out_byte_count == NULL ||
        (hex_length % 2U) != 0U) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_byte_count = 0U;

    bytes = malloc(byte_count + 1U);
    if (bytes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t byte_index = 0U; byte_index < byte_count; ++byte_index) {
        unsigned char high = 0U;
        unsigned char low = 0U;
        size_t hex_index = byte_index * 2U;

        if (hex_nibble_value(default_text[hex_index], &high) != MYLITE_OK ||
            hex_nibble_value(default_text[hex_index + 1U], &low) != MYLITE_OK) {
            free(bytes);
            return MYLITE_MISUSE;
        }
        bytes[byte_index] = (char)((high << byte_high_nibble_shift) | low);
    }
    bytes[byte_count] = '\0';
    *out_bytes = bytes;
    *out_byte_count = byte_count;
    return MYLITE_OK;
}

static size_t binary_default_display_byte_count(const char *bytes, size_t byte_count) {
    for (size_t index = 0U; index < byte_count; ++index) {
        if (bytes[index] == '\0') {
            return index;
        }
    }
    return byte_count;
}

static int append_mysql_utf8mb4_expression_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = mylite_dynamic_string_append(string, "_utf8mb4'");

    if (rc == MYLITE_OK) {
        rc = append_mysql_escaped_expression_text(string, text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }
    return rc;
}

static int format_mysql_utf8mb4_expression_text(
    const char *text,
    char *buffer,
    size_t buffer_size
) {
    struct mylite_dynamic_string string = {0};
    int rc = append_mysql_utf8mb4_expression_text(&string, text);

    if (rc == MYLITE_OK && (string.length + 1U) > buffer_size) {
        rc = MYLITE_NOMEM;
    }
    if (rc == MYLITE_OK) {
        memcpy(buffer, string.text, string.length + 1U);
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_mysql_escaped_expression_text(
    struct mylite_dynamic_string *string,
    const char *text
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        switch (text[index]) {
        case '\'':
            rc = mylite_dynamic_string_append(string, "\\'");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        case '\t':
            rc = mylite_dynamic_string_append(string, "\\t");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }

    return rc;
}

static const char *column_extra_text(const struct mylite_catalog_column_descriptor *column) {
    bool generated = column_default_is_generated_extra(column);

    if (column->is_generated && column->generated_kind == MYLITE_CATALOG_GENERATED_COLUMN_STORED &&
        !column->is_visible) {
        return "STORED GENERATED INVISIBLE";
    }
    if (column->is_generated && column->generated_kind == MYLITE_CATALOG_GENERATED_COLUMN_STORED) {
        return "STORED GENERATED";
    }
    if (column->is_generated && !column->is_visible) {
        return "VIRTUAL GENERATED INVISIBLE";
    }
    if (column->is_generated) {
        return "VIRTUAL GENERATED";
    }
    if (column->is_auto_increment && !column->is_visible) {
        return "auto_increment INVISIBLE";
    }
    if (column->is_auto_increment) {
        return "auto_increment";
    }
    if (generated && column->on_update_current_timestamp && !column->is_visible) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (generated && column->on_update_current_timestamp) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP";
    }
    if (generated && !column->is_visible) {
        return "DEFAULT_GENERATED INVISIBLE";
    }
    if (generated) {
        return "DEFAULT_GENERATED";
    }
    if (column->on_update_current_timestamp && !column->is_visible) {
        return "on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (column->on_update_current_timestamp) {
        return "on update CURRENT_TIMESTAMP";
    }
    if (!column->is_visible) {
        return "INVISIBLE";
    }
    return "";
}

static bool column_default_is_generated_extra(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column_default_kind_is_expression(column->default_kind) ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            (column_descriptor_is_text_family(column) &&
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) ||
            (column_descriptor_is_binary_blob_family(column) &&
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE ||
            column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) != 0;
}

static int append_information_schema_columns_numeric_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    char *numeric_precision_text,
    size_t numeric_precision_text_size,
    char *numeric_scale_text,
    size_t numeric_scale_text_size,
    const char **values
) {
    if (column_descriptor_is_decimal(column)) {
        struct decimal_type_info info = {0};
        int rc = decimal_type_info_for_logical_type(column->logical_type, &info);

        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                (int64_t)info.precision,
                numeric_precision_text,
                numeric_precision_text_size
            );
        }
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                (int64_t)info.scale,
                numeric_scale_text,
                numeric_scale_text_size
            );
        }
        if (rc == MYLITE_OK) {
            values[information_schema_columns_numeric_precision_column] = numeric_precision_text;
            values[information_schema_columns_numeric_scale_column] = numeric_scale_text;
        }
        return rc;
    }
    if (column_descriptor_is_bit(column)) {
        uint64_t width = 0U;
        int rc = bit_width_for_logical_type(column->logical_type, &width);

        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                (int64_t)width,
                numeric_precision_text,
                numeric_precision_text_size
            );
        }
        if (rc == MYLITE_OK) {
            values[information_schema_columns_numeric_precision_column] = numeric_precision_text;
            values[information_schema_columns_numeric_scale_column] = NULL;
        }
        return rc;
    }
    if (column_descriptor_is_approximate(column)) {
        struct approximate_type_info info = {
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = false,
        };
        int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

        if (rc != MYLITE_OK) {
            return rc;
        }
        values[information_schema_columns_numeric_precision_column] =
            info.type_class == APPROXIMATE_TYPE_FLOAT ? "12" : "22";
        values[information_schema_columns_numeric_scale_column] = NULL;
    }

    return MYLITE_OK;
}

static int append_information_schema_innodb_virtual_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t virtual_index = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_virtual_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        const struct mylite_catalog_column_descriptor *column = &columns[index];

        if (!column->is_generated ||
            column->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL) {
            continue;
        }
        rc = append_information_schema_innodb_virtual_column_rows(
            database,
            rows,
            table,
            columns,
            column_count,
            column,
            virtual_index
        );
        ++virtual_index;
    }

    free(columns);
    return rc;
}

static int append_information_schema_innodb_virtual_column_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index
) {
    bool *base_positions = NULL;
    size_t base_position_count = 0U;
    int64_t position = 0;
    int rc = MYLITE_OK;

    if (column_count == 0U) {
        return MYLITE_OK;
    }
    base_positions = calloc(column_count, sizeof(*base_positions));
    if (base_positions == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = collect_information_schema_innodb_virtual_base_positions(
        database,
        columns,
        column_count,
        column,
        base_positions,
        &base_position_count
    );
    if (rc == MYLITE_OK && base_position_count != 0U) {
        rc = information_schema_innodb_virtual_position(database, column, virtual_index, &position);
    }
    for (size_t base_position = 0U; rc == MYLITE_OK && base_position < column_count;
         ++base_position) {
        if (!base_positions[base_position]) {
            continue;
        }
        if (base_position > (size_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base position");
            rc = MYLITE_ERROR;
            break;
        }
        rc = append_information_schema_innodb_virtual_dependency_row(
            database,
            rows,
            table->table_id,
            position,
            (int64_t)base_position
        );
    }

    free(base_positions);
    return rc;
}

static int collect_information_schema_innodb_virtual_base_positions(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    bool *base_positions,
    size_t *out_base_position_count
) {
    const char *expression = column == NULL ? NULL : column->generation_expression;
    size_t expression_index = 0U;
    int rc = MYLITE_OK;

    *out_base_position_count = 0U;
    if (expression == NULL || expression[0] == '\0') {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL expression");
        return MYLITE_ERROR;
    }

    while (rc == MYLITE_OK && expression[expression_index] != '\0') {
        char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        if (expression[expression_index] != '`') {
            ++expression_index;
            continue;
        }
        rc = copy_information_schema_innodb_virtual_identifier(
            database,
            expression,
            &expression_index,
            identifier,
            sizeof(identifier)
        );
        if (rc == MYLITE_OK) {
            rc = collect_information_schema_innodb_virtual_base_identifier(
                database,
                columns,
                column_count,
                identifier,
                base_positions,
                out_base_position_count
            );
        }
    }

    return rc;
}

static int collect_information_schema_innodb_virtual_base_identifier(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    bool *base_positions,
    size_t *base_position_count
) {
    const struct mylite_catalog_column_descriptor *base_column = NULL;
    int64_t base_position = 0;
    int rc = find_information_schema_innodb_virtual_base_column(
        columns,
        column_count,
        identifier,
        &base_column
    );

    if (rc != MYLITE_OK || base_column == NULL || base_column->is_generated) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base column");
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_position(database, base_column, &base_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (base_position < 0 || (uint64_t)base_position >= column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base position");
        return MYLITE_ERROR;
    }
    if (!base_positions[base_position]) {
        base_positions[base_position] = true;
        ++(*base_position_count);
    }
    return MYLITE_OK;
}

static int copy_information_schema_innodb_virtual_identifier(
    struct mylite_db *database,
    const char *expression,
    size_t *expression_index,
    char *identifier,
    size_t identifier_size
) {
    size_t identifier_index = 0U;
    size_t index = *expression_index + 1U;

    while (expression[index] != '\0') {
        if (expression[index] == '`') {
            if (expression[index + 1U] == '`') {
                if (identifier_index + 1U >= identifier_size) {
                    set_runtime_error(
                        database,
                        "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier"
                    );
                    return MYLITE_ERROR;
                }
                identifier[identifier_index] = '`';
                ++identifier_index;
                index += 2U;
                continue;
            }
            if (identifier_index == 0U) {
                set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
                return MYLITE_ERROR;
            }
            identifier[identifier_index] = '\0';
            *expression_index = index + 1U;
            return MYLITE_OK;
        }
        if (identifier_index + 1U >= identifier_size) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
            return MYLITE_ERROR;
        }
        identifier[identifier_index] = expression[index];
        ++identifier_index;
        ++index;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
    return MYLITE_ERROR;
}

static int find_information_schema_innodb_virtual_base_column(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    const struct mylite_catalog_column_descriptor **out_column
) {
    *out_column = NULL;
    for (size_t index = 0U; index < column_count; ++index) {
        if (text_equals_ascii_case_insensitive(columns[index].name, identifier)) {
            *out_column = &columns[index];
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static int information_schema_innodb_virtual_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index,
    int64_t *out_position
) {
    const int64_t virtual_position_shift = 65536;
    int64_t column_position = 0;
    uint64_t virtual_number = (uint64_t)virtual_index + 1U;
    int rc = information_schema_innodb_column_position(database, column, &column_position);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_position < 0 ||
        virtual_number > (uint64_t)((INT64_MAX - column_position) / virtual_position_shift)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL position");
        return MYLITE_ERROR;
    }

    *out_position = (int64_t)(virtual_number * (uint64_t)virtual_position_shift) + column_position;
    return MYLITE_OK;
}

static int append_information_schema_innodb_virtual_dependency_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    int64_t table_id,
    int64_t position,
    int64_t base_position
) {
    char table_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    char base_position_text[integer_text_capacity];
    int rc =
        information_schema_format_i64(database, table_id, table_id_text, sizeof(table_id_text));

    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, position, position_text, sizeof(position_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            base_position,
            base_position_text,
            sizeof(base_position_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_virtual_column_count] = {
            table_id_text,
            position_text,
            base_position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_columns_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        rc = append_information_schema_innodb_columns_column_row(
            database,
            rows,
            table,
            &columns[index]
        );
    }

    free(columns);
    return rc;
}

static int append_information_schema_innodb_columns_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    struct information_schema_innodb_column_type_info type_info = {0};
    char table_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    char mtype_text[integer_text_capacity];
    char prtype_text[integer_text_capacity];
    char length_text[integer_text_capacity];
    char has_default_text[integer_text_capacity];
    int64_t position = 0;
    int rc = MYLITE_OK;

    rc = information_schema_innodb_column_position(database, column, &position);
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_type_info(database, table, column, &type_info);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, position, position_text, sizeof(position_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.mtype,
            mtype_text,
            sizeof(mtype_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.prtype,
            prtype_text,
            sizeof(prtype_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.length,
            length_text,
            sizeof(length_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_column_has_default,
            has_default_text,
            sizeof(has_default_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_columns_column_count] = {
            table_id_text,
            column->name,
            position_text,
            mtype_text,
            prtype_text,
            length_text,
            has_default_text,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int information_schema_innodb_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_position
) {
    if (column == NULL || out_position == NULL || column->ordinal_position <= 0) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS position");
        return MYLITE_ERROR;
    }

    *out_position = column->ordinal_position - 1;
    return MYLITE_OK;
}

static int information_schema_innodb_column_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    if (out_info == NULL || column == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column input");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(column)) {
        return information_schema_innodb_column_character_type_info(
            database,
            table,
            column,
            (struct information_schema_innodb_character_type_request){
                .mtype = information_schema_innodb_column_mtype_char,
                .mysql_type_code = information_schema_innodb_column_mysql_type_string,
            },
            out_info
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return information_schema_innodb_column_character_type_info(
            database,
            table,
            column,
            (struct information_schema_innodb_character_type_request){
                .mtype = information_schema_innodb_column_mtype_varchar,
                .mysql_type_code = information_schema_innodb_column_mysql_type_varchar,
            },
            out_info
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return information_schema_innodb_column_text_family_type_info(
            database,
            table,
            column,
            out_info
        );
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return information_schema_innodb_column_binary_type_info(database, column, out_info);
    }

    return information_schema_innodb_column_nonstring_type_info(database, column, out_info);
}

static int information_schema_innodb_column_nonstring_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    if (column_descriptor_is_json(column)) {
        int64_t prtype = 0;

        int rc = information_schema_innodb_column_collation_type_prtype(
            database,
            "utf8mb4_bin",
            information_schema_innodb_column_numeric_flag +
                information_schema_innodb_column_mysql_type_json,
            column->is_nullable,
            &prtype
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_blob,
            .prtype = prtype,
            .length = information_schema_innodb_column_blob_length_long,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_spatial(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_geometry,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_mysql_type_geometry +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = information_schema_innodb_column_blob_length_long,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_bit(column)) {
        return information_schema_innodb_column_bit_type_info(database, column, out_info);
    }
    if (column_descriptor_is_decimal(column)) {
        return information_schema_innodb_column_decimal_type_info(database, column, out_info);
    }
    if (column_descriptor_is_approximate(column)) {
        return information_schema_innodb_column_approximate_type_info(database, column, out_info);
    }
    if (column_descriptor_is_year(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_int,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_unsigned_flag +
                      information_schema_innodb_column_mysql_type_year +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = year_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_date(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_int,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_mysql_type_date +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = date_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_time(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_time +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = time_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_datetime(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_datetime +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = datetime_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_timestamp(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_timestamp +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = timestamp_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_enum(column)) {
        return information_schema_innodb_column_enum_type_info(database, column, out_info);
    }
    if (column_descriptor_is_set(column)) {
        return information_schema_innodb_column_set_type_info(database, column, out_info);
    }

    return information_schema_innodb_column_integer_type_info(database, column, out_info);
}

static int information_schema_innodb_column_character_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_character_type_request request,
    struct information_schema_innodb_column_type_info *out_info
) {
    size_t character_count = 0U;
    uint64_t length = 0U;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (request.mtype == information_schema_innodb_column_mtype_char) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline CHAR descriptors",
            &character_count
        );
    } else {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline VARCHAR descriptors",
            &character_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    length = (uint64_t)character_count * column_effective_max_bytes_per_character(table, column);
    if (length > (uint64_t)INT64_MAX) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_collation_prtype(
        database,
        column_effective_collation_name(table, column),
        request.mysql_type_code,
        column->is_nullable,
        &prtype
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = request.mtype,
        .prtype = prtype,
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_text_family_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    const struct text_family_type_info *text_info =
        text_family_type_info_for_logical_type(column->logical_type);
    int64_t length = 0;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (text_info == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline TEXT descriptors"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_blob_family_length(
        database,
        text_info->maximum_length,
        &length
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_collation_prtype(
            database,
            column_effective_collation_name(table, column),
            information_schema_innodb_column_mysql_type_blob,
            column->is_nullable,
            &prtype
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_blob,
        .prtype = prtype,
        .length = length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_binary_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *binary_info =
        binary_string_type_info_for_logical_type(column->logical_type, &storage);
    int64_t length = 0;
    int64_t mtype = information_schema_innodb_column_mtype_blob;
    int64_t mysql_type_code = information_schema_innodb_column_mysql_type_blob;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (binary_info == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline binary descriptors"
        );
        return MYLITE_ERROR;
    }
    if (binary_info->blob_family) {
        rc = information_schema_innodb_column_blob_family_length(
            database,
            binary_info->maximum_length,
            &length
        );
    } else {
        if (binary_info->maximum_length > (uint64_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
            return MYLITE_ERROR;
        }
        length = (int64_t)binary_info->maximum_length;
        if (binary_info->fixed_length) {
            mtype = information_schema_innodb_column_mtype_fixbinary;
            mysql_type_code = information_schema_innodb_column_mysql_type_string;
        } else {
            mtype = information_schema_innodb_column_mtype_binary;
            mysql_type_code = information_schema_innodb_column_mysql_type_varchar;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_binary_prtype(
            database,
            mysql_type_code,
            column->is_nullable,
            &prtype
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = mtype,
        .prtype = prtype,
        .length = length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_integer_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t length = 0U;
    int64_t mysql_type_code = 0;
    int rc = information_schema_innodb_column_integer_mysql_type(
        database,
        column->logical_type,
        &mysql_type_code
    );

    if (rc == MYLITE_OK) {
        rc = integer_row_size_bytes(column->logical_type, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline integer descriptors"
        );
        return MYLITE_ERROR;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_numeric_flag + mysql_type_code +
                  (information_schema_innodb_column_logical_type_is_unsigned(column->logical_type)
                       ? information_schema_innodb_column_unsigned_flag
                       : 0) +
                  (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_decimal_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct decimal_type_info decimal_info = {0};
    uint64_t length = 0U;
    int rc = decimal_type_info_for_logical_type(column->logical_type, &decimal_info);

    if (rc == MYLITE_OK) {
        rc = decimal_row_size_bytes(&decimal_info, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline DECIMAL descriptors"
        );
        return MYLITE_ERROR;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_fixbinary,
        .prtype = information_schema_innodb_column_temporal_decimal_flag +
                  information_schema_innodb_column_mysql_type_newdecimal +
                  (decimal_info.is_unsigned ? information_schema_innodb_column_unsigned_flag : 0) +
                  (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_approximate_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    uint64_t length = 0U;
    int64_t mtype = information_schema_innodb_column_mtype_double;
    int64_t mysql_type_code = information_schema_innodb_column_mysql_type_double;
    int rc = approximate_type_info_for_logical_type(column->logical_type, &approximate_info);

    if (rc == MYLITE_OK) {
        rc = approximate_row_size_bytes(&approximate_info, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline approximate descriptors"
        );
        return MYLITE_ERROR;
    }
    if (approximate_info.type_class == APPROXIMATE_TYPE_FLOAT) {
        mtype = information_schema_innodb_column_mtype_float;
        mysql_type_code = information_schema_innodb_column_mysql_type_float;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = mtype,
        .prtype =
            information_schema_innodb_column_numeric_flag + mysql_type_code +
            (approximate_info.is_unsigned ? information_schema_innodb_column_unsigned_flag : 0) +
            (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_bit_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t width = 0U;
    int64_t prtype = 0;
    int rc = bit_width_for_logical_type(column->logical_type, &width);

    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline BIT descriptors"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_collation_type_prtype(
        database,
        "binary",
        information_schema_innodb_column_numeric_flag +
            information_schema_innodb_column_unsigned_flag +
            information_schema_innodb_column_mysql_type_bit,
        column->is_nullable,
        &prtype
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_fixbinary,
        .prtype = prtype,
        .length = (int64_t)bit_byte_width(width),
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_enum_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct enum_type_info enum_info = {0};
    int rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline ENUM descriptors",
        &enum_info
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_unsigned_flag +
                  information_schema_innodb_column_mysql_type_string +
                  information_schema_innodb_column_not_null_prtype_flag(column),
        .length = enum_info.label_count <= enum_one_byte_label_count_max ? 1 : 2,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_set_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t length = 0U;
    int rc = set_row_size_bytes(
        database,
        column->logical_type,
        &length,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline SET descriptors"
    );

    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        if (rc == MYLITE_OK) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
            return MYLITE_ERROR;
        }
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_unsigned_flag +
                  information_schema_innodb_column_mysql_type_string +
                  information_schema_innodb_column_not_null_prtype_flag(column),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_blob_family_length(
    struct mylite_db *database,
    uint64_t maximum_length,
    int64_t *out_length
) {
    if (maximum_length == tinytext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_tiny;
        return MYLITE_OK;
    }
    if (maximum_length == text_max_length) {
        *out_length = information_schema_innodb_column_blob_length_regular;
        return MYLITE_OK;
    }
    if (maximum_length == mediumtext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_medium;
        return MYLITE_OK;
    }
    if (maximum_length == longtext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_long;
        return MYLITE_OK;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS blob length");
    return MYLITE_ERROR;
}

static int information_schema_innodb_column_collation_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
) {
    return information_schema_innodb_column_collation_type_prtype(
        database,
        collation_name,
        mysql_type_code,
        is_nullable,
        out_prtype
    );
}

static int information_schema_innodb_column_binary_prtype(
    struct mylite_db *database,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
) {
    return information_schema_innodb_column_collation_type_prtype(
        database,
        "binary",
        information_schema_innodb_column_numeric_flag + mysql_type_code,
        is_nullable,
        out_prtype
    );
}

static int information_schema_innodb_column_collation_type_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t type_flags,
    bool is_nullable,
    int64_t *out_prtype
) {
    uint32_t collation_id = result_metadata_collation_id(collation_name);
    int64_t nullability_flag = is_nullable ? 0 : information_schema_innodb_column_not_null_flag;
    int64_t collation_part = 0;

    if (out_prtype == NULL || type_flags < 0 ||
        collation_id > (uint32_t)(INT64_MAX >> information_schema_innodb_column_collation_shift)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS prtype");
        return MYLITE_ERROR;
    }

    collation_part = (int64_t)collation_id << information_schema_innodb_column_collation_shift;
    if (type_flags > INT64_MAX - nullability_flag ||
        collation_part > INT64_MAX - type_flags - nullability_flag) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS prtype");
        return MYLITE_ERROR;
    }

    *out_prtype = collation_part + type_flags + nullability_flag;
    return MYLITE_OK;
}

static int information_schema_innodb_column_integer_mysql_type(
    struct mylite_db *database,
    const char *logical_type,
    int64_t *out_type_code
) {
    if (logical_type == NULL || out_type_code == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS integer type");
        return MYLITE_ERROR;
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_tiny;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_short;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_int24;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_longlong;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_long;
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline integer descriptors"
    );
    return MYLITE_ERROR;
}

static int64_t information_schema_innodb_column_not_null_prtype_flag(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && !column->is_nullable) {
        return information_schema_innodb_column_not_null_flag;
    }
    return 0;
}

static bool information_schema_innodb_column_logical_type_is_unsigned(const char *logical_type) {
    static const char suffix[] = " UNSIGNED";
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t suffix_length = sizeof(suffix) - 1U;

    return (logical_length >= suffix_length &&
            strcmp(&logical_type[logical_length - suffix_length], suffix) == 0) != 0;
}

static int append_information_schema_innodb_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct table_status_values status = {0};
    char *table_name = NULL;
    char table_id_text[integer_text_capacity];
    char flag_text[integer_text_capacity];
    char column_count_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char zip_page_size_text[integer_text_capacity];
    char instant_cols_text[integer_text_capacity];
    char total_row_versions_text[integer_text_capacity];
    size_t column_count = 0U;
    int64_t flag = 0;
    int64_t innodb_column_count = 0;
    int64_t zip_page_size = 0;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tables_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_status_row_format(database, table, &status);
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_schema_object_name(
            database,
            schema->name,
            table->name,
            &table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_flag(database, status.row_format, &flag);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_column_count(
            database,
            column_count,
            &innodb_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_zip_page_size(
            database,
            table,
            status.row_format,
            &zip_page_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, flag, flag_text, sizeof(flag_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            innodb_column_count,
            column_count_text,
            sizeof(column_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            zip_page_size,
            zip_page_size_text,
            sizeof(zip_page_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_instant_cols,
            instant_cols_text,
            sizeof(instant_cols_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_total_row_versions,
            total_row_versions_text,
            sizeof(total_row_versions_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_tables_column_count] = {
            table_id_text,
            table_name,
            flag_text,
            column_count_text,
            space_text,
            status.row_format,
            zip_page_size_text,
            "Single",
            instant_cols_text,
            total_row_versions_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(table_name);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_tablestats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char *table_name = NULL;
    char table_id_text[integer_text_capacity];
    char row_count_text[integer_text_capacity];
    char other_index_size_text[integer_text_capacity];
    char autoinc_text[integer_text_capacity];
    int64_t row_count = 0;
    int64_t other_index_size = 0;
    int64_t autoinc = 0;
    bool has_auto_increment = false;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tablestats_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESTATS columns");
        return MYLITE_ERROR;
    }

    rc = copy_information_schema_schema_object_name(
        database,
        schema->name,
        table->name,
        &table_name
    );
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read INFORMATION_SCHEMA.INNODB_TABLESTATS rows");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_count_nonprimary_indexes(
            database,
            table->table_id,
            &other_index_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = table_status_has_auto_increment(database, table, &has_auto_increment);
    }
    if (rc == MYLITE_OK && has_auto_increment) {
        autoinc = table->auto_increment_next;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            row_count,
            row_count_text,
            sizeof(row_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            other_index_size,
            other_index_size_text,
            sizeof(other_index_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, autoinc, autoinc_text, sizeof(autoinc_text));
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_tablestats_column_count] = {
            table_id_text,
            table_name,
            "Initialized",
            row_count_text,
            "1",
            other_index_size_text,
            "0",
            autoinc_text,
            "1",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(table_name);
    return rc;
}

static int information_schema_count_nonprimary_indexes(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_count
) {
    struct nonprimary_index_count_context context = {.count = 0};
    int rc = MYLITE_OK;

    if (out_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0;

    rc = mylite_catalog_for_each_index_in_table(
        database,
        table_id,
        count_nonprimary_index,
        &context
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.INNODB_TABLESTATS indexes");
        return MYLITE_ERROR;
    }
    *out_count = context.count;
    return MYLITE_OK;
}

static int count_nonprimary_index(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct nonprimary_index_count_context *context = user_data;

    if (context == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        if (context->count == INT64_MAX) {
            return MYLITE_ERROR;
        }
        ++context->count;
    }
    return MYLITE_OK;
}

static int information_schema_innodb_table_flag(
    struct mylite_db *database,
    const char *row_format,
    int64_t *out_flag
) {
    if (text_equals_ascii_case_insensitive(row_format, "Redundant")) {
        *out_flag = information_schema_innodb_table_redundant_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Compact")) {
        *out_flag = information_schema_innodb_table_compact_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Dynamic")) {
        *out_flag = information_schema_innodb_table_dynamic_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Compressed")) {
        *out_flag = information_schema_innodb_table_compressed_flag;
        return MYLITE_OK;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES row format");
    return MYLITE_ERROR;
}

static int information_schema_innodb_table_column_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_column_count
) {
    if (column_count > (size_t)INT64_MAX - information_schema_innodb_table_hidden_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES column count");
        return MYLITE_ERROR;
    }

    *out_column_count =
        (int64_t)(column_count + information_schema_innodb_table_hidden_column_count);
    return MYLITE_OK;
}

static int information_schema_innodb_table_zip_page_size(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const char *row_format,
    int64_t *out_zip_page_size
) {
    if (!text_equals_ascii_case_insensitive(row_format, "Compressed")) {
        *out_zip_page_size = information_schema_innodb_table_default_zip_page_size;
        return MYLITE_OK;
    }
    if (table->key_block_size < 0) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES zip page size");
        return MYLITE_ERROR;
    }
    if (table->key_block_size == 0) {
        *out_zip_page_size = information_schema_innodb_table_default_compressed_zip_page_size;
        return MYLITE_OK;
    }
    if (table->key_block_size > INT64_MAX / information_schema_innodb_table_zip_page_unit) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES zip page size");
        return MYLITE_ERROR;
    }

    *out_zip_page_size = table->key_block_size * information_schema_innodb_table_zip_page_unit;
    return MYLITE_OK;
}

static int append_information_schema_innodb_indexes_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    const struct loaded_index_info *clustered_index = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_indexes_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        clustered_index = information_schema_innodb_clustered_index(indexes, index_count);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_innodb_index_row(
            database,
            rows,
            table,
            column_count,
            clustered_index,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK && clustered_index == NULL) {
        rc = append_information_schema_innodb_generated_cluster_index_row(
            database,
            rows,
            table,
            column_count
        );
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
) {
    char index_id_text[integer_text_capacity];
    char table_id_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    char field_count_text[integer_text_capacity];
    char page_no_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char merge_threshold_text[integer_text_capacity];
    int64_t type = 0;
    int64_t field_count = 0;
    int64_t page_no = information_schema_innodb_index_default_page_no;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        page_no = information_schema_innodb_index_fulltext_page_no;
    }

    rc = information_schema_format_i64(
        database,
        index->index.index_id,
        index_id_text,
        sizeof(index_id_text)
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_index_type(database, index, clustered_index, &type);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, type, type_text, sizeof(type_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_index_field_count(
            database,
            index,
            column_count,
            clustered_index,
            &field_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            field_count,
            field_count_text,
            sizeof(field_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, page_no, page_no_text, sizeof(page_no_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_merge_threshold,
            merge_threshold_text,
            sizeof(merge_threshold_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_indexes_column_count] = {
            index_id_text,
            index->index.name,
            table_id_text,
            type_text,
            field_count_text,
            page_no_text,
            space_text,
            merge_threshold_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_innodb_generated_cluster_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count
) {
    char index_id_text[integer_text_capacity];
    char table_id_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    char field_count_text[integer_text_capacity];
    char page_no_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char merge_threshold_text[integer_text_capacity];
    int64_t index_id = 0;
    int64_t field_count = 0;
    int rc =
        information_schema_innodb_generated_cluster_index_id(database, table->table_id, &index_id);

    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_generated_cluster_field_count(
            database,
            column_count,
            &field_count
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, index_id, index_id_text, sizeof(index_id_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_generated_cluster_type,
            type_text,
            sizeof(type_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            field_count,
            field_count_text,
            sizeof(field_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_default_page_no,
            page_no_text,
            sizeof(page_no_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_merge_threshold,
            merge_threshold_text,
            sizeof(merge_threshold_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_indexes_column_count] = {
            index_id_text,
            information_schema_innodb_generated_cluster_index_name,
            table_id_text,
            type_text,
            field_count_text,
            page_no_text,
            space_text,
            merge_threshold_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_innodb_fields_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_fields_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FIELDS columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_innodb_fields_index_rows(database, rows, &indexes[index]);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_fields_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        rc = append_information_schema_innodb_fields_part_row(
            database,
            rows,
            index,
            &index->parts[part_index]
        );
    }
    return rc;
}

static int append_information_schema_innodb_fields_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part
) {
    char index_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (part->index_column.ordinal_position < information_schema_innodb_field_position_offset) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FIELDS position");
        return MYLITE_ERROR;
    }

    rc = information_schema_format_i64(
        database,
        index->index.index_id,
        index_id_text,
        sizeof(index_id_text)
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position - information_schema_innodb_field_position_offset,
            position_text,
            sizeof(position_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_fields_column_count] = {
            index_id_text,
            part->column.name,
            position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int information_schema_innodb_index_type(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    int64_t *out_type
) {
    switch (index->index.kind) {
    case MYLITE_CATALOG_INDEX_KIND_PRIMARY:
        *out_type = information_schema_innodb_index_primary_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_SECONDARY:
        if (index == clustered_index) {
            *out_type = information_schema_innodb_index_primary_type;
        } else {
            *out_type = index->index.is_unique ? information_schema_innodb_index_unique_type
                                               : information_schema_innodb_index_nonunique_type;
        }
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_FULLTEXT:
        *out_type = information_schema_innodb_index_fulltext_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_SPATIAL:
        *out_type = information_schema_innodb_index_spatial_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_INVALID:
        break;
    }

    set_runtime_error(database, "unknown INFORMATION_SCHEMA.INNODB_INDEXES index type");
    return MYLITE_ERROR;
}

static int information_schema_innodb_index_field_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    int64_t *out_field_count
) {
    size_t clustered_part_count = 0U;
    size_t field_count = index->part_count;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        if (index->part_count > (size_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        *out_field_count = (int64_t)index->part_count;
        return MYLITE_OK;
    }
    if (index == clustered_index) {
        if (column_count >
            (size_t)INT64_MAX - information_schema_innodb_cluster_system_field_count) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        *out_field_count =
            (int64_t)(column_count + information_schema_innodb_cluster_system_field_count);
        return MYLITE_OK;
    }

    rc = information_schema_innodb_secondary_clustered_part_count(
        database,
        index,
        clustered_index,
        &clustered_part_count
    );
    if (rc == MYLITE_OK) {
        if (field_count > (size_t)INT64_MAX - clustered_part_count) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        field_count += clustered_part_count;
        *out_field_count = (int64_t)field_count;
    }
    return rc;
}

static int information_schema_innodb_secondary_clustered_part_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    size_t *out_count
) {
    size_t count = 0U;

    if (clustered_index == NULL) {
        *out_count = information_schema_innodb_generated_cluster_key_part_count;
        return MYLITE_OK;
    }

    for (size_t part_index = 0U; part_index < clustered_index->part_count; ++part_index) {
        if (!information_schema_innodb_index_contains_column(
                index,
                clustered_index->parts[part_index].column.column_id
            )) {
            if (count == SIZE_MAX) {
                set_runtime_error(
                    database,
                    "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count"
                );
                return MYLITE_ERROR;
            }
            ++count;
        }
    }

    *out_count = count;
    return MYLITE_OK;
}

static int information_schema_innodb_generated_cluster_field_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_field_count
) {
    if (column_count >
        (size_t)INT64_MAX - information_schema_innodb_generated_cluster_system_field_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
        return MYLITE_ERROR;
    }

    *out_field_count =
        (int64_t)(column_count + information_schema_innodb_generated_cluster_system_field_count);
    return MYLITE_OK;
}

static int information_schema_innodb_generated_cluster_index_id(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_index_id
) {
    if (table_id < 0 ||
        table_id > INT64_MAX - information_schema_innodb_generated_cluster_index_id_base) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES generated index id");
        return MYLITE_ERROR;
    }

    *out_index_id = information_schema_innodb_generated_cluster_index_id_base + table_id;
    return MYLITE_OK;
}

static const struct loaded_index_info *information_schema_innodb_clustered_index(
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    for (size_t index = 0U; index < index_count; ++index) {
        if (indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            return &indexes[index];
        }
    }
    for (size_t index = 0U; index < index_count; ++index) {
        if (information_schema_innodb_index_is_all_not_null_unique(&indexes[index])) {
            return &indexes[index];
        }
    }

    return NULL;
}

static bool information_schema_innodb_index_is_all_not_null_unique(
    const struct loaded_index_info *index
) {
    if (index->index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY || !index->index.is_unique ||
        index->part_count == 0U) {
        return false;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column.is_nullable) {
            return false;
        }
    }
    return true;
}

static bool information_schema_innodb_index_contains_column(
    const struct loaded_index_info *index,
    int64_t column_id
) {
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column.column_id == column_id) {
            return true;
        }
    }
    return false;
}

static int append_information_schema_innodb_foreign_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_foreign_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_innodb_foreign_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_foreign_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    struct mylite_catalog_schema_descriptor parent_schema = {0};
    const struct mylite_catalog_schema_descriptor *reference_schema = schema;
    char *foreign_key_id = NULL;
    char *foreign_table_name = NULL;
    char *referenced_table_name = NULL;
    char column_count_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    int64_t type = 0;
    int rc = MYLITE_OK;

    if (foreign_key->part_count > (size_t)INT64_MAX) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN column count");
        return MYLITE_ERROR;
    }

    if (foreign_key->parent_table.schema_id != schema->schema_id) {
        rc = mylite_catalog_read_schema_by_id(
            database,
            foreign_key->parent_table.schema_id,
            &parent_schema
        );
        if (rc == MYLITE_OK) {
            reference_schema = &parent_schema;
        }
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_id(
            database,
            schema,
            foreign_key,
            &foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_table_name(
            database,
            schema,
            table,
            &foreign_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_table_name(
            database,
            reference_schema,
            &foreign_key->parent_table,
            &referenced_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)foreign_key->part_count,
            column_count_text,
            sizeof(column_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_foreign_type(database, &foreign_key->foreign_key, &type);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, type, type_text, sizeof(type_text));
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_foreign_column_count] = {
            foreign_key_id,
            foreign_table_name,
            referenced_table_name,
            column_count_text,
            type_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(referenced_table_name);
    free(foreign_table_name);
    free(foreign_key_id);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_foreign_cols_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN_COLS columns");
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_innodb_foreign_cols_foreign_key_rows(
            database,
            rows,
            schema,
            &foreign_keys[index]
        );
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key
) {
    char *foreign_key_id = NULL;
    int rc =
        copy_information_schema_innodb_foreign_id(database, schema, foreign_key, &foreign_key_id);

    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key->part_count; ++index) {
        rc = append_information_schema_innodb_foreign_cols_part_row(
            database,
            rows,
            foreign_key_id,
            &foreign_key->parts[index]
        );
    }

    free(foreign_key_id);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *foreign_key_id,
    const struct loaded_foreign_key_part *part
) {
    char position_text[integer_text_capacity];
    int rc = information_schema_format_i64(
        database,
        part->foreign_key_column.ordinal_position,
        position_text,
        sizeof(position_text)
    );

    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_foreign_cols_column_count] = {
            foreign_key_id,
            part->child_column.name,
            part->parent_column.name,
            position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int copy_information_schema_innodb_foreign_id(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_id
) {
    return copy_information_schema_schema_object_name(
        database,
        schema->name,
        foreign_key->foreign_key.name,
        out_id
    );
}

static int copy_information_schema_innodb_foreign_table_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
) {
    return copy_information_schema_schema_object_name(
        database,
        schema->name,
        table->name,
        out_name
    );
}

static int copy_information_schema_schema_object_name(
    struct mylite_db *database,
    const char *schema_name,
    const char *object_name,
    char **out_name
) {
    size_t schema_length = strlen(schema_name);
    size_t object_length = strlen(object_name);
    size_t required_length = 0U;
    char *name = NULL;

    *out_name = NULL;
    if (schema_length > SIZE_MAX - object_length - tablespace_name_separator_size -
                            tablespace_name_terminator_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    required_length = schema_length + object_length + tablespace_name_separator_size +
                      tablespace_name_terminator_size;
    name = (char *)malloc(required_length);
    if (name == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(name, schema_name, schema_length);
    name[schema_length] = '/';
    memcpy(&name[schema_length + tablespace_name_separator_size], object_name, object_length);
    name[required_length - tablespace_name_terminator_size] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int information_schema_innodb_foreign_type(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    int64_t *out_type
) {
    static const struct information_schema_innodb_foreign_action_type_flags delete_flags = {
        .cascade = information_schema_innodb_foreign_delete_cascade_type,
        .set_null = information_schema_innodb_foreign_delete_set_null_type,
        .no_action = information_schema_innodb_foreign_delete_no_action_type,
    };
    static const struct information_schema_innodb_foreign_action_type_flags update_flags = {
        .cascade = information_schema_innodb_foreign_update_cascade_type,
        .set_null = information_schema_innodb_foreign_update_set_null_type,
        .no_action = information_schema_innodb_foreign_update_no_action_type,
    };
    int64_t delete_type = 0;
    int64_t update_type = 0;
    int rc = information_schema_innodb_foreign_action_type(
        database,
        foreign_key->delete_rule,
        &delete_flags,
        &delete_type
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_foreign_action_type(
            database,
            foreign_key->update_rule,
            &update_flags,
            &update_type
        );
    }
    if (rc == MYLITE_OK) {
        *out_type = delete_type | update_type;
    }
    return rc;
}

static int information_schema_innodb_foreign_action_type(
    struct mylite_db *database,
    const char *rule,
    const struct information_schema_innodb_foreign_action_type_flags *flags,
    int64_t *out_type
) {
    if (foreign_key_rule_is_cascade(rule)) {
        *out_type = flags->cascade;
        return MYLITE_OK;
    }
    if (foreign_key_rule_is_set_null(rule)) {
        *out_type = flags->set_null;
        return MYLITE_OK;
    }
    if (foreign_key_rule_equals(rule, "NO ACTION")) {
        *out_type = flags->no_action;
        return MYLITE_OK;
    }
    if (foreign_key_rule_equals(rule, "RESTRICT")) {
        *out_type = 0;
        return MYLITE_OK;
    }

    set_runtime_error(database, "unknown INFORMATION_SCHEMA.INNODB_FOREIGN action");
    return MYLITE_ERROR;
}

static int append_information_schema_table_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    size_t check_constraint_count = 0U;
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_check_constraint_infos(
            database,
            table->table_id,
            &check_constraints,
            &check_constraint_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_table_constraints_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_table_constraints_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        rc = append_information_schema_table_constraints_check_row(
            database,
            rows,
            schema,
            table,
            &check_constraints[index]
        );
    }

    loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_table_constraints_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    const char *constraint_type =
        index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ? "PRIMARY KEY" : "UNIQUE";
    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        index->index.name,
        schema->name,
        table->name,
        constraint_type,
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        schema->name,
        table->name,
        "FOREIGN KEY",
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_check_constraint_info *check_constraint
) {
    const char *enforced = "NO";

    if (check_constraint->check_constraint.is_enforced) {
        enforced = "YES";
    }

    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        check_constraint->check_constraint.name,
        schema->name,
        table->name,
        "CHECK",
        enforced,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_table_constraints_mysql_system_table_row(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_table_constraints_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        const char *values[information_schema_table_constraints_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            definition->schema_name,
            definition->query_definition.name,
            "PRIMARY KEY",
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];

        if (!secondary_index->is_unique) {
            continue;
        }
        const char *values[information_schema_table_constraints_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            definition->schema_name,
            definition->query_definition.name,
            "UNIQUE",
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_table_constraints_extensions_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_table_constraints_extensions_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_table_constraints_extensions_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_table_constraints_extensions_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    const char *values[information_schema_table_constraints_extensions_column_count] = {
        "def",
        schema->name,
        index->index.name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_extensions_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_table_constraints_extensions_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_extensions_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_table_constraints_extensions_mysql_system_table_row(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_table_constraints_extensions_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        const char *values[information_schema_table_constraints_extensions_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            definition->query_definition.name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        const char *values[information_schema_table_constraints_extensions_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            definition->query_definition.name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_check_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t check_constraint_count = 0U;
    int rc = load_table_check_constraint_infos(
        database,
        table->table_id,
        &check_constraints,
        &check_constraint_count
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        rc = append_information_schema_check_constraints_check_row(
            database,
            rows,
            schema,
            &check_constraints[index]
        );
    }

    loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    return rc;
}

static int append_information_schema_check_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_check_constraint_info *check_constraint
) {
    const char *values[information_schema_check_constraints_column_count] = {
        "def",
        schema->name,
        check_constraint->check_constraint.name,
        check_constraint->check_constraint.check_clause,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_key_column_usage_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_key_column_usage_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_key_column_usage_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_key_column_usage_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    char ordinal_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            schema->name,
            index->index.name,
            "def",
            schema->name,
            table->name,
            part->column.name,
            ordinal_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_key_column_usage_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    char ordinal_text[integer_text_capacity];
    char referenced_ordinal_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *part = &foreign_key->parts[part_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            schema->name,
            foreign_key->foreign_key.name,
            "def",
            schema->name,
            table->name,
            part->child_column.name,
            ordinal_text,
            referenced_ordinal_text,
            schema->name,
            foreign_key->parent_table.name,
            part->parent_column.name,
        };

        rc = information_schema_format_i64(
            database,
            part->foreign_key_column.ordinal_position,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                part->foreign_key_column.position_in_unique_constraint,
                referenced_ordinal_text,
                sizeof(referenced_ordinal_text)
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_key_column_usage_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_key_column_usage_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_key_column_usage_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;
    size_t primary_key_column_count = mysql_system_table_primary_key_column_count(definition);

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }

    for (size_t key_index = 0U; rc == MYLITE_OK && key_index < primary_key_column_count;
         ++key_index) {
        const struct mylite_execution_catalog_column_definition *column =
            mysql_system_table_primary_key_column(definition, key_index);
        char sequence_text[integer_text_capacity];

        if (column == NULL) {
            set_runtime_error(database, "invalid mysql system table primary key");
            return MYLITE_ERROR;
        }

        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            sequence_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)key_index + 1,
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        char sequence_text[integer_text_capacity];

        if (!secondary_index->is_unique) {
            continue;
        }
        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table unique key");
            return MYLITE_ERROR;
        }

        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[secondary_index->column_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            sequence_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(database, 1, sequence_text, sizeof(sequence_text));
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_referential_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_referential_constraints_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_referential_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_referential_constraints_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        "def",
        schema->name,
        foreign_key->parent_index.name,
        foreign_key->foreign_key.match_option,
        foreign_key->foreign_key.update_rule,
        foreign_key->foreign_key.delete_rule,
        table->name,
        foreign_key->parent_table.name,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_statistics_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_statistics_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_statistics_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;
    size_t primary_key_column_count = mysql_system_table_primary_key_column_count(definition);

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }

    for (size_t key_index = 0U; rc == MYLITE_OK && key_index < primary_key_column_count;
         ++key_index) {
        const struct mylite_execution_catalog_column_definition *column =
            mysql_system_table_primary_key_column(definition, key_index);
        char sequence_text[integer_text_capacity];

        if (column == NULL) {
            set_runtime_error(database, "invalid mysql system table primary key");
            return MYLITE_ERROR;
        }

        const char *values[information_schema_statistics_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            "0",
            definition->schema_name,
            "PRIMARY",
            sequence_text,
            column->name,
            mysql_system_table_primary_key_collation(definition, key_index + 1),
            mysql_system_table_primary_key_cardinality(definition, key_index + 1),
            NULL,
            NULL,
            "",
            "BTREE",
            "",
            "",
            "YES",
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)key_index + 1,
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        char sequence_text[integer_text_capacity];

        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table secondary index");
            return MYLITE_ERROR;
        }

        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[secondary_index->column_index];
        const char *values[information_schema_statistics_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            secondary_index->non_unique,
            definition->schema_name,
            secondary_index->name,
            sequence_text,
            column->name,
            "A",
            secondary_index->cardinality,
            NULL,
            NULL,
            "",
            "BTREE",
            "",
            "",
            "YES",
            NULL,
        };

        rc = information_schema_format_i64(database, 1, sequence_text, sizeof(sequence_text));
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_statistics_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_statistics_base_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_statistics_base_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    static const char cardinality[] = "0";
    char sequence[integer_text_capacity];
    const char *non_unique = "1";
    int rc = MYLITE_OK;

    if (index->index.is_unique) {
        non_unique = "0";
    }

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        char prefix_text[integer_text_capacity];
        const char *nullable = "";
        const bool is_fulltext = index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT;
        const bool is_spatial = index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL;
        const char *collation =
            part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC ? "D"
                                                                                          : "A";
        const char *prefix_value = NULL;
        const char *index_type = "BTREE";

        if (part->column.is_nullable) {
            nullable = "YES";
        }
        if (is_fulltext) {
            collation = NULL;
            index_type = "FULLTEXT";
        }
        if (is_spatial) {
            collation = "A";
            prefix_value = "32";
            index_type = "SPATIAL";
        }
        if (!is_fulltext && !is_spatial && part->index_column.has_prefix_length) {
            rc = information_schema_format_i64(
                database,
                part->index_column.prefix_length,
                prefix_text,
                sizeof(prefix_text)
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            prefix_value = prefix_text;
        }
        const char *values[information_schema_statistics_column_count] = {
            "def",
            schema->name,
            table->name,
            non_unique,
            schema->name,
            index->index.name,
            sequence,
            part->column.name,
            collation,
            cardinality,
            prefix_value,
            NULL,
            nullable,
            index_type,
            "",
            index->index.comment,
            index_visibility_text(index->index.is_visible),
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position,
            sequence,
            sizeof(sequence)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
) {
    int rc = MYLITE_OK;

    if (query->is_count_star) {
        char *column_name = NULL;

        if (query->count_alias != NULL) {
            rc = copy_select_item_alias_text(database, query->count_alias, &column_name);
        } else {
            rc = copy_aggregate_result_column_name(
                database,
                &query->count_expression->span,
                &column_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_result_append_column(result, column_name);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
        free(column_name);
        return rc;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < query->projection_count;
         ++column_index) {
        rc = mylite_result_append_column(result, query->projection_names[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_append_result_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    size_t *indexes = NULL;
    size_t index_count = 0U;
    size_t visible_count = 0U;
    const char **values = NULL;
    int rc = information_schema_matching_row_indexes(
        database,
        statement,
        query,
        rows,
        &indexes,
        &index_count
    );

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (rc == MYLITE_OK && index_count == 0U) {
        free(indexes);
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && rows->rows == NULL) {
        free(indexes);
        set_runtime_error(database, "information_schema row set is inconsistent");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_sort_row_indexes(query, rows, indexes, index_count);
    }
    if (rc == MYLITE_OK && query->projection_count != 0U) {
        values = (const char **)calloc(query->projection_count, sizeof(*values));
        if (values == NULL) {
            free(indexes);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    for (size_t output_index = 0U; rc == MYLITE_OK && output_index < index_count; ++output_index) {
        size_t row_index = indexes[output_index];

        if (query->limit.has_limit && visible_count >= (size_t)query->limit.row_count) {
            break;
        }
        for (size_t column_index = 0U; column_index < query->projection_count; ++column_index) {
            values[column_index] = rows->rows[row_index][query->projection_indexes[column_index]];
        }
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
        ++visible_count;
    }
    free((void *)values);
    if (rc == MYLITE_OK && out_read_row_count != NULL) {
        *out_read_row_count = visible_count;
    }
    free(indexes);
    return rc;
}

static int information_schema_append_count_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    char count_text[integer_text_capacity];
    const char *values[1] = {count_text};
    size_t *indexes = NULL;
    size_t index_count = 0U;
    bool emits_count_row = true;
    int rc = information_schema_matching_row_indexes(
        database,
        statement,
        query,
        rows,
        &indexes,
        &index_count
    );

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (query->limit.has_limit && query->limit.row_count == 0) {
        emits_count_row = false;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)index_count,
            count_text,
            sizeof(count_text)
        );
    }
    if (rc == MYLITE_OK && emits_count_row) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && emits_count_row && out_read_row_count != NULL) {
        *out_read_row_count = index_count;
    }
    free(indexes);
    return rc;
}

static int information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t **out_indexes,
    size_t *out_index_count
) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    size_t *indexes = NULL;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    *out_indexes = NULL;
    *out_index_count = 0U;
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
            break;
        }
        optional_clause = optional_clause->next_sibling;
    }

    if (rows->row_count == 0U) {
        return MYLITE_OK;
    }
    if (rows->row_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    indexes = malloc(rows->row_count * sizeof(*indexes));
    if (indexes == NULL && rows->row_count != 0U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->row_count; ++row_index) {
        bool matches = true;

        if (where_clause != NULL) {
            rc = information_schema_predicate_matches(
                database,
                query,
                child_at(where_clause, 0U),
                rows->rows[row_index],
                &matches
            );
        }
        if (rc == MYLITE_OK && matches) {
            indexes[index_count] = row_index;
            ++index_count;
        }
    }
    if (rc != MYLITE_OK) {
        free(indexes);
        return rc;
    }

    *out_indexes = indexes;
    *out_index_count = index_count;
    return MYLITE_OK;
}

static int information_schema_sort_row_indexes(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    if (!query->has_order) {
        return MYLITE_OK;
    }
    for (size_t index = 1U; index < index_count; ++index) {
        size_t row_index = indexes[index];
        size_t scan = index;

        while (scan > 0U && information_schema_compare_rows(
                                query,
                                rows,
                                (struct information_schema_row_order_pair){
                                    .left_row = row_index,
                                    .right_row = indexes[scan - 1U],
                                }
                            ) < 0) {
            indexes[scan] = indexes[scan - 1U];
            --scan;
        }
        indexes[scan] = row_index;
    }
    return MYLITE_OK;
}

static int information_schema_compare_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair
) {
    const char *left = rows->rows[pair.left_row][query->order_index];
    const char *right = rows->rows[pair.right_row][query->order_index];
    int comparison = 0;

    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        comparison = -1;
    } else if (right == NULL) {
        comparison = 1;
    } else if (information_schema_column_is_numeric(query->definition, query->order_index)) {
        int64_t left_value = 0;
        int64_t right_value = 0;

        if (information_schema_text_to_i64(left, &left_value) == MYLITE_OK &&
            information_schema_text_to_i64(right, &right_value) == MYLITE_OK) {
            comparison = (left_value > right_value) - (left_value < right_value);
        } else {
            comparison =
                information_schema_compare_text(query->definition, query->order_index, left, right);
        }
    } else {
        comparison =
            information_schema_compare_text(query->definition, query->order_index, left, right);
    }
    if (query->order_direction == PLANNED_SELECT_ORDER_DESC) {
        comparison = -comparison;
    }
    return comparison;
}

static int information_schema_plan_where(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct information_schema_query *query
) {
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports metadata predicates");
        return MYLITE_ERROR;
    }
    return information_schema_validate_predicate(database, query, child_at(where_clause, 0U));
}

static int information_schema_validate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_frame_stack frame_stack = {0};
    struct information_schema_predicate_eval_frame frame = {
        .node = predicate_node,
        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
    };
    int rc = information_schema_predicate_frame_stack_push(database, &frame_stack, frame);

    while (rc == MYLITE_OK && information_schema_predicate_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
            continue;
        }

        switch (current->kind) {
        case MYLITE_SQL_AST_NOT_PREDICATE:
            rc = information_schema_predicate_frame_stack_push(
                database,
                &frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(current, 0U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
            break;
        case MYLITE_SQL_AST_AND_PREDICATE:
        case MYLITE_SQL_AST_OR_PREDICATE:
        case MYLITE_SQL_AST_XOR_PREDICATE:
            rc = information_schema_predicate_frame_stack_push(
                database,
                &frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(current, 1U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
            if (rc == MYLITE_OK) {
                rc = information_schema_predicate_frame_stack_push(
                    database,
                    &frame_stack,
                    (struct information_schema_predicate_eval_frame){
                        .node = child_at(current, 0U),
                        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                    }
                );
            }
            break;
        case MYLITE_SQL_AST_COMPARISON_PREDICATE:
            rc = information_schema_validate_comparison_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_IS_NULL_PREDICATE:
            rc = information_schema_validate_is_null_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_BETWEEN_PREDICATE:
            rc = information_schema_validate_between_predicate(database, query, current);
            break;
        case MYLITE_SQL_AST_IN_PREDICATE:
            rc = information_schema_validate_in_predicate(database, query, current);
            break;
        default:
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
            break;
        }
    }

    information_schema_predicate_frame_stack_deinit(&frame_stack);
    return rc;
}

static int information_schema_validate_comparison_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_value right = {
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc == MYLITE_OK) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
            rc = information_schema_predicate_like_pattern_text(
                database,
                child_at(predicate_node, 1U),
                &right
            );
        } else {
            rc = information_schema_predicate_value_text(
                database,
                child_at(predicate_node, 1U),
                &right
            );
        }
    }
    free(right.text);
    return rc;
}

static int information_schema_validate_is_null_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    size_t column_index = 0U;

    return information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );
}

static int information_schema_validate_between_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    struct information_schema_predicate_value lower = {0};
    struct information_schema_predicate_value upper = {0};
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 1U),
            &lower
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 2U),
            &upper
        );
    }
    free(lower.text);
    free(upper.text);
    return rc;
}

static int information_schema_validate_in_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *values = child_at(predicate_node, 1U);
    const struct mylite_sql_ast_node *value = NULL;
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    (void)column_index;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || values->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE does not support IN subqueries");
        return MYLITE_ERROR;
    }
    if (values->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports literal IN lists");
        return MYLITE_ERROR;
    }

    value = child_at(values, 0U);
    while (rc == MYLITE_OK && value != NULL) {
        struct information_schema_predicate_value item = {0};

        rc = information_schema_predicate_literal_value_text(database, value, &item);
        free(item.text);
        value = value->next_sibling;
    }
    return rc;
}

static int information_schema_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    bool *out_matches
) {
    struct information_schema_predicate_frame_stack frame_stack = {0};
    struct information_schema_truth_stack truth_stack = {0};
    struct information_schema_predicate_eval_frame frame = {
        .node = predicate_node,
        .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
    };
    int rc = information_schema_predicate_frame_stack_push(database, &frame_stack, frame);

    *out_matches = false;
    while (rc == MYLITE_OK && information_schema_predicate_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports metadata predicates"
            );
            rc = MYLITE_ERROR;
        } else if (frame.action == INFORMATION_SCHEMA_PREDICATE_VISIT) {
            rc = information_schema_visit_predicate(database, &frame_stack, current);
        } else {
            rc = information_schema_evaluate_predicate(database, query, current, row, &truth_stack);
        }
    }
    if (rc == MYLITE_OK) {
        enum information_schema_truth_value truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

        if (truth_stack.count != 1U || !information_schema_truth_stack_pop(&truth_stack, &truth)) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate state");
            rc = MYLITE_ERROR;
        } else {
            *out_matches = information_schema_truth_is_true(truth);
        }
    }

    information_schema_truth_stack_deinit(&truth_stack);
    information_schema_predicate_frame_stack_deinit(&frame_stack);
    return rc;
}

static void information_schema_predicate_frame_stack_deinit(
    struct information_schema_predicate_frame_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_predicate_frame_stack){0};
}

static int information_schema_predicate_frame_stack_push(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame frame
) {
    struct information_schema_predicate_eval_frame *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = frame;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static bool information_schema_predicate_frame_stack_pop(
    struct information_schema_predicate_frame_stack *stack,
    struct information_schema_predicate_eval_frame *out_frame
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_frame = stack->items[stack->count];
    return true;
}

static void information_schema_truth_stack_deinit(struct information_schema_truth_stack *stack) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_truth_stack){0};
}

static int information_schema_truth_stack_push(
    struct mylite_db *database,
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value value
) {
    enum information_schema_truth_value *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = value;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = value;
    ++stack->count;
    return MYLITE_OK;
}

static bool information_schema_truth_stack_pop(
    struct information_schema_truth_stack *stack,
    enum information_schema_truth_value *out_value
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_value = stack->items[stack->count];
    return true;
}

static int information_schema_visit_predicate(
    struct mylite_db *database,
    struct information_schema_predicate_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate_node
) {
    int rc = information_schema_predicate_frame_stack_push(
        database,
        frame_stack,
        (struct information_schema_predicate_eval_frame){
            .node = predicate_node,
            .action = INFORMATION_SCHEMA_PREDICATE_EVALUATE,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate_node->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return information_schema_predicate_frame_stack_push(
            database,
            frame_stack,
            (struct information_schema_predicate_eval_frame){
                .node = child_at(predicate_node, 0U),
                .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
            }
        );
    }
    if (predicate_node->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_OR_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        rc = information_schema_predicate_frame_stack_push(
            database,
            frame_stack,
            (struct information_schema_predicate_eval_frame){
                .node = child_at(predicate_node, 1U),
                .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
            }
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_predicate_frame_stack_push(
                database,
                frame_stack,
                (struct information_schema_predicate_eval_frame){
                    .node = child_at(predicate_node, 0U),
                    .action = INFORMATION_SCHEMA_PREDICATE_VISIT,
                }
            );
        }
    }
    return rc;
}

static int information_schema_evaluate_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    struct information_schema_truth_stack *truth_stack
) {
    enum information_schema_truth_value truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    int rc = MYLITE_OK;

    if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = information_schema_comparison_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = information_schema_is_null_predicate_matches(
            database,
            query,
            predicate_node,
            row,
            &truth
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = information_schema_between_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = information_schema_in_matches(database, query, predicate_node, row, &truth);
    } else if (predicate_node->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum information_schema_truth_value child_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

        if (!information_schema_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate stack");
            return MYLITE_ERROR;
        }
        if (child_truth == INFORMATION_SCHEMA_TRUTH_TRUE) {
            truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else if (child_truth == INFORMATION_SCHEMA_TRUTH_FALSE) {
            truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else {
            truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (
        predicate_node->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_OR_PREDICATE ||
        predicate_node->kind == MYLITE_SQL_AST_XOR_PREDICATE
    ) {
        rc = information_schema_compound_predicate_matches(
            database,
            predicate_node->kind,
            truth_stack,
            &truth
        );
    } else {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports metadata predicates");
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = information_schema_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int information_schema_is_null_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    size_t column_index = 0U;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    if (rc == MYLITE_OK) {
        bool is_null = row[column_index] == NULL;
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);

        *out_truth = information_schema_truth_from_bool(is_null);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_IS_NULL) {
            if (is_null) {
                *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
            } else {
                *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
            }
        }
    }
    return rc;
}

static int information_schema_compound_predicate_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_node_kind predicate_kind,
    struct information_schema_truth_stack *truth_stack,
    enum information_schema_truth_value *out_truth
) {
    enum information_schema_truth_value right_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    enum information_schema_truth_value left_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;

    if (!information_schema_truth_stack_pop(truth_stack, &right_truth) ||
        !information_schema_truth_stack_pop(truth_stack, &left_truth)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA predicate stack");
        return MYLITE_ERROR;
    }
    if (predicate_kind == MYLITE_SQL_AST_AND_PREDICATE) {
        if (left_truth == INFORMATION_SCHEMA_TRUTH_FALSE ||
            right_truth == INFORMATION_SCHEMA_TRUTH_FALSE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else if (
            left_truth == INFORMATION_SCHEMA_TRUTH_TRUE &&
            right_truth == INFORMATION_SCHEMA_TRUTH_TRUE
        ) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else {
            *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (predicate_kind == MYLITE_SQL_AST_OR_PREDICATE) {
        if (left_truth == INFORMATION_SCHEMA_TRUTH_TRUE ||
            right_truth == INFORMATION_SCHEMA_TRUTH_TRUE) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
        } else if (
            left_truth == INFORMATION_SCHEMA_TRUTH_FALSE &&
            right_truth == INFORMATION_SCHEMA_TRUTH_FALSE
        ) {
            *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
        } else {
            *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        }
    } else if (
        left_truth == INFORMATION_SCHEMA_TRUTH_UNKNOWN ||
        right_truth == INFORMATION_SCHEMA_TRUTH_UNKNOWN
    ) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    } else {
        *out_truth = information_schema_truth_from_bool(left_truth != right_truth);
    }
    return MYLITE_OK;
}

static enum information_schema_truth_value information_schema_truth_from_bool(bool condition) {
    if (condition) {
        return INFORMATION_SCHEMA_TRUTH_TRUE;
    }
    return INFORMATION_SCHEMA_TRUTH_FALSE;
}

static bool information_schema_truth_is_true(enum information_schema_truth_value truth) {
    return truth == INFORMATION_SCHEMA_TRUTH_TRUE;
}

static int information_schema_comparison_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    size_t column_index = 0U;
    struct information_schema_predicate_value right = {
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);
    const char *left_text = NULL;
    int rc = information_schema_comparison_predicate_value(
        database,
        query,
        predicate_node,
        operator_kind,
        &column_index,
        &right
    );

    if (rc != MYLITE_OK) {
        free(right.text);
        return rc;
    }

    left_text = row[column_index];
    rc = information_schema_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        &right,
        operator_kind,
        out_truth
    );

    free(right.text);
    return rc;
}

static int information_schema_comparison_predicate_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    enum mylite_sql_ast_operator operator_kind,
    size_t *out_column_index,
    struct information_schema_predicate_value *out_right
) {
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        out_column_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return information_schema_predicate_like_pattern_text(
            database,
            child_at(predicate_node, 1U),
            out_right
        );
    }
    return information_schema_predicate_value_text(
        database,
        child_at(predicate_node, 1U),
        out_right
    );
}

static int information_schema_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return information_schema_like_comparison_truth(
            database,
            query,
            column_index,
            left_text,
            right,
            out_truth
        );
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return information_schema_null_safe_comparison_truth(
            database,
            query,
            column_index,
            left_text,
            right,
            operator_kind,
            out_truth
        );
    }
    return information_schema_regular_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_like_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum information_schema_truth_value *out_truth
) {
    bool like_matches = false;
    int rc = MYLITE_OK;

    if (left_text == NULL || right->is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    rc = information_schema_like_comparison_matches(
        database,
        &query->definition->columns[column_index],
        left_text,
        right->text,
        &like_matches
    );
    if (rc == MYLITE_OK) {
        *out_truth = information_schema_truth_from_bool(like_matches);
    }
    return rc;
}

static int information_schema_null_safe_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    if (left_text == NULL || right->is_null) {
        bool both_null = ((left_text == NULL) && right->is_null) != 0;

        *out_truth = information_schema_truth_from_bool(both_null);
        return MYLITE_OK;
    }

    return information_schema_nonnull_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_regular_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    if (left_text == NULL || right->is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    return information_schema_nonnull_comparison_truth(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        out_truth
    );
}

static int information_schema_nonnull_comparison_truth(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    enum information_schema_truth_value *out_truth
) {
    bool comparison_matches = false;
    int rc = information_schema_compare_nonnull_value_matches(
        database,
        query,
        column_index,
        left_text,
        right,
        operator_kind,
        &comparison_matches
    );

    if (rc == MYLITE_OK) {
        *out_truth = information_schema_truth_from_bool(comparison_matches);
    }
    return rc;
}

static int information_schema_like_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    const char *left_text,
    const char *right_text,
    bool *out_matches
) {
    bool case_sensitive = true;
    bool backslash_escapes = true;

    if (information_schema_column_uses_case_insensitive_collation(column)) {
        case_sensitive = false;
    }
    if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        backslash_escapes = false;
    }

    *out_matches = show_like_pattern_matches_with_escape(
        right_text,
        strlen(right_text),
        left_text,
        strlen(left_text),
        case_sensitive,
        backslash_escapes
    );
    return MYLITE_OK;
}

static int information_schema_compare_nonnull_value_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
) {
    if (information_schema_column_is_numeric(query->definition, column_index)) {
        return information_schema_numeric_comparison_matches(
            database,
            operator_kind,
            left_text,
            right->text,
            out_matches
        );
    }

    return information_schema_text_comparison_matches(
        database,
        &query->definition->columns[column_index],
        operator_kind,
        left_text,
        right->text,
        right->is_numeric,
        out_matches
    );
}

static int information_schema_between_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    struct information_schema_predicate_value lower = {0};
    struct information_schema_predicate_value upper = {0};
    size_t column_index = 0U;
    const char *left_text = NULL;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 1U),
            &lower
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_predicate_literal_value_text(
            database,
            child_at(predicate_node, 2U),
            &upper
        );
    }
    if (rc != MYLITE_OK) {
        free(lower.text);
        free(upper.text);
        return rc;
    }

    left_text = row[column_index];
    if (left_text == NULL || lower.is_null || upper.is_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    } else {
        bool lower_matches = false;
        bool upper_matches = false;

        rc = information_schema_between_bound_matches(
            database,
            query,
            column_index,
            left_text,
            &lower,
            MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
            &lower_matches
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_between_bound_matches(
                database,
                query,
                column_index,
                left_text,
                &upper,
                MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
                &upper_matches
            );
        }
        if (rc == MYLITE_OK) {
            *out_truth = information_schema_truth_from_bool((lower_matches && upper_matches) != 0);
        }
    }

    free(lower.text);
    free(upper.text);
    return rc;
}

static int information_schema_between_bound_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t column_index,
    const char *left_text,
    const struct information_schema_predicate_value *right,
    enum mylite_sql_ast_operator operator_kind,
    bool *out_matches
) {
    int comparison = 0;

    if (information_schema_column_is_numeric(query->definition, column_index)) {
        return information_schema_numeric_comparison_matches(
            database,
            operator_kind,
            left_text,
            right->text,
            out_matches
        );
    }

    comparison = information_schema_compare_column_text(
        &query->definition->columns[column_index],
        left_text,
        right->text
    );
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((comparison <= 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((comparison >= 0) != 0);
    } else {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE BETWEEN supports bound ordering");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_in_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate_node,
    char **row,
    enum information_schema_truth_value *out_truth
) {
    const struct mylite_sql_ast_node *values = child_at(predicate_node, 1U);
    const struct mylite_sql_ast_node *value = NULL;
    size_t column_index = 0U;
    const char *left_text = NULL;
    bool saw_null = false;
    int rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate_node, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );

    *out_truth = INFORMATION_SCHEMA_TRUTH_FALSE;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values == NULL || values->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE does not support IN subqueries");
        return MYLITE_ERROR;
    }
    if (values->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(database, "INFORMATION_SCHEMA WHERE supports literal IN lists");
        return MYLITE_ERROR;
    }

    left_text = row[column_index];
    if (left_text == NULL) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
        return MYLITE_OK;
    }

    value = child_at(values, 0U);
    while (rc == MYLITE_OK && value != NULL) {
        struct information_schema_predicate_value item = {0};

        rc = information_schema_predicate_literal_value_text(database, value, &item);
        if (rc == MYLITE_OK) {
            if (item.is_null) {
                saw_null = true;
            } else {
                bool item_matches = false;

                rc = information_schema_compare_nonnull_value_matches(
                    database,
                    query,
                    column_index,
                    left_text,
                    &item,
                    MYLITE_SQL_AST_OPERATOR_EQUAL,
                    &item_matches
                );
                if (rc == MYLITE_OK && item_matches) {
                    *out_truth = INFORMATION_SCHEMA_TRUTH_TRUE;
                    free(item.text);
                    return MYLITE_OK;
                }
            }
        }
        free(item.text);
        value = value->next_sibling;
    }
    if (rc == MYLITE_OK && saw_null) {
        *out_truth = INFORMATION_SCHEMA_TRUTH_UNKNOWN;
    }
    return rc;
}

static int information_schema_numeric_comparison_matches(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool *out_matches
) {
    int64_t left_value = 0;
    int64_t right_value = 0;

    if (information_schema_text_to_i64(left_text, &left_value) != MYLITE_OK ||
        information_schema_text_to_i64(right_text, &right_value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison requires integer metadata values"
        );
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        *out_matches = ((left_value == right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        *out_matches = ((left_value != right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS) {
        *out_matches = ((left_value < right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((left_value <= right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER) {
        *out_matches = ((left_value > right_value) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((left_value >= right_value) != 0);
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison supports equality and ordering operators"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_text_comparison_matches(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column,
    enum mylite_sql_ast_operator operator_kind,
    const char *left_text,
    const char *right_text,
    bool right_is_numeric,
    bool *out_matches
) {
    int comparison = 0;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        *out_matches = information_schema_compare_column_text(column, left_text, right_text) == 0;
        return MYLITE_OK;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_EQUAL) {
        *out_matches = information_schema_compare_column_text(column, left_text, right_text) != 0;
        return MYLITE_OK;
    }
    if (!right_is_numeric) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE string predicates support only equality comparisons"
        );
        return MYLITE_ERROR;
    }

    comparison = information_schema_compare_column_text(column, left_text, right_text);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS) {
        *out_matches = ((comparison < 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_LESS_EQUAL) {
        *out_matches = ((comparison <= 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER) {
        *out_matches = ((comparison > 0) != 0);
    } else if (operator_kind == MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL) {
        *out_matches = ((comparison >= 0) != 0);
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE comparison supports equality and ordering operators"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_predicate_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    *out_value = (struct information_schema_predicate_value){
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    if (value_node == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE supports string, integer, DATABASE(), and SCHEMA() values"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
        value_node->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION) {
        return information_schema_predicate_function_value(database, value_node, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        size_t text_length = 0U;

        return decode_sql_string_literal(
            database,
            value_node,
            "INFORMATION_SCHEMA WHERE supports only string literals",
            "INFORMATION_SCHEMA WHERE string literals do not support NUL bytes",
            &out_value->text,
            &text_length
        );
    }

    return information_schema_predicate_integer_value_text(database, value_node, out_value);
}

static int information_schema_predicate_literal_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    if (value_node != NULL && (value_node->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
                               value_node->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION)) {
        *out_value = (struct information_schema_predicate_value){0};
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE IN and BETWEEN support only literal values"
        );
        return MYLITE_ERROR;
    }
    return information_schema_predicate_value_text(database, value_node, out_value);
}

static int information_schema_predicate_like_pattern_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct information_schema_predicate_value){
        .text = NULL,
        .is_null = false,
        .is_numeric = false,
    };
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE LIKE supports only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "INFORMATION_SCHEMA WHERE LIKE supports only string pattern literals",
        "INFORMATION_SCHEMA WHERE LIKE pattern literals do not support NUL bytes",
        &out_value->text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(out_value->text, text_length)) {
        free(out_value->text);
        *out_value = (struct information_schema_predicate_value){0};
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA WHERE LIKE pattern literals support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_predicate_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    struct session_scalar_cell cell = {0};
    int rc = session_scalar_value(database, value_node, &cell);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (cell.value == NULL) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    return duplicate_text(database, cell.value, &out_value->text);
}

static int information_schema_predicate_integer_value_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct information_schema_predicate_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    char buffer[integer_text_capacity];
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports only decimal integer predicate values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA WHERE supports string, integer, DATABASE(), and SCHEMA() values"
            );
            return MYLITE_ERROR;
        }
        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
    }

    rc = information_schema_format_signed_magnitude(
        database,
        is_negative,
        magnitude,
        buffer,
        sizeof(buffer)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_numeric = true;
    return duplicate_text(database, buffer, &out_value->text);
}

static int information_schema_format_signed_magnitude(
    struct mylite_db *database,
    bool is_negative,
    uint64_t magnitude,
    char *buffer,
    size_t buffer_size
) {
    const uint64_t int64_positive_max = (uint64_t)INT64_MAX;
    const uint64_t int64_negative_abs_max = ((uint64_t)INT64_MAX) + 1U;
    int written = 0;

    if (is_negative) {
        if (magnitude > int64_negative_abs_max) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        if (magnitude == int64_negative_abs_max) {
            written = snprintf(buffer, buffer_size, "%" PRId64, INT64_MIN);
        } else {
            written = snprintf(buffer, buffer_size, "-%" PRIu64, magnitude);
        }
    } else {
        if (magnitude > int64_positive_max) {
            set_limit_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        written = snprintf(buffer, buffer_size, "%" PRIu64, magnitude);
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA predicate value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_resolve_column_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *column_node,
    enum column_reference_diagnostic_context diagnostic_context,
    size_t *out_column_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    const char *column_name = NULL;
    bool qualifier_matches = false;
    int rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        column_name = parts[0];
    } else {
        if (part_count == 2U) {
            if (query->has_alias) {
                qualifier_matches = text_equals_ascii_case_insensitive(parts[0], query->alias);
            } else {
                qualifier_matches =
                    text_equals_ascii_case_insensitive(parts[0], query->definition->name);
            }
        }
        if (qualifier_matches) {
            column_name = parts[1];
        } else {
            rc = information_schema_column_reference_text(database, column_node, &display_text);
            if (rc == MYLITE_OK) {
                set_unknown_column_reference_error(database, diagnostic_context, display_text);
                rc = MYLITE_ERROR;
            }
            free(display_text);
            return rc;
        }
    }

    rc =
        information_schema_table_definition_index(query->definition, column_name, out_column_index);
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    rc = information_schema_column_reference_text(database, column_node, &display_text);
    if (rc == MYLITE_OK) {
        set_unknown_column_reference_error(database, diagnostic_context, display_text);
        rc = MYLITE_ERROR;
    }
    free(display_text);
    return rc;
}

static int information_schema_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_dynamic_string string;
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, parts[part_index]);
        }
    }
    if (rc == MYLITE_OK) {
        *out_text = mylite_dynamic_string_take(&string);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
            set_nomem_error(database);
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        for (size_t column_index = 0U; column_index < out_query->definition->column_count;
             ++column_index) {
            int rc = information_schema_append_projection(database, out_query, column_index, NULL);

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        return MYLITE_OK;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *expression =
            unwrap_parenthesized_expression(child_at(item, 0U));
        enum planned_count_function count_function = count_function_from_expression(expression);
        size_t column_index = 0U;
        int rc = MYLITE_OK;

        if (count_function == PLANNED_COUNT_STAR) {
            if (mylite_sql_ast_node_child_count(select_list) != 1U) {
                set_unsupported_error(
                    database,
                    "INFORMATION_SCHEMA SELECT supports either COUNT(*) or metadata columns"
                );
                return MYLITE_ERROR;
            }
            out_query->is_count_star = true;
            out_query->count_expression = expression;
            out_query->count_alias = child_at(item, 1U);
            return MYLITE_OK;
        }
        if (count_function != PLANNED_COUNT_NONE ||
            (expression == NULL || (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                    expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER))) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports only metadata columns and COUNT(*)"
            );
            return MYLITE_ERROR;
        }

        rc = information_schema_resolve_column_reference(
            database,
            out_query,
            expression,
            COLUMN_REFERENCE_FIELD,
            &column_index
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_append_projection(
                database,
                out_query,
                column_index,
                child_at(item, 1U)
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int information_schema_append_projection(
    struct mylite_db *database,
    struct information_schema_query *query,
    size_t column_index,
    const struct mylite_sql_ast_node *alias
) {
    size_t required_count = query->projection_count + 1U;
    size_t *indexes = NULL;
    char **names = NULL;
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (required_count > SIZE_MAX / sizeof(*indexes) ||
        required_count > SIZE_MAX / sizeof(*names)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = realloc(query->projection_indexes, required_count * sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_indexes = indexes;

    names = (char **)realloc((void *)query->projection_names, required_count * sizeof(*names));
    if (names == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    query->projection_names = names;

    if (alias != NULL) {
        rc = copy_select_item_alias_text(database, alias, &column_name);
    } else {
        rc = duplicate_text(database, query->definition->columns[column_index].name, &column_name);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    query->projection_indexes[query->projection_count] = column_index;
    query->projection_names[query->projection_count] = column_name;
    query->projection_count = required_count;
    return MYLITE_OK;
}

static int information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *direction = NULL;
    int rc = MYLITE_OK;

    out_query->has_order = false;
    out_query->order_direction = PLANNED_SELECT_ORDER_DEFAULT;
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports one ORDER BY column");
        return MYLITE_ERROR;
    }
    rc = information_schema_resolve_column_reference(
        database,
        out_query,
        child_at(order_clause, 0U),
        COLUMN_REFERENCE_ORDER,
        &out_query->order_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_query->has_order = true;
    out_query->order_direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_query->order_direction = PLANNED_SELECT_ORDER_DESC;
    }
    return MYLITE_OK;
}

static int information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
) {
    out_query->limit = (struct planned_select_limit){
        .has_limit = false,
        .row_count = 0,
        .has_offset = false,
        .offset = 0,
    };
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE || child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    return plan_delete_limit(database, limit_clause, &out_query->limit);
}

static int information_schema_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *table_name = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT requires a schema-qualified metadata table"
        );
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        child_at(from_clause, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 2U && schema_name_is_information_schema(parts[0])) {
        table_name = parts[1];
    } else if (part_count == 1U && selected_schema_is_information_schema(database)) {
        table_name = parts[0];
    } else {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT requires a schema-qualified metadata table"
        );
        return MYLITE_ERROR;
    }

    out_query->definition = find_information_schema_table_definition(table_name);
    if (out_query->definition == NULL) {
        set_unknown_information_schema_table_error(database, table_name);
        return MYLITE_ERROR;
    }
    if (from_table_index_hint_list_node(from_clause) != NULL) {
        set_unsupported_error(database, "INFORMATION_SCHEMA SELECT does not support index hints");
        return MYLITE_ERROR;
    }
    if (from_table_alias_node(from_clause) != NULL) {
        rc = copy_identifier_text(
            from_table_alias_node(from_clause),
            out_query->alias,
            sizeof(out_query->alias),
            database
        );
        if (rc == MYLITE_OK) {
            out_query->has_alias = true;
        }
    }
    return rc;
}

static const struct mylite_execution_catalog_table_definition *find_information_schema_table_definition(
    const char *table_name
) {
    return mylite_execution_catalog_information_schema_table_definition_by_name(table_name);
}

static int information_schema_table_definition_index(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *column_name,
    size_t *out_index
) {
    for (size_t column_index = 0U; column_index < definition->column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(
                definition->columns[column_index].name,
                column_name
            )) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static int information_schema_compare_text(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
) {
    return information_schema_compare_column_text(&definition->columns[column_index], left, right);
}

static int information_schema_compare_column_text(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left,
    const char *right
) {
    if (information_schema_column_uses_case_insensitive_collation(column)) {
        return compare_ascii_case_insensitive_text(left, right);
    }
    return strcmp(left, right);
}

static int compare_ascii_case_insensitive_text(const char *left, const char *right) {
    size_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        unsigned char left_byte = (unsigned char)ascii_lower((unsigned char)left[index]);
        unsigned char right_byte = (unsigned char)ascii_lower((unsigned char)right[index]);

        if (left_byte < right_byte) {
            return -1;
        }
        if (left_byte > right_byte) {
            return 1;
        }
        ++index;
    }
    if (left[index] == '\0' && right[index] == '\0') {
        return 0;
    }
    if (left[index] == '\0') {
        return -1;
    }
    return 1;
}

static bool information_schema_column_uses_case_insensitive_collation(
    const struct mylite_execution_catalog_column_definition *column
) {
    const char *collation = column->collation_name;

    if (collation == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(collation, "utf8mb3_general_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb3_tolower_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_0900_ai_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_general_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_unicode_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_unicode_520_ci")) {
        return true;
    }
    return false;
}

static bool information_schema_column_is_numeric(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index
) {
    const char *data_type = definition->columns[column_index].data_type;

    if (data_type == NULL) {
        return false;
    }
    if (strcmp(data_type, "int") == 0 || strcmp(data_type, "bigint") == 0) {
        return true;
    }
    return false;
}

static int information_schema_text_to_i64(const char *text, int64_t *out_value) {
    char *end = NULL;
    long long value = 0;

    if (text == NULL || text[0] == '\0') {
        return MYLITE_ERROR;
    }
    value = strtoll(text, &end, decimal_base);
    if (end == NULL || *end != '\0') {
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)value;
    return MYLITE_OK;
}

static int information_schema_format_i64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_character_metadata_for_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    char *character_length_text,
    size_t character_length_text_size,
    char *character_octet_text,
    size_t character_octet_text_size,
    struct information_schema_character_metadata *out_metadata
) {
    uint64_t character_length = 0U;
    uint64_t character_octet_length = 0U;
    bool has_character_metadata = false;
    bool has_binary_metadata = false;
    int rc = MYLITE_OK;

    *out_metadata = (struct information_schema_character_metadata){0};
    if (column_descriptor_is_char(column)) {
        size_t length = 0U;

        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline CHAR descriptors",
            &length
        );
        character_length = (uint64_t)length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_varchar(column)) {
        size_t length = 0U;

        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline VARCHAR descriptors",
            &length
        );
        character_length = (uint64_t)length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_text_family(column)) {
        const struct text_family_type_info *info =
            text_family_type_info_for_logical_type(column->logical_type);

        if (info == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA.COLUMNS supports only baseline TEXT descriptors"
            );
            return MYLITE_ERROR;
        }
        character_length = info->maximum_length;
        character_octet_length = info->maximum_length;
        has_character_metadata = true;
    } else if (column_descriptor_is_enum(column)) {
        struct enum_type_info info = {0};

        rc = enum_type_info_for_logical_type(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline ENUM descriptors",
            &info
        );
        character_length = (uint64_t)info.max_label_character_length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_set(column)) {
        struct set_type_info info = {0};

        rc = set_type_info_for_logical_type(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.COLUMNS supports only baseline SET descriptors",
            &info
        );
        character_length = (uint64_t)info.max_display_character_length;
        character_octet_length =
            character_length * column_effective_max_bytes_per_character(table, column);
        has_character_metadata = true;
    } else if (column_descriptor_is_binary_string_family(column)) {
        struct binary_string_type_info storage = {0};
        const struct binary_string_type_info *info =
            binary_string_type_info_for_logical_type(column->logical_type, &storage);

        if (info == NULL) {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA.COLUMNS supports only baseline binary descriptors"
            );
            return MYLITE_ERROR;
        }
        character_length = info->maximum_length;
        character_octet_length = info->maximum_length;
        has_character_metadata = true;
        has_binary_metadata = true;
    }
    if (rc != MYLITE_OK || !has_character_metadata) {
        return rc;
    }

    rc = information_schema_format_i64(
        database,
        (int64_t)character_length,
        character_length_text,
        character_length_text_size
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)character_octet_length,
            character_octet_text,
            character_octet_text_size
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *character_set_name = column_effective_character_set_name(table, column);
    const char *collation_name = column_effective_collation_name(table, column);

    *out_metadata = (struct information_schema_character_metadata){
        .character_maximum_length = character_length_text,
        .character_octet_length = character_octet_text,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
    };
    if (has_binary_metadata) {
        out_metadata->character_set_name = NULL;
        out_metadata->collation_name = NULL;
    }
    return MYLITE_OK;
}

static const char *information_schema_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const char *logical_type = column->logical_type;
    const char *spatial_type = NULL;

    if (column_descriptor_is_char(column)) {
        return "char";
    }
    if (column_descriptor_is_varchar(column)) {
        return "varchar";
    }
    if (column_descriptor_is_text_family(column)) {
        return information_schema_text_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_enum(column)) {
        return "enum";
    }
    if (column_descriptor_is_set(column)) {
        return "set";
    }
    if (column_descriptor_is_json(column)) {
        return "json";
    }
    if (spatial_logical_type_display_text(logical_type, &spatial_type)) {
        return spatial_type;
    }
    if (column_descriptor_is_bit(column)) {
        return "bit";
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return information_schema_binary_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_decimal(column)) {
        return "decimal";
    }
    if (column_descriptor_is_approximate(column)) {
        return information_schema_approximate_data_type_for_descriptor(column);
    }
    if (column_descriptor_is_date(column)) {
        return "date";
    }
    if (column_descriptor_is_time(column)) {
        return "time";
    }
    if (column_descriptor_is_datetime(column)) {
        return "datetime";
    }
    if (column_descriptor_is_timestamp(column)) {
        return "timestamp";
    }
    if (column_descriptor_is_year(column)) {
        return "year";
    }
    if (logical_type == NULL) {
        return "";
    }
    return information_schema_integer_data_type_for_descriptor(logical_type);
}

static const char *information_schema_text_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const struct text_family_type_info *info =
        text_family_type_info_for_logical_type(column->logical_type);

    return info == NULL ? "" : info->display_type;
}

static const char *information_schema_binary_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(column->logical_type, &storage);
    const char *logical_type = column->logical_type;

    if (info == NULL || logical_type == NULL) {
        return "";
    }
    if (strncmp(logical_type, "BINARY(", binary_logical_prefix_length) == 0) {
        return "binary";
    }
    if (strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) == 0) {
        return "varbinary";
    }
    if (info->display_type == NULL) {
        return "";
    }
    return info->display_type;
}

static const char *information_schema_approximate_data_type_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK &&
        info.type_class == APPROXIMATE_TYPE_FLOAT) {
        return "float";
    }
    return "double";
}

static const char *information_schema_integer_data_type_for_descriptor(const char *logical_type) {
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        return "tinyint";
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        return "smallint";
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        return "mediumint";
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        return "bigint";
    }
    return "int";
}

static const char *information_schema_numeric_precision_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    const char *logical_type = column->logical_type;

    if (column_descriptor_is_bit(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_set(column) ||
        column_descriptor_is_json(column) || column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_spatial(column) || column_descriptor_is_decimal(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return NULL;
    }
    if (column_descriptor_is_approximate(column)) {
        struct approximate_type_info info = {
            .type_class = APPROXIMATE_TYPE_DOUBLE,
            .is_unsigned = false,
        };

        if (approximate_type_info_for_logical_type(column->logical_type, &info) == MYLITE_OK &&
            info.type_class == APPROXIMATE_TYPE_FLOAT) {
            return "12";
        }
        return "22";
    }
    if (logical_type == NULL) {
        return "10";
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        return "3";
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        return "5";
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        return "7";
    }
    if (strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        return "20";
    }
    if (strcmp(logical_type, "BIGINT") == 0) {
        return "19";
    }
    return "10";
}

static const char *information_schema_numeric_scale_for_descriptor(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_bit(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_enum(column) || column_descriptor_is_set(column) ||
        column_descriptor_is_json(column) || column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_spatial(column) || column_descriptor_is_decimal(column) ||
        column_descriptor_is_approximate(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return NULL;
    }
    return "0";
}

static int execute_checksum_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *table_names = child_at(statement, 0U);
    const size_t table_count =
        table_names == NULL ? 0U : mylite_sql_ast_node_child_count(table_names);
    struct table_maintenance_target *targets = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (table_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    targets = (struct table_maintenance_target *)calloc(table_count, sizeof(targets[0]));
    if (targets == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = resolve_table_maintenance_target(
            database,
            child_at(table_names, target_index),
            &targets[target_index]
        );
        if (rc == MYLITE_OK) {
            rc = check_table_maintenance_duplicate_target(
                database,
                targets,
                target_index,
                &targets[target_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_checksum_table_result_columns(database, result);
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = append_checksum_table_target_row(database, result, &targets[target_index]);
    }

    free(targets);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_checksum_table_result_columns(struct mylite_db *database, mylite_result *result) {
    const uint32_t table_collation_id = result_metadata_collation_id("latin1_swedish_ci");
    const struct mylite_result_column_descriptor descriptors[checksum_table_result_column_count] = {
        {
            .label = "Table",
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
            .flags = 0U,
            .charset_id = table_collation_id,
            .collation_id = table_collation_id,
            .display_length = checksum_table_name_display_length,
            .decimals = mysql_approximate_decimals,
            .nullable = true,
        },
        {
            .label = "Checksum",
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = checksum_table_checksum_display_length,
            .decimals = 0U,
            .nullable = true,
        },
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < checksum_table_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column_descriptor(result, &descriptors[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int append_checksum_table_target_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct table_maintenance_target *target
) {
    const char *const values[checksum_table_result_column_count] = {
        target == NULL ? NULL : target->display_name,
        NULL,
    };
    int rc = mylite_result_append_text_row(result, values);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    return append_checksum_table_target_warning(database, target);
}

static int append_checksum_table_target_warning(
    struct mylite_db *database,
    const struct table_maintenance_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (target == NULL) {
        set_runtime_error(database, "invalid checksum table target");
        return MYLITE_ERROR;
    }
    if (target->missing_schema) {
        written = snprintf(message, sizeof(message), "Unknown database '%s'", target->schema_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            set_runtime_error(database, "failed to format checksum schema diagnostic");
            return MYLITE_ERROR;
        }
        return append_checksum_table_warning(
            database,
            mysql_error_unknown_database,
            "42000",
            message
        );
    }
    if (target->missing_table) {
        written =
            snprintf(message, sizeof(message), "Table '%s' doesn't exist", target->display_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            set_runtime_error(database, "failed to format checksum table diagnostic");
            return MYLITE_ERROR;
        }
        return append_checksum_table_warning(
            database,
            mysql_error_table_does_not_exist,
            "42S02",
            message
        );
    }
    if (target->unsupported_kind) {
        written =
            snprintf(message, sizeof(message), "'%s' is not BASE TABLE", target->display_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            set_runtime_error(database, "failed to format checksum object-kind diagnostic");
            return MYLITE_ERROR;
        }
        return append_checksum_table_warning(database, mysql_error_not_view, "HY000", message);
    }

    return MYLITE_OK;
}

static int append_checksum_table_warning(
    struct mylite_db *database,
    int code,
    const char *sqlstate,
    const char *message
) {
    struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    const int rc = mylite_diagnostics_append_error(diagnostics, code, sqlstate, message);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int execute_table_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *table_names = child_at(statement, 0U);
    const enum table_maintenance_operation operation =
        table_maintenance_operation_for_statement(statement);
    const size_t table_count =
        table_names == NULL ? 0U : mylite_sql_ast_node_child_count(table_names);
    struct table_maintenance_target *targets = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (table_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    targets = (struct table_maintenance_target *)calloc(table_count, sizeof(targets[0]));
    if (targets == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = resolve_table_maintenance_target(
            database,
            child_at(table_names, target_index),
            &targets[target_index]
        );
        if (rc == MYLITE_OK) {
            rc = check_table_maintenance_duplicate_target(
                database,
                targets,
                target_index,
                &targets[target_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_table_maintenance_result_columns(database, result);
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = append_table_maintenance_target_rows(
            database,
            result,
            operation,
            &targets[target_index]
        );
    }

    free(targets);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static enum table_maintenance_operation table_maintenance_operation_for_statement(
    const struct mylite_sql_ast_node *statement
) {
    switch (statement->kind) {
    case MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_ANALYZE;
    case MYLITE_SQL_AST_CHECK_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_CHECK;
    case MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_OPTIMIZE;
    case MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_REPAIR;
    default:
        return TABLE_MAINTENANCE_CHECK;
    }
}

static int append_table_maintenance_result_columns(
    struct mylite_db *database,
    mylite_result *result
) {
    static const char *const result_columns[table_maintenance_result_column_count] = {
        "Table",
        "Op",
        "Msg_type",
        "Msg_text",
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < table_maintenance_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int append_table_maintenance_target_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    if (target->missing_schema) {
        return append_table_maintenance_unknown_schema_rows(database, result, operation, target);
    }
    if (target->missing_table || target->unsupported_kind) {
        return append_table_maintenance_unknown_table_rows(database, result, operation, target);
    }
    return append_table_maintenance_success_rows(database, result, operation, target);
}

static int append_table_maintenance_success_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    const char *operation_text = table_maintenance_operation_text(operation);
    int rc = MYLITE_OK;

    if (operation == TABLE_MAINTENANCE_OPTIMIZE) {
        rc = append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "note",
            "Table does not support optimize, doing recreate + analyze instead"
        );
        if (rc == MYLITE_OK) {
            rc = append_table_maintenance_row(
                database,
                result,
                target->display_name,
                operation_text,
                "status",
                "OK"
            );
        }
        return rc;
    }
    if (operation == TABLE_MAINTENANCE_REPAIR) {
        return append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "note",
            "The storage engine for the table doesn't support repair"
        );
    }

    return append_table_maintenance_row(
        database,
        result,
        target->display_name,
        operation_text,
        "status",
        "OK"
    );
}

static int append_table_maintenance_unknown_schema_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *operation_text = table_maintenance_operation_text(operation);
    int written = snprintf(message, sizeof(message), "Unknown database '%s'", target->schema_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = append_table_maintenance_row(
        database,
        result,
        target->display_name,
        operation_text,
        "Error",
        message
    );
    if (rc == MYLITE_OK) {
        rc = append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "error",
            "Corrupt"
        );
    }
    return rc;
}

static int append_table_maintenance_unknown_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *operation_text = table_maintenance_operation_text(operation);
    int written =
        snprintf(message, sizeof(message), "Table '%s' doesn't exist", target->display_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = append_table_maintenance_row(
        database,
        result,
        target->display_name,
        operation_text,
        "Error",
        message
    );
    if (rc == MYLITE_OK) {
        rc = append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "status",
            "Operation failed"
        );
    }
    return rc;
}

static int append_table_maintenance_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *table_name,
    const char *operation,
    const char *message_type,
    const char *message_text
) {
    const char *const values[table_maintenance_result_column_count] = {
        table_name,
        operation,
        message_type,
        message_text,
    };
    int rc = mylite_result_append_text_row(result, values);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int resolve_table_maintenance_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_maintenance_target *out_target
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    size_t part_count = 0U;
    bool schema_found = false;
    bool table_found = false;
    int rc = MYLITE_OK;

    *out_target = (struct table_maintenance_target){0};
    rc = collect_identifier_parts(node, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        if (database == NULL || !database->session.has_selected_schema) {
            set_no_database_error(database);
            return MYLITE_ERROR;
        }
        memcpy(
            out_target->schema_name,
            database->session.selected_schema,
            sizeof(out_target->schema_name)
        );
        memcpy(out_target->table_name, parts[0], sizeof(out_target->table_name));
    } else if (part_count == 2U) {
        memcpy(out_target->schema_name, parts[0], sizeof(out_target->schema_name));
        memcpy(out_target->table_name, parts[1], sizeof(out_target->table_name));
    } else {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = format_table_maintenance_display_name(database, out_target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(out_target->schema_name)) {
        out_target->missing_schema = true;
        return MYLITE_OK;
    }
    rc = mylite_catalog_try_read_schema_by_name(
        database,
        out_target->schema_name,
        &schema,
        &schema_found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        return rc;
    }
    if (!schema_found) {
        out_target->missing_schema = true;
        return MYLITE_OK;
    }
    if (mylite_catalog_name_is_reserved(out_target->table_name)) {
        out_target->missing_table = true;
        return MYLITE_OK;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        out_target->schema_name,
        out_target->table_name,
        &table,
        &table_found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (!table_found) {
        rc = mylite_catalog_try_read_table_by_name(
            database,
            schema.schema_id,
            out_target->table_name,
            &table,
            &table_found
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read table descriptor");
            return rc;
        }
    }
    if (!table_found) {
        out_target->missing_table = true;
        return MYLITE_OK;
    }
    if (table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        out_target->unsupported_kind = true;
    }
    return MYLITE_OK;
}

static int check_table_maintenance_duplicate_target(
    struct mylite_db *database,
    const struct table_maintenance_target *targets,
    size_t target_count,
    const struct table_maintenance_target *target
) {
    for (size_t index = 0U; index < target_count; ++index) {
        if (text_equals_ascii_case_insensitive(targets[index].schema_name, target->schema_name) &&
            text_equals_ascii_case_insensitive(targets[index].table_name, target->table_name)) {
            set_duplicate_table_alias_error(database, target->table_name);
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int format_table_maintenance_display_name(
    struct mylite_db *database,
    struct table_maintenance_target *target
) {
    int written = snprintf(
        target->display_name,
        sizeof(target->display_name),
        "%s.%s",
        target->schema_name,
        target->table_name
    );

    if (written < 0 || (size_t)written >= sizeof(target->display_name)) {
        set_identifier_too_long_error(database, "table");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static const char *table_maintenance_operation_text(enum table_maintenance_operation operation) {
    switch (operation) {
    case TABLE_MAINTENANCE_ANALYZE:
        return "analyze";
    case TABLE_MAINTENANCE_CHECK:
        return "check";
    case TABLE_MAINTENANCE_OPTIMIZE:
        return "optimize";
    case TABLE_MAINTENANCE_REPAIR:
        return "repair";
    }
    return "";
}

static int execute_show_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_schema_resolution schema_resolution = {0};
    struct show_tables_filter_nodes nodes = {0};
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    bool is_full = mylite_sql_ast_node_show_tables_is_full(statement) != 0;
    char *column_name = NULL;
    struct show_tables_context context = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = resolve_show_tables_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = resolve_show_statement_schema(database, nodes.schema, &schema_resolution);
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, nodes.like, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_tables_column_name(
            show_schema_resolution_name(&schema_resolution),
            &filter,
            &column_name
        );
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_tables_where_clause(database, nodes.where, column_name, is_full);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_tables_result_columns(database, result, column_name, is_full);
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        context.where_clause = nodes.where;
        context.table_name_column = column_name;
        context.is_full = is_full;
        if (schema_resolution.builtin_directory != NULL) {
            rc = append_show_builtin_schema_tables(schema_resolution.builtin_directory, &context);
        } else {
            rc = mylite_catalog_for_each_table_in_schema(
                database,
                schema_resolution.schema.schema_id,
                append_show_table,
                &context
            );
        }
        if (rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW TABLES result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        free(column_name);
        show_like_filter_deinit(&filter);
        return rc;
    }

    free(column_name);
    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int resolve_show_statement_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_node,
    struct show_schema_resolution *out_resolution
) {
    *out_resolution = (struct show_schema_resolution){0};
    if (schema_node == NULL) {
        return resolve_show_statement_selected_schema(database, out_resolution);
    }
    return resolve_show_statement_explicit_schema(database, schema_node, out_resolution);
}

static int resolve_show_statement_selected_schema(
    struct mylite_db *database,
    struct show_schema_resolution *out_resolution
) {
    if (database == NULL || !database->session.has_selected_schema) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    out_resolution->builtin_directory =
        find_builtin_schema_table_directory(database->session.selected_schema);
    if (out_resolution->builtin_directory != NULL) {
        return MYLITE_OK;
    }
    return resolve_selected_schema(database, &out_resolution->schema);
}

static int resolve_show_statement_explicit_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_node,
    struct show_schema_resolution *out_resolution
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        return MYLITE_ERROR;
    }

    out_resolution->builtin_directory = find_builtin_schema_table_directory(schema_name);
    if (out_resolution->builtin_directory != NULL) {
        return MYLITE_OK;
    }
    return resolve_schema_name(database, schema_name, &out_resolution->schema);
}

static const char *show_schema_resolution_name(const struct show_schema_resolution *resolution) {
    if (resolution->builtin_directory != NULL) {
        return resolution->builtin_directory->schema_name;
    }
    return resolution->schema.name;
}

static int append_show_tables_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *column_name,
    bool is_full
) {
    int rc = mylite_result_append_column(result, column_name);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (!is_full) {
        return MYLITE_OK;
    }

    rc = mylite_result_append_column(result, "Table_type");
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int resolve_show_tables_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_tables_filter_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *filter_node = second_child;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct show_tables_filter_nodes){0};
    if (first_child != NULL && (first_child->kind == MYLITE_SQL_AST_LITERAL ||
                                first_child->kind == MYLITE_SQL_AST_WHERE_CLAUSE)) {
        schema_node = NULL;
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        out_nodes->like = filter_node;
    } else if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        out_nodes->where = filter_node;
    } else if (filter_node != NULL) {
        set_runtime_error(database, "invalid SHOW TABLES filter");
        return MYLITE_ERROR;
    }

    out_nodes->schema = schema_node;
    return MYLITE_OK;
}

static int execute_show_table_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_schema_resolution schema_resolution = {0};
    struct show_table_status_filter_nodes nodes = {0};
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_table_status_context context = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = resolve_show_table_status_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = resolve_show_statement_schema(database, nodes.schema, &schema_resolution);
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, nodes.like, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_table_status_where_clause(database, nodes.where);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_table_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, show_table_status_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        context.where_clause = nodes.where;
        if (schema_resolution.builtin_directory != NULL) {
            rc = append_show_builtin_schema_table_status(
                schema_resolution.builtin_directory,
                &context
            );
        } else {
            rc = mylite_catalog_for_each_table_in_schema(
                database,
                schema_resolution.schema.schema_id,
                append_show_table_status,
                &context
            );
        }
        if (rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW TABLE STATUS result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int resolve_show_table_status_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_table_status_filter_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *filter_node = second_child;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct show_table_status_filter_nodes){0};
    if (first_child != NULL && (first_child->kind == MYLITE_SQL_AST_LITERAL ||
                                first_child->kind == MYLITE_SQL_AST_WHERE_CLAUSE)) {
        schema_node = NULL;
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        out_nodes->like = filter_node;
    } else if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        out_nodes->where = filter_node;
    } else if (filter_node != NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS filter");
        return MYLITE_ERROR;
    }

    out_nodes->schema = schema_node;
    return MYLITE_OK;
}

static int execute_show_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_character_set_result_column_count] = {
        "Charset",
        "Description",
        "Default collation",
        "Maxlen",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_character_set_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_character_set_count();
         ++row_index) {
        const struct mylite_execution_catalog_character_set *charset =
            mylite_execution_catalog_mysql_character_set_at(row_index);
        const char *const values[show_character_set_result_column_count] = {
            charset->name,
            charset->description,
            charset->default_collation,
            charset->maxlen,
        };

        if (show_like_filter_matches(&filter, values[0], false)) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_collation_result_column_count] = {
        "Collation",
        "Charset",
        "Id",
        "Default",
        "Compiled",
        "Sortlen",
        "Pad_attribute",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_collation_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *const values[show_collation_result_column_count] = {
            collation->name,
            collation->character_set,
            collation->id,
            collation->is_default,
            collation->compiled,
            collation->sortlen,
            collation->pad_attribute,
        };

        if (show_like_filter_matches(&filter, values[0], false)) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_variables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *filter_node = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        filter_node = child_at(statement, 1U);
    } else {
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else {
        like_pattern = filter_node;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(result_columns) / sizeof(result_columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_system_variable_descriptor_count();
         ++index) {
        rc = append_show_variable(
            database,
            result,
            &filter,
            where_clause,
            global_scope,
            mylite_execution_system_variable_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        like_pattern = child_at(statement, 1U);
    } else {
        like_pattern = first_child;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(result_columns) / sizeof(result_columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        rc = append_show_status(
            database,
            result,
            &filter,
            global_scope,
            mylite_execution_show_status_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int append_show_status(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    bool global_scope,
    const struct mylite_execution_show_status_descriptor *descriptor
) {
    const char *values[2] = {NULL, NULL};
    int rc = MYLITE_OK;

    if (!show_status_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    values[1] = descriptor->value;
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static bool show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}

static bool show_variables_scope_is_global(const struct mylite_sql_ast_node *scope) {
    static const char expected[] = "global";

    if (scope == NULL) {
        return false;
    }
    if (scope->span.text == NULL || scope->span.length != sizeof(expected) - 1U) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(expected) - 1U; ++index) {
        if (ascii_lower((unsigned char)scope->span.text[index]) != expected[index]) {
            return false;
        }
    }
    return true;
}

static int append_show_variable(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_system_variable_descriptor *descriptor
) {
    char integer_buffer[integer_text_capacity];
    const char *values[2] = {NULL, NULL};
    struct show_variables_where_row row = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (!show_variable_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    rc = show_system_variable_value(
        database,
        descriptor->kind,
        global_scope,
        integer_buffer,
        sizeof(integer_buffer),
        &values[1]
    );
    if (rc == MYLITE_OK && where_clause != NULL) {
        row.variable_name = values[0];
        row.value = values[1];
        rc = show_variables_where_clause_matches(database, where_clause, &row, &where_matches);
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static bool show_variable_descriptor_visible(
    const struct mylite_execution_system_variable_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}

static int show_variables_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_variables_where_row *row,
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE || row == NULL ||
        out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_variables_where_predicate(database, child_at(where_clause, 0U), row, &truth);
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_variables_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || row == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_variables_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_variables_where_frame(database, current, row, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_variables_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
) {
    int rc = show_variables_where_frame_stack_push(
        database,
        frame_stack,
        (struct show_variables_where_eval_frame){
            .node = predicate,
            .action = SHOW_VARIABLES_WHERE_EVALUATE,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 0U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
    }
    if (predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE) {
        rc = show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 1U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
        if (rc == MYLITE_OK) {
            rc = show_variables_where_frame_stack_push(
                database,
                frame_stack,
                (struct show_variables_where_eval_frame){
                    .node = child_at(predicate, 0U),
                    .action = SHOW_VARIABLES_WHERE_VISIT,
                }
            );
        }
    }
    return rc;
}

static int evaluate_show_variables_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_variables_where_comparison_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_variables_where_in_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_variables_where_is_null_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW VARIABLES WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW VARIABLES WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int evaluate_show_variables_where_logical_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    struct show_variables_where_truth_stack *truth_stack,
    enum show_variables_where_truth *out_truth
) {
    enum show_variables_where_truth left = SHOW_VARIABLES_WHERE_FALSE;
    enum show_variables_where_truth right = SHOW_VARIABLES_WHERE_FALSE;

    if (!show_variables_where_truth_stack_pop(truth_stack, &right) ||
        !show_variables_where_truth_stack_pop(truth_stack, &left)) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate stack");
        return MYLITE_ERROR;
    }

    if (predicate->kind == MYLITE_SQL_AST_AND_PREDICATE) {
        *out_truth = and_show_variables_where_truth(left, right);
    } else {
        *out_truth = or_show_variables_where_truth(left, right);
    }
    return MYLITE_OK;
}

static int evaluate_show_variables_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    const char *left = NULL;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_variables_where_column_value(database, left_node, row, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) {
        set_unsupported_error(database, "SHOW VARIABLES WHERE does not support REGEXP predicates");
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        char *pattern = NULL;

        rc = decode_show_variables_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            bool matches =
                show_like_pattern_matches(pattern, strlen(pattern), left, strlen(left), false);

            if (matches) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            } else {
                *out_truth = SHOW_VARIABLES_WHERE_FALSE;
            }
        }
        free(pattern);
        return rc;
    }

    return compare_show_variables_where_literal(
        database,
        operator_kind,
        left,
        right_node,
        out_truth
    );
}

static int evaluate_show_variables_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const char *left = NULL;
    size_t value_count = 0U;
    bool saw_null = false;
    int rc = show_variables_where_column_value(database, left_node, row, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE IN list");
        return MYLITE_ERROR;
    }

    value_count = mylite_sql_ast_node_child_count(value_list);
    for (size_t index = 0U; index < value_count; ++index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, index);
        enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
        char *right = NULL;

        if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
            set_unsupported_error(
                database,
                "SHOW VARIABLES WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }

        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            saw_null = true;
            continue;
        }
        rc = decode_show_variables_where_string_literal(database, value_node, &right);
        if (rc == MYLITE_OK && compare_ascii_case_insensitive_text(left, right) == 0) {
            *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            free(right);
            return MYLITE_OK;
        }
        free(right);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (saw_null) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
    } else {
        *out_truth = SHOW_VARIABLES_WHERE_FALSE;
    }
    return MYLITE_OK;
}

static int evaluate_show_variables_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    int rc = show_variables_where_column_value(database, child_at(predicate, 0U), row, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_truth = mylite_sql_ast_node_operator(predicate) == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL
                     ? SHOW_VARIABLES_WHERE_TRUE
                     : SHOW_VARIABLES_WHERE_FALSE;
    return MYLITE_OK;
}

static enum show_variables_where_truth negate_show_variables_where_truth(
    enum show_variables_where_truth truth
) {
    switch (truth) {
    case SHOW_VARIABLES_WHERE_TRUE:
        return SHOW_VARIABLES_WHERE_FALSE;
    case SHOW_VARIABLES_WHERE_FALSE:
        return SHOW_VARIABLES_WHERE_TRUE;
    case SHOW_VARIABLES_WHERE_UNKNOWN:
    default:
        return SHOW_VARIABLES_WHERE_UNKNOWN;
    }
}

static enum show_variables_where_truth and_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
) {
    if (left == SHOW_VARIABLES_WHERE_FALSE || right == SHOW_VARIABLES_WHERE_FALSE) {
        return SHOW_VARIABLES_WHERE_FALSE;
    }
    if (left == SHOW_VARIABLES_WHERE_TRUE && right == SHOW_VARIABLES_WHERE_TRUE) {
        return SHOW_VARIABLES_WHERE_TRUE;
    }
    return SHOW_VARIABLES_WHERE_UNKNOWN;
}

static enum show_variables_where_truth or_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
) {
    if (left == SHOW_VARIABLES_WHERE_TRUE || right == SHOW_VARIABLES_WHERE_TRUE) {
        return SHOW_VARIABLES_WHERE_TRUE;
    }
    if (left == SHOW_VARIABLES_WHERE_FALSE && right == SHOW_VARIABLES_WHERE_FALSE) {
        return SHOW_VARIABLES_WHERE_FALSE;
    }
    return SHOW_VARIABLES_WHERE_UNKNOWN;
}

static int show_variables_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct show_variables_where_row *row,
    const char **out_value
) {
    enum show_variables_where_column column = SHOW_VARIABLES_WHERE_COLUMN_NONE;
    int rc = resolve_show_variables_where_column(database, column_node, &column);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (row == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE row");
        return MYLITE_ERROR;
    }

    switch (column) {
    case SHOW_VARIABLES_WHERE_COLUMN_VARIABLE_NAME:
        *out_value = row->variable_name;
        return MYLITE_OK;
    case SHOW_VARIABLES_WHERE_COLUMN_VALUE:
        *out_value = row->value;
        return MYLITE_OK;
    case SHOW_VARIABLES_WHERE_COLUMN_NONE:
    default:
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE column");
        return MYLITE_ERROR;
    }
}

static int resolve_show_variables_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    enum show_variables_where_column *out_column
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column = SHOW_VARIABLES_WHERE_COLUMN_NONE;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "SHOW VARIABLES WHERE supports only output columns");
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U && text_equals_ascii_case_insensitive(parts[0], "Variable_name")) {
        *out_column = SHOW_VARIABLES_WHERE_COLUMN_VARIABLE_NAME;
        return MYLITE_OK;
    }
    if (part_count == 1U && text_equals_ascii_case_insensitive(parts[0], "Value")) {
        *out_column = SHOW_VARIABLES_WHERE_COLUMN_VALUE;
        return MYLITE_OK;
    }

    rc = show_variables_column_reference_text(database, column_node, &display_text);
    if (rc == MYLITE_OK) {
        set_unknown_where_column_error(database, display_text);
        rc = MYLITE_ERROR;
    }
    free(display_text);
    return rc;
}

static int show_variables_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_dynamic_string string;
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (rc != MYLITE_OK) {
        return rc;
    }

    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, parts[part_index]);
        }
    }
    if (rc == MYLITE_OK) {
        *out_text = mylite_dynamic_string_take(&string);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int compare_show_variables_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *right_text = NULL;
    int comparison = 0;
    int rc = MYLITE_OK;

    if (left == NULL || right == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW VARIABLES WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_truth = operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL
                         ? SHOW_VARIABLES_WHERE_FALSE
                         : SHOW_VARIABLES_WHERE_UNKNOWN;
        return MYLITE_OK;
    }

    rc = decode_show_variables_where_string_literal(database, right, &right_text);
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }

    comparison = compare_ascii_case_insensitive_text(left, right_text);
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        *out_truth = comparison == 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        *out_truth = comparison != 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        *out_truth = comparison < 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        *out_truth = comparison <= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        *out_truth = comparison > 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        *out_truth = comparison >= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    default:
        set_unsupported_error(
            database,
            "SHOW VARIABLES WHERE comparison operator is not supported"
        );
        rc = MYLITE_ERROR;
        break;
    }

    free(right_text);
    return rc;
}

static int decode_show_variables_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW VARIABLES WHERE supports only string literal predicates",
        "SHOW VARIABLES WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}

static void show_variables_where_frame_stack_deinit(
    struct show_variables_where_frame_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct show_variables_where_frame_stack){0};
}

static int show_variables_where_frame_stack_push(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame frame
) {
    struct show_variables_where_eval_frame *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = frame;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static bool show_variables_where_frame_stack_pop(
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame *out_frame
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_frame = stack->items[stack->count];
    return true;
}

static void show_variables_where_truth_stack_deinit(
    struct show_variables_where_truth_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct show_variables_where_truth_stack){0};
}

static int show_variables_where_truth_stack_push(
    struct mylite_db *database,
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth truth
) {
    enum show_variables_where_truth *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = truth;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = truth;
    ++stack->count;
    return MYLITE_OK;
}

static bool show_variables_where_truth_stack_pop(
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth *out_truth
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_truth = stack->items[stack->count];
    return true;
}

static int execute_show_triggers_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_triggers_result_column_count] = {
        "Trigger",
        "Event",
        "Table",
        "Statement",
        "Timing",
        "Created",
        "sql_mode",
        "Definer",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node == NULL) {
        rc = resolve_show_triggers_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = resolve_show_triggers_schema_name(database, schema_name, &schema);
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_triggers_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_sys_sys_config_trigger_rows(database, result, &schema, &filter);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int resolve_show_triggers_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    if (database == NULL || !database->session.has_selected_schema) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    return resolve_show_triggers_schema_name(
        database,
        database->session.selected_schema,
        out_schema
    );
}

static int resolve_show_triggers_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    const struct mylite_execution_catalog_builtin_schema *builtin_schema =
        find_builtin_schema_descriptor(schema_name);

    if (builtin_schema != NULL) {
        int name_written = 0;
        int charset_written = 0;
        int collation_written = 0;

        *out_schema = (struct mylite_catalog_schema_descriptor){0};
        name_written =
            snprintf(out_schema->name, sizeof(out_schema->name), "%s", builtin_schema->name);
        charset_written = snprintf(
            out_schema->default_charset,
            sizeof(out_schema->default_charset),
            "%s",
            builtin_schema->default_charset
        );
        collation_written = snprintf(
            out_schema->default_collation,
            sizeof(out_schema->default_collation),
            "%s",
            builtin_schema->default_collation
        );

        if (name_written < 0 || (size_t)name_written >= sizeof(out_schema->name) ||
            charset_written < 0 || (size_t)charset_written >= sizeof(out_schema->default_charset) ||
            collation_written < 0 ||
            (size_t)collation_written >= sizeof(out_schema->default_collation)) {
            set_runtime_error(database, "failed to format builtin schema metadata");
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    return resolve_schema_name(database, schema_name, out_schema);
}

static int append_show_sys_sys_config_trigger_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct show_like_filter *filter
) {
    char created[timestamp_2_text_length + 1U];
    int rc = MYLITE_OK;

    if (schema == NULL || strcmp(schema->name, "sys") != 0 ||
        !show_like_filter_matches(filter, "sys_config", false)) {
        return MYLITE_OK;
    }

    rc = mysql_system_current_timestamp2(database, created, sizeof(created));
    for (size_t trigger_index = 0U;
         rc == MYLITE_OK && trigger_index < mylite_execution_catalog_sys_sys_config_trigger_count();
         ++trigger_index) {
        const struct mylite_execution_catalog_sys_config_trigger *trigger =
            mylite_execution_catalog_sys_sys_config_trigger_at(trigger_index);
        const char *values[show_triggers_result_column_count] = {
            trigger->name,
            trigger->event,
            "sys_config",
            mylite_execution_catalog_sys_sys_config_trigger_action_statement(),
            "BEFORE",
            created,
            mylite_execution_catalog_sys_sys_config_trigger_sql_mode(),
            "mysql.sys@localhost",
            "utf8mb4",
            "utf8mb4_0900_ai_ci",
            "utf8mb4_0900_ai_ci",
        };

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int execute_show_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_events_result_column_count] = {
        "Db",
        "Name",
        "Definer",
        "Time zone",
        "Type",
        "Execute at",
        "Interval value",
        "Interval field",
        "Starts",
        "Ends",
        "Status",
        "Originator",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node == NULL) {
        struct mylite_catalog_schema_descriptor schema = {0};

        rc = resolve_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_events_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_open_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_open_tables_result_column_count] = {
        "Database",
        "Table",
        "In_use",
        "Name_locked",
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node != NULL) {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_open_tables_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_routine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_routine_status_result_column_count] = {
        "Db",
        "Name",
        "Type",
        "Language",
        "Definer",
        "Modified",
        "Created",
        "Security_type",
        "Comment",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const struct mylite_sql_ast_node *like_node = child_at(statement, 0U);
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, like_node, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_routine_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_processlist_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_processlist_result_column_count] = {
        "Id",
        "User",
        "Host",
        "db",
        "Command",
        "Time",
        "State",
        "Info",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_processlist_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_processlist_row(database, context, statement, result);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_processlist_warning(database);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_show_processlist_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
) {
    char connection_id_text[integer_text_capacity];
    char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char *info = NULL;
    const char *selected_schema = NULL;
    int written = 0;
    int rc = MYLITE_OK;

    const char *values[show_processlist_result_column_count] = {
        connection_id_text,
        user,
        host,
        NULL,
        "Query",
        "0",
        "init",
        NULL,
    };

    written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        database->session.connection_id
    );
    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        set_runtime_error(database, "failed to format SHOW PROCESSLIST Id");
        return MYLITE_ERROR;
    }
    if (database->session.has_selected_schema) {
        selected_schema = database->session.selected_schema;
    }
    values[show_processlist_db_column] = selected_schema;

    rc = format_show_processlist_user_host(database, user, sizeof(user), host, sizeof(host));
    if (rc == MYLITE_OK) {
        rc = copy_show_processlist_info(database, context, statement, &info);
    }
    if (rc == MYLITE_OK) {
        values[show_processlist_info_column] = info;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(info);
    return rc;
}

static int format_show_processlist_user_host(
    struct mylite_db *database,
    char *user,
    size_t user_size,
    char *host,
    size_t host_size
) {
    const char *identity = database->session.client_user_identity;
    const char *at_sign = strchr(identity, '@');
    const char *host_part = at_sign == NULL ? "" : at_sign + 1;
    size_t user_length = at_sign == NULL ? strlen(identity) : (size_t)(at_sign - identity);
    size_t host_length = strlen(host_part);

    if (user_length >= user_size || host_length >= host_size) {
        set_runtime_error(database, "failed to format SHOW PROCESSLIST user identity");
        return MYLITE_ERROR;
    }

    memcpy(user, identity, user_length);
    user[user_length] = '\0';
    memcpy(host, host_part, host_length);
    host[host_length] = '\0';

    return MYLITE_OK;
}

static int copy_show_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    char **out_info
) {
    const char *sql = mylite_statement_context_sql(context);
    size_t sql_size = mylite_statement_context_sql_size(context);
    size_t length = 0U;
    char *info = NULL;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;
    if (sql == NULL) {
        set_runtime_error(database, "failed to read SHOW PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    length = statement_info_length_without_terminator(sql, sql_size);
    if (statement->kind != MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT &&
        length > show_processlist_info_truncation_length) {
        length = show_processlist_info_truncation_length;
    }
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    info = (char *)malloc(length + 1U);
    if (info == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length > 0U) {
        memcpy(info, sql, length);
    }
    info[length] = '\0';
    *out_info = info;

    return MYLITE_OK;
}

static size_t statement_info_length_without_terminator(const char *sql, size_t sql_size) {
    size_t length = sql_size;

    while (length > 0U &&
           (sql[length - 1U] == ' ' || sql[length - 1U] == '\t' || sql[length - 1U] == '\n' ||
            sql[length - 1U] == '\r' || sql[length - 1U] == '\f' || sql[length - 1U] == '\v')) {
        --length;
    }
    if (length > 0U && sql[length - 1U] == ';') {
        --length;
    }
    while (length > 0U &&
           (sql[length - 1U] == ' ' || sql[length - 1U] == '\t' || sql[length - 1U] == '\n' ||
            sql[length - 1U] == '\r' || sql[length - 1U] == '\f' || sql[length - 1U] == '\v')) {
        --length;
    }

    return length;
}

static int append_show_processlist_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_information_schema_processlist_deprecated,
        "HY000",
        "'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future "
        "release. Please use performance_schema.processlist instead"
    );
}

static int append_information_schema_profiling_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_information_schema_profiling_deprecated,
        "HY000",
        "'INFORMATION_SCHEMA.PROFILING' is deprecated and will be removed in a future release. "
        "Please use Performance Schema instead"
    );
}

static int execute_show_grants_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_grants_result_column_count] = {
        "Grants for root@%",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_grants_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(show_grants_embedded_root_rows) /
                                            sizeof(show_grants_embedded_root_rows[0]);
         ++row_index) {
        const char *const values[show_grants_result_column_count] = {
            show_grants_embedded_root_rows[row_index],
        };

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_privileges_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_privileges_result_column_count] = {
        "Privilege",
        "Context",
        "Comment",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_privileges_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < sizeof(show_privileges_rows) /
                                                                   sizeof(show_privileges_rows[0]);
         ++row_index) {
        const char *const values[show_privileges_result_column_count] = {
            show_privileges_rows[row_index].privilege,
            show_privileges_rows[row_index].context,
            show_privileges_rows[row_index].comment,
        };

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_binary_log_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_binary_log_status_result_column_count] = {
        "File",
        "Position",
        "Binlog_Do_DB",
        "Binlog_Ignore_DB",
        "Executed_Gtid_Set",
    };
    static const char *const values[show_binary_log_status_result_column_count] = {
        show_binary_log_file_name,
        show_binary_log_placeholder_position,
        "",
        "",
        "",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_binary_log_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_binary_logs_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_binary_logs_result_column_count] = {
        "Log_name",
        "File_size",
        "Encrypted",
    };
    static const char *const values[show_binary_logs_result_column_count] = {
        show_binary_log_file_name,
        show_binary_log_placeholder_file_size,
        "No",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_binary_logs_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_replica_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_replica_status_result_column_count] = {
        "Replica_IO_State",
        "Source_Host",
        "Source_User",
        "Source_Port",
        "Connect_Retry",
        "Source_Log_File",
        "Read_Source_Log_Pos",
        "Relay_Log_File",
        "Relay_Log_Pos",
        "Relay_Source_Log_File",
        "Replica_IO_Running",
        "Replica_SQL_Running",
        "Replicate_Do_DB",
        "Replicate_Ignore_DB",
        "Replicate_Do_Table",
        "Replicate_Ignore_Table",
        "Replicate_Wild_Do_Table",
        "Replicate_Wild_Ignore_Table",
        "Last_Errno",
        "Last_Error",
        "Skip_Counter",
        "Exec_Source_Log_Pos",
        "Relay_Log_Space",
        "Until_Condition",
        "Until_Log_File",
        "Until_Log_Pos",
        "Source_SSL_Allowed",
        "Source_SSL_CA_File",
        "Source_SSL_CA_Path",
        "Source_SSL_Cert",
        "Source_SSL_Cipher",
        "Source_SSL_Key",
        "Seconds_Behind_Source",
        "Source_SSL_Verify_Server_Cert",
        "Last_IO_Errno",
        "Last_IO_Error",
        "Last_SQL_Errno",
        "Last_SQL_Error",
        "Replicate_Ignore_Server_Ids",
        "Source_Server_Id",
        "Source_UUID",
        "Source_Info_File",
        "SQL_Delay",
        "SQL_Remaining_Delay",
        "Replica_SQL_Running_State",
        "Source_Retry_Count",
        "Source_Bind",
        "Last_IO_Error_Timestamp",
        "Last_SQL_Error_Timestamp",
        "Source_SSL_Crl",
        "Source_SSL_Crlpath",
        "Retrieved_Gtid_Set",
        "Executed_Gtid_Set",
        "Auto_Position",
        "Replicate_Rewrite_DB",
        "Channel_Name",
        "Source_TLS_Version",
        "Source_public_key_path",
        "Get_Source_public_key",
        "Network_Namespace",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_replica_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_replicas_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_replicas_result_column_count] = {
        "Server_Id",
        "Host",
        "Port",
        "Source_Id",
        "Replica_UUID",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_replicas_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_warnings_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_warnings_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_diagnostics_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_warnings_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_warnings_result_column_count] = {
        "@@session.warning_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_warnings_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_errors_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_errors_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_errors_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_errors_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_errors_result_column_count] = {
        "@@session.error_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_errors_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int plan_diagnostics_show_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_diagnostics_show_limit *out_limit
) {
    int rc = MYLITE_OK;

    if (out_limit == NULL) {
        set_runtime_error(database, "invalid diagnostics SHOW LIMIT plan");
        return MYLITE_ERROR;
    }

    out_limit->has_limit = false;
    out_limit->row_count = UINT64_MAX;
    out_limit->has_offset = false;
    out_limit->offset = 0U;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "diagnostics SHOW supports only literal LIMIT clauses");
        return MYLITE_ERROR;
    }

    rc = convert_diagnostics_show_limit_integer_literal(
        database,
        child_at(limit_clause, 0U),
        &out_limit->row_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    if (child_at(limit_clause, 1U) != NULL) {
        rc = convert_diagnostics_show_limit_integer_literal(
            database,
            child_at(limit_clause, 1U),
            &out_limit->offset
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int convert_diagnostics_show_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "diagnostics SHOW supports only unsigned decimal integer LIMIT literals"
        );
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&value_node->span, &value);
    if (rc != MYLITE_OK) {
        set_limit_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_value = value;
    return MYLITE_OK;
}

static int append_show_diagnostics_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t total_count = 0U;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t emitted_count = 0U;
    uint64_t condition_count = 0U;
    uint64_t warning_count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (rc == MYLITE_OK) {
        warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
        total_count = condition_count + warning_count;
    }

    for (uint64_t index = start_index;
         rc == MYLITE_OK && index < total_count && emitted_count < row_count;
         ++index, ++emitted_count) {
        if (index < warning_count) {
            const struct mylite_diagnostic_record *warning =
                mylite_diagnostics_warning_at(diagnostics, (size_t)index);
            if (warning == NULL) {
                set_runtime_error(database, "invalid previous diagnostics warning index");
                rc = MYLITE_ERROR;
            } else {
                rc = append_show_diagnostics_row(database, result, warning->level, warning);
            }
        } else if (condition_count != 0U) {
            rc = append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
        }
    }

    return rc;
}

static int append_show_diagnostics_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *level,
    const struct mylite_diagnostic_record *record
) {
    char code_text[integer_text_capacity];
    const char *values[show_warnings_result_column_count] = {
        level,
        code_text,
        record == NULL ? NULL : record->message,
    };
    int written = 0;
    int rc = MYLITE_OK;

    if (record == NULL) {
        set_runtime_error(database, "invalid diagnostics record");
        return MYLITE_ERROR;
    }

    written = snprintf(code_text, sizeof(code_text), "%d", record->code);
    if (written < 0 || (size_t)written >= sizeof(code_text)) {
        set_runtime_error(database, "failed to format diagnostic code");
        return MYLITE_ERROR;
    }

    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_show_count_warnings_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_warnings_result_column_count] = {count_text};
    uint64_t condition_count = 0U;
    uint64_t count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (rc != MYLITE_OK) {
        return rc;
    }

    count = condition_count + (uint64_t)mylite_diagnostics_warning_count(diagnostics);
    rc = format_uint64(database, count, count_text, sizeof(count_text));
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_show_errors_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t error_index = 0U;
    uint64_t emitted_count = 0U;
    int rc = MYLITE_OK;

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (row_count == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_diagnostics_warning_count(diagnostics);
         ++index) {
        const struct mylite_diagnostic_record *record =
            mylite_diagnostics_warning_at(diagnostics, index);

        if (!diagnostic_record_level_is_error(record)) {
            continue;
        }
        if (error_index >= start_index && emitted_count < row_count) {
            rc = append_show_diagnostics_row(database, result, "Error", record);
            ++emitted_count;
        }
        ++error_index;
    }
    if (rc == MYLITE_OK && diagnostics_has_error_condition(diagnostics) &&
        error_index >= start_index && emitted_count < row_count) {
        rc = append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
    }
    return rc;
}

static int append_show_count_errors_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_errors_result_column_count] = {count_text};
    uint64_t count = 0U;
    int rc = previous_diagnostics_error_count(diagnostics, &count);

    if (rc == MYLITE_OK) {
        rc = format_uint64(database, count, count_text, sizeof(count_text));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    uint64_t warning_count = 0U;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
    if (diagnostics_has_error_condition(diagnostics) && warning_count == UINT64_MAX) {
        return MYLITE_NOMEM;
    }

    *out_count = 0U;
    if (diagnostics_has_error_condition(diagnostics)) {
        *out_count = 1U;
    }
    return MYLITE_OK;
}

static bool diagnostics_has_error_condition(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return false;
    }
    if (mylite_diagnostics_errcode(diagnostics) == MYLITE_OK) {
        return false;
    }
    return true;
}

static int previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    uint64_t count = 0U;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < mylite_diagnostics_warning_count(diagnostics); ++index) {
        if (diagnostic_record_level_is_error(mylite_diagnostics_warning_at(diagnostics, index))) {
            if (count == UINT64_MAX) {
                return MYLITE_NOMEM;
            }
            ++count;
        }
    }
    if (diagnostics_has_error_condition(diagnostics)) {
        if (count == UINT64_MAX) {
            return MYLITE_NOMEM;
        }
        ++count;
    }

    *out_count = count;
    return MYLITE_OK;
}

static bool diagnostic_record_level_is_error(const struct mylite_diagnostic_record *record) {
    if (record == NULL) {
        return false;
    }
    if (strcmp(record->level, "Error") != 0) {
        return false;
    }
    return true;
}

static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRIu64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format unsigned integer");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_show_columns_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_columns_filter_nodes nodes = {0};
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    const struct mylite_execution_catalog_mysql_system_table *mysql_system_definition = NULL;
    struct primary_key_info primary_key = primary_key_info_init();
    size_t column_count = 0U;
    bool missing_schema = false;
    bool mysql_system_target = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_columns_context context = {0};
    mylite_result *result = NULL;
    bool full = statement->kind == MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT;
    int rc = MYLITE_OK;

    rc = resolve_show_columns_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = resolve_show_columns_mysql_system_table(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            },
            &mysql_system_definition,
            &mysql_system_target
        );
    }
    if (rc == MYLITE_OK && mysql_system_definition != NULL) {
        return execute_show_columns_mysql_system_table_statement(
            database,
            &nodes,
            mysql_system_definition,
            full,
            out_result
        );
    }
    if (rc == MYLITE_OK && mysql_system_target) {
        set_unsupported_error(database, "SHOW COLUMNS supports selected mysql system tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_unknown_show_columns_information_schema_table(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_show_columns_table_name(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            },
            &target,
            &missing_schema
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_metadata_table_reference(database, &target, missing_schema, &table);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_show_columns_key_metadata(
            database,
            &table,
            columns,
            column_count,
            &primary_key,
            &indexes,
            &context.index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, nodes.like, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_columns_where_clause(database, nodes.where, full);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_columns_result_columns(database, result, full);
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        context.where_clause = nodes.where;
        context.table = &table;
        context.primary_key = &primary_key;
        context.indexes = indexes;
        context.full = full;
        for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count;
             ++column_index) {
            rc = append_show_column(&columns[column_index], &context);
        }
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (
            rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
        ) {
            set_runtime_error(database, "failed to build SHOW COLUMNS result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        primary_key_info_deinit(&primary_key);
        loaded_index_infos_deinit(&indexes, &context.index_count);
        free(columns);
        return rc;
    }

    show_like_filter_deinit(&filter);
    primary_key_info_deinit(&primary_key);
    loaded_index_infos_deinit(&indexes, &context.index_count);
    free(columns);
    return finish_successful_result(database, result, out_result);
}

static int resolve_show_columns_mysql_system_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    const struct mylite_execution_catalog_mysql_system_table **out_definition,
    bool *out_mysql_system_target
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_target = false;
    int rc = MYLITE_OK;

    if (out_definition == NULL || out_mysql_system_target == NULL) {
        return MYLITE_MISUSE;
    }
    *out_definition = NULL;
    *out_mysql_system_target = false;

    rc = copy_show_columns_target_schema_and_table(
        database,
        nodes,
        schema_name,
        table_name,
        &has_target
    );
    if (rc != MYLITE_OK || !has_target) {
        return rc;
    }

    *out_definition = find_mysql_system_table_definition(schema_name, table_name);
    if (*out_definition != NULL) {
        *out_mysql_system_target = true;
        return MYLITE_OK;
    }
    if (!schema_name_is_mysql_system_schema(schema_name)) {
        return MYLITE_OK;
    }

    *out_mysql_system_target = true;
    if (mylite_catalog_name_is_reserved(table_name)) {
        set_reserved_name_error(database, "table", table_name);
        return MYLITE_ERROR;
    }
    if (mysql_data_dictionary_table_is_hidden(table_name)) {
        set_mysql_data_dictionary_table_access_error(database, table_name);
        return MYLITE_ERROR;
    }

    if (mysql_system_table_directory_contains(table_name)) {
        return MYLITE_OK;
    }

    set_table_does_not_exist_error(database, schema_name, table_name);
    return MYLITE_ERROR;
}

static int reject_unknown_show_columns_information_schema_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_target = false;
    int rc = copy_show_columns_target_schema_and_table(
        database,
        nodes,
        schema_name,
        table_name,
        &has_target
    );

    if (rc != MYLITE_OK || !has_target || !schema_name_is_information_schema(schema_name)) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(table_name)) {
        set_reserved_name_error(database, "table", table_name);
        return MYLITE_ERROR;
    }
    if (find_information_schema_table_definition(table_name) != NULL) {
        return MYLITE_OK;
    }

    set_unknown_information_schema_table_error(database, table_name);
    return MYLITE_ERROR;
}

static int copy_show_columns_target_schema_and_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    char *schema_name,
    char *table_name,
    bool *out_has_target
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (schema_name == NULL || table_name == NULL || out_has_target == NULL) {
        return MYLITE_MISUSE;
    }
    *out_has_target = false;
    memset(schema_name, 0, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    memset(table_name, 0, MYLITE_CATALOG_IDENTIFIER_CAPACITY);

    if (nodes.schema != NULL) {
        rc = copy_identifier_text(
            nodes.schema,
            schema_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY,
            database
        );
        if (rc == MYLITE_OK) {
            rc = copy_show_columns_explicit_table_name(
                database,
                nodes.table,
                table_name,
                MYLITE_CATALOG_IDENTIFIER_CAPACITY
            );
        }
        if (rc == MYLITE_OK) {
            *out_has_target = true;
        }
        return rc;
    }

    rc = collect_identifier_parts(
        nodes.table,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        if (database == NULL || !database->session.has_selected_schema) {
            return MYLITE_OK;
        }
        memcpy(schema_name, database->session.selected_schema, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        memcpy(table_name, parts[0], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        *out_has_target = true;
        return MYLITE_OK;
    }
    if (part_count == 2U) {
        memcpy(schema_name, parts[0], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        memcpy(table_name, parts[1], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        *out_has_target = true;
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static bool mysql_system_table_directory_contains(const char *table_name) {
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory = NULL;

    if (table_name == NULL) {
        return false;
    }
    directory = find_builtin_schema_table_directory("mysql");
    if (directory == NULL) {
        return false;
    }
    for (size_t index = 0U; index < directory->table_count; ++index) {
        if (strcmp(table_name, directory->table_names[index]) == 0) {
            return true;
        }
    }
    return false;
}

static int execute_show_columns_mysql_system_table_statement(
    struct mylite_db *database,
    const struct show_columns_filter_nodes *nodes,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool full,
    mylite_result **out_result
) {
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (nodes == NULL || definition == NULL) {
        return MYLITE_MISUSE;
    }

    rc = make_show_like_filter(database, nodes->like, &filter);
    if (rc == MYLITE_OK) {
        rc = validate_show_columns_where_clause(database, nodes->where, full);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_columns_result_columns(database, result, full);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_columns_mysql_system_table_rows(
            database,
            result,
            &filter,
            nodes->where,
            definition,
            full
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (
            rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
        ) {
            set_runtime_error(database, "failed to build SHOW COLUMNS result");
        }
    }
    show_like_filter_deinit(&filter);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_show_columns_mysql_system_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool full
) {
    int rc = MYLITE_OK;

    if (result == NULL || definition == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < definition->query_definition.column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[column_index];
        const char *values[show_full_columns_result_column_count] = {0};
        bool where_matches = true;

        if (!show_like_filter_matches(filter, column->name, false)) {
            continue;
        }

        values[0] = column->name;
        values[1] = column->column_type;
        if (full) {
            values[2] = column->collation_name;
            values[3] = column->is_nullable;
            values[4] = definition->column_keys[column_index];
            values[show_full_columns_default_column] = column->column_default;
            values[show_full_columns_extra_column] = definition->column_extras[column_index];
            values[show_full_columns_privileges_column] =
                definition->column_privileges[column_index];
            values[show_full_columns_comment_column] =
                mysql_system_table_column_comment(definition, column_index);
        } else {
            values[2] = column->is_nullable;
            values[3] = definition->column_keys[column_index];
            values[4] = column->column_default;
            values[show_columns_extra_column] = definition->column_extras[column_index];
        }

        if (where_clause != NULL) {
            rc = show_columns_where_clause_matches(
                database,
                where_clause,
                values,
                full,
                &where_matches
            );
        }
        if (rc == MYLITE_OK && where_matches) {
            rc = mylite_result_append_text_row(result, values);
        }
    }
    return rc;
}

static int load_show_columns_key_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct primary_key_info *primary_key,
    struct loaded_index_info **out_indexes,
    size_t *out_index_count
) {
    int rc = MYLITE_OK;

    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        table->kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        return MYLITE_OK;
    }

    rc = load_primary_key_info(database, table->table_id, columns, column_count, primary_key);
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            out_indexes,
            out_index_count
        );
    }
    return rc;
}

static int append_show_columns_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    bool full
) {
    const char *const *selected_result_columns = show_columns_result_columns;
    size_t selected_result_column_count = show_columns_result_column_count;
    int rc = MYLITE_OK;

    if (full) {
        selected_result_columns = show_full_columns_result_columns;
        selected_result_column_count = show_full_columns_result_column_count;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < selected_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, selected_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int resolve_show_columns_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_columns_filter_nodes *out_nodes
) {
    size_t child_count = 0U;

    if (statement == NULL || out_nodes == NULL) {
        return MYLITE_MISUSE;
    }

    *out_nodes = (struct show_columns_filter_nodes){0};
    out_nodes->table = child_at(statement, 0U);
    child_count = mylite_sql_ast_node_child_count(statement);
    for (size_t index = 1U; index < child_count; ++index) {
        const struct mylite_sql_ast_node *child = child_at(statement, index);

        if (child == NULL) {
            continue;
        }
        if (child->kind == MYLITE_SQL_AST_LITERAL) {
            if (out_nodes->like != NULL || out_nodes->where != NULL) {
                set_runtime_error(database, "invalid SHOW COLUMNS filter");
                return MYLITE_ERROR;
            }
            out_nodes->like = child;
        } else if (child->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            if (out_nodes->like != NULL || out_nodes->where != NULL) {
                set_runtime_error(database, "invalid SHOW COLUMNS filter");
                return MYLITE_ERROR;
            }
            out_nodes->where = child;
        } else {
            if (out_nodes->schema != NULL) {
                set_runtime_error(database, "invalid SHOW COLUMNS schema");
                return MYLITE_ERROR;
            }
            out_nodes->schema = child;
        }
    }

    return MYLITE_OK;
}

static int validate_show_columns_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    bool full
) {
    static const char *const validation_values[show_columns_result_column_count] = {
        "field",
        "type",
        "YES",
        "",
        NULL,
        "extra",
    };
    static const char *const full_validation_values[show_full_columns_result_column_count] = {
        "field",
        "type",
        NULL,
        "YES",
        "",
        NULL,
        "extra",
        "select,insert,update,references",
        "",
    };
    const char *const *selected_validation_values = validation_values;
    bool matches = false;

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (full) {
        selected_validation_values = full_validation_values;
    }
    return show_columns_where_clause_matches(
        database,
        where_clause,
        selected_validation_values,
        full,
        &matches
    );
}

static int show_columns_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const *values,
    bool full,
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        values == NULL || out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_columns_where_predicate(
        database,
        child_at(where_clause, 0U),
        values,
        full,
        &truth
    );
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_columns_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || values == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_columns_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_columns_where_frame(database, current, values, full, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_columns_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
) {
    int rc = show_variables_where_frame_stack_push(
        database,
        frame_stack,
        (struct show_variables_where_eval_frame){
            .node = predicate,
            .action = SHOW_VARIABLES_WHERE_EVALUATE,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 0U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
    }
    if (predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE) {
        rc = show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 1U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
        if (rc == MYLITE_OK) {
            rc = show_variables_where_frame_stack_push(
                database,
                frame_stack,
                (struct show_variables_where_eval_frame){
                    .node = child_at(predicate, 0U),
                    .action = SHOW_VARIABLES_WHERE_VISIT,
                }
            );
        }
    }
    return rc;
}

static int evaluate_show_columns_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_columns_where_comparison_predicate(
            database,
            predicate,
            values,
            full,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_columns_where_in_predicate(database, predicate, values, full, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_columns_where_is_null_predicate(
            database,
            predicate,
            values,
            full,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW COLUMNS WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW COLUMNS WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int execute_show_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_index_filter_nodes nodes = {0};
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    bool missing_schema = false;
    bool mysql_system_handled = false;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = resolve_show_index_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = execute_show_index_mysql_system_target(
            database,
            &nodes,
            out_result,
            &mysql_system_handled
        );
    }
    if (rc != MYLITE_OK || mysql_system_handled) {
        return rc;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_show_columns_table_name(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            },
            &target,
            &missing_schema
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_table(database, &target, missing_schema, &table);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_index_where_clause(database, nodes.where);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < show_index_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, show_index_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (int group = index_display_group_primary;
         rc == MYLITE_OK && group <= index_display_group_fulltext;
         ++group) {
        for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
            if (index_display_group(&indexes[index]) != group) {
                continue;
            }
            rc = append_show_index(database, result, &target, &indexes[index], nodes.where);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        loaded_index_infos_deinit(&indexes, &index_count);
        free(columns);
        return rc;
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_index_mysql_system_target(
    struct mylite_db *database,
    const struct show_index_filter_nodes *nodes,
    mylite_result **out_result,
    bool *out_handled
) {
    const struct mylite_execution_catalog_mysql_system_table *definition = NULL;
    bool mysql_system_target = false;
    int rc = MYLITE_OK;

    if (nodes == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;

    rc = resolve_show_index_mysql_system_table(
        database,
        (struct show_columns_target_nodes){
            .table = nodes->table,
            .schema = nodes->schema,
        },
        &definition,
        &mysql_system_target
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (definition != NULL) {
        *out_handled = true;
        return execute_show_index_mysql_system_table_statement(
            database,
            nodes->where,
            definition,
            out_result
        );
    }
    if (mysql_system_target) {
        *out_handled = true;
        set_unsupported_error(database, "SHOW INDEX supports selected mysql system tables");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int resolve_show_index_mysql_system_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    const struct mylite_execution_catalog_mysql_system_table **out_definition,
    bool *out_mysql_system_target
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_target = false;
    int rc = MYLITE_OK;

    if (out_definition == NULL || out_mysql_system_target == NULL) {
        return MYLITE_MISUSE;
    }
    *out_definition = NULL;
    *out_mysql_system_target = false;

    rc = copy_show_columns_target_schema_and_table(
        database,
        nodes,
        schema_name,
        table_name,
        &has_target
    );
    if (rc != MYLITE_OK || !has_target) {
        return rc;
    }

    *out_definition = find_mysql_system_table_definition(schema_name, table_name);
    if (*out_definition != NULL) {
        *out_mysql_system_target = true;
        return MYLITE_OK;
    }
    if (!schema_name_is_mysql_system_schema(schema_name)) {
        return MYLITE_OK;
    }

    *out_mysql_system_target = true;
    if (mylite_catalog_name_is_reserved(table_name)) {
        set_reserved_name_error(database, "table", table_name);
        return MYLITE_ERROR;
    }
    if (mysql_data_dictionary_table_is_hidden(table_name)) {
        set_mysql_data_dictionary_table_access_error(database, table_name);
        return MYLITE_ERROR;
    }

    if (mysql_system_table_directory_contains(table_name)) {
        return MYLITE_OK;
    }

    set_table_does_not_exist_error(database, schema_name, table_name);
    return MYLITE_ERROR;
}

static int execute_show_index_mysql_system_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = validate_show_index_where_clause(database, where_clause);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < show_index_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, show_index_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_index_mysql_system_table_rows(database, result, where_clause, definition);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (
            rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
        ) {
            set_runtime_error(database, "failed to build SHOW INDEX result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_show_index_mysql_system_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (result == NULL || definition == NULL) {
        return MYLITE_MISUSE;
    }

    int rc = append_show_index_mysql_system_table_primary_key_rows(
        database,
        result,
        where_clause,
        definition
    );
    if (rc == MYLITE_OK) {
        rc = append_show_index_mysql_system_table_secondary_rows(
            database,
            result,
            where_clause,
            definition
        );
    }

    return rc;
}

static int append_show_index_mysql_system_table_primary_key_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;
    size_t primary_key_column_count = mysql_system_table_primary_key_column_count(definition);

    for (size_t key_index = 0U; rc == MYLITE_OK && key_index < primary_key_column_count;
         ++key_index) {
        const struct mylite_execution_catalog_column_definition *column =
            mysql_system_table_primary_key_column(definition, key_index);
        char sequence_text[integer_text_capacity];

        if (column == NULL) {
            set_runtime_error(database, "invalid mysql system table primary key");
            return MYLITE_ERROR;
        }

        rc = information_schema_format_i64(
            database,
            (int64_t)key_index + 1,
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            const char *values[show_index_result_column_count] = {
                definition->query_definition.name,
                "0",
                "PRIMARY",
                sequence_text,
                column->name,
                mysql_system_table_primary_key_collation(definition, key_index + 1),
                mysql_system_table_primary_key_cardinality(definition, key_index + 1),
                NULL,
                NULL,
                "",
                "BTREE",
                "",
                "",
                "YES",
                NULL,
            };

            rc = append_show_index_mysql_system_table_row_if_matches(
                database,
                result,
                where_clause,
                values
            );
        }
    }

    return rc;
}

static int append_show_index_mysql_system_table_secondary_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        char sequence_text[integer_text_capacity];

        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table secondary index");
            return MYLITE_ERROR;
        }

        rc = information_schema_format_i64(database, 1, sequence_text, sizeof(sequence_text));
        if (rc == MYLITE_OK) {
            const struct mylite_execution_catalog_column_definition *column =
                &definition->query_definition.columns[secondary_index->column_index];
            const char *values[show_index_result_column_count] = {
                definition->query_definition.name,
                secondary_index->non_unique,
                secondary_index->name,
                sequence_text,
                column->name,
                "A",
                secondary_index->cardinality,
                NULL,
                NULL,
                "",
                "BTREE",
                "",
                "",
                "YES",
                NULL,
            };

            rc = append_show_index_mysql_system_table_row_if_matches(
                database,
                result,
                where_clause,
                values
            );
        }
    }

    return rc;
}

static int append_show_index_mysql_system_table_row_if_matches(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_index_result_column_count]
) {
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (where_clause != NULL) {
        rc = show_index_where_clause_matches(database, where_clause, values, &where_matches);
    }
    if (rc == MYLITE_OK && where_matches) {
        rc = mylite_result_append_text_row(result, values);
    }

    return rc;
}

static const char *mysql_system_table_primary_key_cardinality(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t sequence
) {
    if (definition == NULL) {
        return "0";
    }
    return mysql_system_table_primary_key_cardinality_for_name(
        definition->query_definition.name,
        sequence
    );
}

static const char *mysql_system_table_primary_key_cardinality_for_name(
    const char *table_name,
    size_t sequence
) {
    enum {
        mysql_primary_key_no_sequence_split = 0,
        mysql_innodb_index_stats_stat_name_sequence = 4,
        mysql_second_key_part_sequence = 2,
        mysql_third_key_part_sequence = 3,
    };

    static const struct mysql_system_table_primary_key_cardinality_value {
        const char *table_name;
        size_t split_sequence;
        const char *first_cardinality;
        const char *split_cardinality;
    } cardinalities[] = {
        {"db", mysql_second_key_part_sequence, "1", "2"},
        {"engine_cost", mysql_primary_key_no_sequence_split, "2", "2"},
        {"global_grants", mysql_third_key_part_sequence, "1", "103"},
        {"proxies_priv", mysql_primary_key_no_sequence_split, "1", "1"},
        {"tables_priv", mysql_second_key_part_sequence, "1", "2"},
        {"innodb_table_stats", mysql_primary_key_no_sequence_split, "2", "2"},
        {"innodb_index_stats", mysql_innodb_index_stats_stat_name_sequence, "2", "6"},
        {"plugin", mysql_primary_key_no_sequence_split, "1", "1"},
        {"server_cost", mysql_primary_key_no_sequence_split, "6", "6"},
        {"sys_config", mysql_primary_key_no_sequence_split, "6", "6"},
        {"help_category", mysql_primary_key_no_sequence_split, "53", "53"},
        {"help_keyword", mysql_primary_key_no_sequence_split, "551", "551"},
        {"help_relation", mysql_second_key_part_sequence, "1393", "2258"},
        {"help_topic", mysql_primary_key_no_sequence_split, "596", "596"},
        {"time_zone", mysql_primary_key_no_sequence_split, "1457", "1457"},
        {"time_zone_leap_second", mysql_primary_key_no_sequence_split, "0", "0"},
        {"time_zone_name", mysql_primary_key_no_sequence_split, "1712", "1712"},
        {"time_zone_transition", mysql_second_key_part_sequence, "1252", "119074"},
        {"time_zone_transition_type", mysql_second_key_part_sequence, "1954", "10529"},
        {"user", mysql_second_key_part_sequence, "2", "5"},
    };

    for (size_t index = 0U; index < sizeof(cardinalities) / sizeof(cardinalities[0]); ++index) {
        if (strcmp(table_name, cardinalities[index].table_name) == 0) {
            if (cardinalities[index].split_sequence != mysql_primary_key_no_sequence_split &&
                sequence >= cardinalities[index].split_sequence) {
                return cardinalities[index].split_cardinality;
            }
            return cardinalities[index].first_cardinality;
        }
    }
    return "0";
}

static const char *mysql_system_table_primary_key_collation(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t sequence
) {
    if (definition == NULL) {
        return "A";
    }
    return mysql_system_table_primary_key_collation_for_name(
        definition->query_definition.name,
        sequence
    );
}

static const char *mysql_system_table_primary_key_collation_for_name(
    const char *table_name,
    size_t sequence
) {
    enum {
        mysql_password_history_timestamp_sequence = 3,
    };

    if (table_name != NULL && strcmp(table_name, "password_history") == 0 &&
        sequence == mysql_password_history_timestamp_sequence) {
        return "D";
    }
    return "A";
}

static int resolve_show_index_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_index_filter_nodes *out_nodes
) {
    size_t child_count = 0U;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct show_index_filter_nodes){0};
    if (statement == NULL) {
        set_runtime_error(database, "invalid SHOW INDEX statement");
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(statement);
    out_nodes->table = child_at(statement, 0U);
    if (out_nodes->table == NULL) {
        set_runtime_error(database, "invalid SHOW INDEX target");
        return MYLITE_ERROR;
    }

    for (size_t child_index = 1U; child_index < child_count; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(statement, child_index);

        if (child == NULL) {
            continue;
        }
        if (child->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            if (out_nodes->where != NULL) {
                set_runtime_error(database, "invalid SHOW INDEX filter");
                return MYLITE_ERROR;
            }
            out_nodes->where = child;
            continue;
        }
        if (out_nodes->schema == NULL) {
            out_nodes->schema = child;
            continue;
        }
        set_runtime_error(database, "invalid SHOW INDEX filter");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_show_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_show_create_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_show_create_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_show_create_table_from_plan(database, &plan, result);
    }
    planned_show_create_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_view_descriptor view = {0};
    mylite_result *result = NULL;
    bool missing_schema = false;
    const struct mylite_execution_catalog_builtin_sys_view *selected_builtin_sys_view = NULL;
    const struct mylite_execution_catalog_builtin_sys_view *builtin_sys_view = NULL;
    int rc = show_create_statement_targets_selected_builtin_sys_view(
        database,
        statement,
        &selected_builtin_sys_view
    );

    if (rc == MYLITE_OK && selected_builtin_sys_view != NULL) {
        return execute_show_create_builtin_sys_view_statement(
            database,
            selected_builtin_sys_view,
            false,
            out_result
        );
    }

    if (rc == MYLITE_OK) {
        rc = resolve_table_name_allow_missing_schema(
            database,
            child_at(statement, 0U),
            &target,
            &missing_schema
        );
    }

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(target.table_name)) {
        set_reserved_name_error(database, "table", target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        builtin_sys_view = show_create_target_builtin_sys_view(&target);
    }
    if (rc == MYLITE_OK && builtin_sys_view != NULL) {
        return execute_show_create_builtin_sys_view_statement(
            database,
            builtin_sys_view,
            show_create_target_was_schema_qualified(statement),
            out_result
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_persistent_metadata_table_reference(database, &target, missing_schema, &table);
    }
    if (rc == MYLITE_OK && table.kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
        set_not_view_error(database, target.schema.name, target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_view_by_table_id(database, table.table_id, &view);
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_view_result(database, result, table.name, &view);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_builtin_sys_view_statement(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_show_create_builtin_sys_view_result(database, result, view, schema_qualified);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_database_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_create_database_result_column_count] = {
        "Database",
        "Create Database",
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *create_sql = NULL;
    const char *values[show_create_database_result_column_count] = {NULL, NULL};
    mylite_result *result = NULL;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_schema_name(database, schema_name, &schema);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_database_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_create_database_sql(&schema, &create_sql);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        values[0] = schema.name;
        values[1] = create_sql;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free(create_sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_engines_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_engines_result_column_count] = {
        "Engine",
        "Support",
        "Comment",
        "Transactions",
        "XA",
        "Savepoints",
    };
    static const char *const values[show_engines_result_column_count] = {
        "InnoDB",
        "DEFAULT",
        "Supports transactions, row-level locking, and foreign keys",
        "YES",
        "YES",
        "YES",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_engines_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_engine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_engine_status_result_column_count] = {
        "Type",
        "Name",
        "Status",
    };
    static const char *const values[show_engine_status_result_column_count] = {
        "InnoDB",
        "",
        "MyLite embedded InnoDB-compatible storage engine is active",
    };
    mylite_result *result = NULL;
    int rc = validate_show_engine_name(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_engine_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int validate_show_engine_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    char engine_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(statement, 0U),
        engine_name,
        sizeof(engine_name),
        (struct table_option_name_policy){
            .identifier_kind = "storage engine",
            .nul_message = "storage engine names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(engine_name, "InnoDB")) {
        set_unknown_storage_engine_error(database, engine_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_show_plugins_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_plugins_result_column_count] = {
        "Name",
        "Status",
        "Type",
        "Library",
        "License",
    };
    static const char *const values[show_plugins_result_column_count] = {
        "InnoDB",
        "ACTIVE",
        "STORAGE ENGINE",
        NULL,
        "GPL",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_plugins_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_databases_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *filter_node = child_at(statement, 0U);
    const struct mylite_sql_ast_node *like_node = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_databases_context context = {0};
    char *column_name = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        like_node = filter_node;
    } else if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else if (filter_node != NULL) {
        set_runtime_error(database, "invalid SHOW DATABASES filter");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_databases_column_name(&filter, &column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_databases_where_clause(database, where_clause);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        rc = append_builtin_show_database_names(&context);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(database, collect_show_database, &context);
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW DATABASES result");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_sorted_show_database_rows(database, result, &filter, where_clause, &context);
    }
    if (rc != MYLITE_OK) {
        free(context.names);
        mylite_result_free(result);
        free(column_name);
        show_like_filter_deinit(&filter);
        return rc;
    }

    free(context.names);
    free(column_name);
    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int plan_show_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_show_create_table *out_plan
) {
    bool missing_schema = false;
    const struct mylite_execution_catalog_builtin_sys_view *selected_builtin_sys_view = NULL;
    const struct mylite_execution_catalog_builtin_sys_view *builtin_sys_view = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_show_create_table){0};
    rc = show_create_statement_targets_selected_builtin_sys_view(
        database,
        statement,
        &selected_builtin_sys_view
    );
    if (rc == MYLITE_OK && selected_builtin_sys_view != NULL) {
        populate_builtin_sys_view_target(&out_plan->target, selected_builtin_sys_view);
        out_plan->is_view = true;
        out_plan->builtin_sys_view = selected_builtin_sys_view;
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_table_name_allow_missing_schema(
            database,
            child_at(statement, 0U),
            &out_plan->target,
            &missing_schema
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->target_was_schema_qualified = show_create_target_was_schema_qualified(statement);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        builtin_sys_view = show_create_target_builtin_sys_view(&out_plan->target);
    }
    if (rc == MYLITE_OK && builtin_sys_view != NULL) {
        out_plan->is_view = true;
        out_plan->builtin_sys_view = builtin_sys_view;
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_metadata_table_reference(
            database,
            &out_plan->target,
            missing_schema,
            &out_plan->table
        );
    }
    if (rc == MYLITE_OK && out_plan->table.kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        out_plan->is_view = true;
        rc = mylite_catalog_read_view_by_table_id(
            database,
            out_plan->table.table_id,
            &out_plan->view
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        }
    }
    if (rc == MYLITE_OK && out_plan->is_view) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_unsupported_error(database, "SHOW CREATE TABLE supports only tables and views");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->indexes,
            &out_plan->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->foreign_keys,
            &out_plan->foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_check_constraint_infos(
            database,
            out_plan->table.table_id,
            &out_plan->check_constraints,
            &out_plan->check_constraint_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_show_create_table_deinit(out_plan);
    }

    return rc;
}

static void planned_show_create_table_deinit(struct planned_show_create_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    loaded_index_infos_deinit(&plan->indexes, &plan->index_count);
    loaded_foreign_key_infos_deinit(&plan->foreign_keys, &plan->foreign_key_count);
    loaded_check_constraint_infos_deinit(&plan->check_constraints, &plan->check_constraint_count);
    *plan = (struct planned_show_create_table){0};
}

static int execute_show_create_table_from_plan(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    mylite_result *result
) {
    static const char *const result_columns[show_create_table_result_column_count] = {
        "Table",
        "Create Table",
    };
    char *create_sql = NULL;
    const char *values[show_create_table_result_column_count] = {NULL, NULL};
    int rc = MYLITE_OK;

    if (plan->builtin_sys_view != NULL) {
        return append_show_create_builtin_sys_view_result(
            database,
            result,
            plan->builtin_sys_view,
            plan->target_was_schema_qualified
        );
    }
    if (plan->is_view) {
        return append_show_create_view_result(database, result, plan->table.name, &plan->view);
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_table_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_create_table_sql(database, plan, &create_sql);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        values[0] = plan->table.name;
        values[1] = create_sql;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(create_sql);
    return rc;
}

static int append_show_create_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const struct mylite_catalog_view_descriptor *view
) {
    return append_show_create_view_text_result(
        database,
        result,
        view_name,
        view->show_create_sql,
        view->character_set_client,
        view->collation_connection
    );
}

static int append_show_create_builtin_sys_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified
) {
    const char *show_create_sql =
        schema_qualified ? view->show_create_qualified_view_sql : view->show_create_view_sql;

    return append_show_create_view_text_result(
        database,
        result,
        view->name,
        show_create_sql,
        "utf8mb4",
        "utf8mb4_0900_ai_ci"
    );
}

static int append_show_create_view_text_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const char *show_create_sql,
    const char *character_set_client,
    const char *collation_connection
) {
    static const char *const result_columns[show_create_view_result_column_count] = {
        "View",
        "Create View",
        "character_set_client",
        "collation_connection",
    };
    const char *values[show_create_view_result_column_count] = {
        view_name,
        show_create_sql,
        character_set_client,
        collation_connection,
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_view_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static const struct mylite_execution_catalog_builtin_sys_view *find_builtin_sys_view_definition(
    const char *view_name
) {
    return mylite_execution_catalog_builtin_sys_view_definition_by_name(view_name);
}

static const struct mylite_execution_catalog_builtin_sys_view *show_create_target_builtin_sys_view(
    const struct table_name_resolution *target
) {
    if (target == NULL || strcmp(target->schema.name, "sys") != 0) {
        return NULL;
    }
    return find_builtin_sys_view_definition(target->table_name);
}

static bool show_create_target_was_schema_qualified(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *target = statement == NULL ? NULL : child_at(statement, 0U);

    return target != NULL && target->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER;
}

static int show_create_statement_targets_selected_builtin_sys_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct mylite_execution_catalog_builtin_sys_view **out_view
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_view == NULL) {
        return MYLITE_MISUSE;
    }
    *out_view = NULL;
    if (database == NULL || !database->session.has_selected_schema ||
        strcmp(database->session.selected_schema, "sys") != 0) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        statement == NULL ? NULL : child_at(statement, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        *out_view = find_builtin_sys_view_definition(parts[0]);
    }
    return MYLITE_OK;
}

static void populate_builtin_sys_view_target(
    struct table_name_resolution *target,
    const struct mylite_execution_catalog_builtin_sys_view *view
) {
    if (target == NULL || view == NULL) {
        return;
    }

    *target = (struct table_name_resolution){0};
    (void)snprintf(target->schema.name, sizeof(target->schema.name), "%s", "sys");
    (void)snprintf(target->table_name, sizeof(target->table_name), "%s", view->name);
}

static int build_show_create_table_sql(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(
        &string,
        plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY ? "CREATE TEMPORARY TABLE "
                                                                : "CREATE TABLE "
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, plan->table.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (\n");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = append_show_create_table_column_definition(
            database,
            &string,
            &plan->table,
            &plan->columns[column_index],
            (plan->index_count == 0U && plan->foreign_key_count == 0U &&
             plan->check_constraint_count == 0U && column_index + 1U == plan->column_count) != 0
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_indexes(database, &string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_keys(database, &string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_check_constraints(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_table_options(database, &string, plan);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_show_create_table_column_definition(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
) {
    const char *type_text = NULL;
    char type_text_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    bool is_national = column_descriptor_is_national_char_or_varchar(column);
    int rc = show_create_table_type_text(
        database,
        column->logical_type,
        type_text_storage,
        sizeof(type_text_storage),
        &type_text
    );

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "  ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, type_text);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_column_charset_collation(string, table, column, is_national);
    }
    if (rc == MYLITE_OK && column->is_generated) {
        rc = mylite_dynamic_string_append(string, " GENERATED ALWAYS AS (");
    }
    if (rc == MYLITE_OK && column->is_generated) {
        rc = mylite_dynamic_string_append(string, column->generation_expression);
    }
    if (rc == MYLITE_OK && column->is_generated) {
        rc = mylite_dynamic_string_append(
            string,
            column->generated_kind == MYLITE_CATALOG_GENERATED_COLUMN_STORED ? ") STORED"
                                                                             : ") VIRTUAL"
        );
    }
    if (rc == MYLITE_OK && !column->is_nullable) {
        rc = mylite_dynamic_string_append(string, " NOT NULL");
    }
    if (rc == MYLITE_OK && !column->is_generated && column->is_nullable &&
        column_descriptor_is_timestamp(column)) {
        rc = mylite_dynamic_string_append(string, " NULL");
    }
    if (rc == MYLITE_OK && !column->is_generated) {
        rc = append_show_create_table_column_default(database, string, column);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_column_suffix(string, column, is_last_column);
    }

    return rc;
}

static int append_show_create_table_column_charset_collation(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_national
) {
    const bool has_explicit_attribute =
        (column->character_set_name[0] != '\0' || column->collation_name[0] != '\0') != 0;
    const char *effective_collation = NULL;
    int rc = MYLITE_OK;

    if (is_national) {
        rc = mylite_dynamic_string_append(string, " CHARACTER SET ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, national_character_set_name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " COLLATE ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, national_collation_name);
        }
        return rc;
    }
    if (!column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }

    effective_collation = column_effective_collation_name(table, column);
    if (has_explicit_attribute) {
        rc = mylite_dynamic_string_append(string, " CHARACTER SET ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(
                string,
                column_effective_character_set_name(table, column)
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " COLLATE ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, effective_collation);
        }
        return rc;
    }
    if (text_equals_ascii_case_insensitive(
            effective_collation,
            MYLITE_CATALOG_DEFAULT_TABLE_COLLATION
        )) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " COLLATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, effective_collation);
    }
    return rc;
}

static int append_show_create_table_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    char default_text[show_create_integer_default_text_capacity];
    struct binary_string_type_info binary_info_storage = {0};
    const struct binary_string_type_info *binary_info =
        binary_string_type_info_for_logical_type(column->logical_type, &binary_info_storage);
    bool binary_blob_family = (column_descriptor_is_binary_string_family(column) &&
                               binary_info != NULL && binary_info->blob_family) != 0;
    int written = 0;

    if (column->is_auto_increment) {
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE && column->is_nullable &&
        !column_descriptor_is_text_family(column) && !binary_blob_family) {
        return mylite_dynamic_string_append(string, " DEFAULT NULL");
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return mylite_dynamic_string_append(string, " DEFAULT CURRENT_TIMESTAMP");
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return mylite_dynamic_string_append(string, " DEFAULT (curdate())");
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return mylite_dynamic_string_append(string, " DEFAULT (curtime())");
    }
    if (column_default_kind_is_expression(column->default_kind)) {
        return append_show_create_table_expression_default(string, column);
    }
    if (column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        return append_show_create_table_non_integer_column_default(database, string, column);
    }

    written = snprintf(
        default_text,
        sizeof(default_text),
        " DEFAULT '%" PRId64 "'",
        column->default_integer
    );
    if (written < 0 || (size_t)written >= sizeof(default_text)) {
        set_runtime_error(database, "failed to format column default");
        return MYLITE_ERROR;
    }

    return mylite_dynamic_string_append(string, default_text);
}

static int append_show_create_table_non_integer_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_bit(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        int rc = mylite_dynamic_string_append(string, " DEFAULT ");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, column->default_text);
        }
        return rc;
    }
    if (column_descriptor_is_text_family(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_show_create_table_text_expression_default(string, column);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        if (column_descriptor_is_binary_blob_family(column)) {
            return append_show_create_table_binary_blob_expression_default(
                database,
                string,
                column
            );
        }
        return append_show_create_table_binary_default(database, string, column);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_mysql_quoted_default_text(string, column->default_text);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return append_show_create_table_text_expression_default(string, column);
    }
    return MYLITE_OK;
}

static int append_show_create_table_binary_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " DEFAULT ");
    }
    if (rc == MYLITE_OK) {
        if (binary_default_bytes_use_hex_literal(bytes, byte_count)) {
            rc = append_binary_default_hex_literal(string, bytes, byte_count);
        } else {
            rc = append_mysql_quoted_binary_default_text(string, bytes, byte_count);
        }
    }
    free(bytes);
    return rc;
}

static int append_show_create_table_binary_blob_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " DEFAULT (");
    }
    if (rc == MYLITE_OK) {
        if (byte_count == 0U) {
            rc = mylite_dynamic_string_append(string, "X''");
        } else {
            rc = append_binary_default_hex_literal(string, bytes, byte_count);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    free(bytes);
    return rc;
}

static bool binary_default_bytes_use_hex_literal(const char *bytes, size_t byte_count) {
    for (size_t index = 0U; index < byte_count; ++index) {
        unsigned char byte = (unsigned char)bytes[index];

        if (byte == '\0' ||
            (byte >= ascii_printable_min_byte && byte <= ascii_printable_max_byte)) {
            continue;
        }
        return true;
    }
    return false;
}

static int append_binary_default_hex_literal(
    struct mylite_dynamic_string *string,
    const char *bytes,
    size_t byte_count
) {
    static const char hex_digits[] = "0123456789ABCDEF";
    int rc = mylite_dynamic_string_append(string, "0x");

    for (size_t byte_index = 0U; rc == MYLITE_OK && byte_index < byte_count; ++byte_index) {
        unsigned char byte = (unsigned char)bytes[byte_index];

        rc = mylite_dynamic_string_append_char(string, hex_digits[byte >> byte_high_nibble_shift]);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, hex_digits[byte & byte_low_nibble_mask]);
        }
    }
    return rc;
}

static int append_mysql_quoted_binary_default_text(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        switch (text[index]) {
        case '\0':
            rc = mylite_dynamic_string_append(string, "\\0");
            break;
        case '\'':
            rc = mylite_dynamic_string_append(string, "''");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        case '\t':
            rc = mylite_dynamic_string_append(string, "\\t");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_show_create_table_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT (");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, column->default_text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_show_create_table_text_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT (");

    if (rc == MYLITE_OK) {
        rc = append_mysql_utf8mb4_expression_text(string, column->default_text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_show_create_table_column_suffix(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
) {
    int rc = MYLITE_OK;

    if (column->is_auto_increment) {
        rc = mylite_dynamic_string_append(string, " AUTO_INCREMENT");
    }
    if (rc == MYLITE_OK && column->on_update_current_timestamp) {
        rc = mylite_dynamic_string_append(string, " ON UPDATE CURRENT_TIMESTAMP");
    }
    if (rc == MYLITE_OK && column->comment[0] != '\0') {
        rc = mylite_dynamic_string_append(string, " COMMENT ");
        if (rc == MYLITE_OK) {
            rc = append_mysql_quoted_text(string, column->comment);
        }
    }
    if (rc == MYLITE_OK && !column->is_visible) {
        rc = mylite_dynamic_string_append(string, " /*!80023 INVISIBLE */");
    }
    if (rc == MYLITE_OK && !is_last_column) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\n');
    }

    return rc;
}

static int append_show_create_table_indexes(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    size_t appended = 0U;
    int rc = MYLITE_OK;

    for (int group = index_display_group_primary;
         rc == MYLITE_OK && group <= index_display_group_fulltext;
         ++group) {
        for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
            if (index_display_group(&plan->indexes[index]) != group) {
                continue;
            }

            ++appended;
            const bool is_last_index = appended == plan->index_count;
            const bool has_no_foreign_keys = plan->foreign_key_count == 0U;
            const bool has_no_check_constraints = plan->check_constraint_count == 0U;
            bool is_last_definition = false;

            if (is_last_index && has_no_foreign_keys && has_no_check_constraints) {
                is_last_definition = true;
            }

            rc = append_show_create_table_index(
                database,
                string,
                &plan->indexes[index],
                is_last_definition
            );
        }
    }

    return rc;
}

static int append_show_create_table_foreign_keys(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    (void)database;
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->foreign_key_count; ++index) {
        bool is_last_constraint = false;

        if (index + 1U == plan->foreign_key_count && plan->check_constraint_count == 0U) {
            is_last_constraint = true;
        }
        rc = append_show_create_table_foreign_key(
            string,
            &plan->foreign_keys[index],
            is_last_constraint
        );
    }

    return rc;
}

static int append_show_create_table_check_constraints(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->check_constraint_count; ++index) {
        const struct mylite_catalog_check_constraint_descriptor *check_constraint =
            &plan->check_constraints[index].check_constraint;

        rc = mylite_dynamic_string_append(string, "  CONSTRAINT ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(
                string,
                check_constraint->name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " CHECK (");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, check_constraint->check_clause);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        if (rc == MYLITE_OK && !check_constraint->is_enforced) {
            rc = mylite_dynamic_string_append(string, " /*!80016 NOT ENFORCED */");
        }
        if (rc == MYLITE_OK && index + 1U < plan->check_constraint_count) {
            rc = mylite_dynamic_string_append_char(string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '\n');
        }
    }

    return rc;
}

static int append_show_create_table_table_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = mylite_dynamic_string_append(string, ") ENGINE=InnoDB");

    if (rc == MYLITE_OK) {
        rc = append_show_create_table_auto_increment_option(database, string, plan);
    }
    if (rc == MYLITE_OK && charset_name_is_binary(plan->table.default_charset) &&
        collation_name_is_binary(plan->table.default_collation)) {
        rc = mylite_dynamic_string_append(string, " DEFAULT CHARSET=binary");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " DEFAULT CHARSET=");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, plan->table.default_charset);
        }
        if (rc == MYLITE_OK && show_create_table_should_append_default_collation(plan)) {
            rc = mylite_dynamic_string_append(string, " COLLATE=");
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append(string, plan->table.default_collation);
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_comment_option(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_storage_statistics_options(database, string, plan);
    }

    return rc;
}

static bool show_create_table_should_append_default_collation(
    const struct planned_show_create_table *plan
) {
    if (plan == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(plan->table.default_charset, "ascii") &&
        text_equals_ascii_case_insensitive(plan->table.default_collation, "ascii_general_ci")) {
        return false;
    }
    return true;
}

static int append_show_create_table_auto_increment_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    char auto_increment_text[integer_text_capacity + sizeof(" AUTO_INCREMENT=")];
    int written = 0;

    if (!show_create_table_has_auto_increment(plan) || plan->table.auto_increment_next <= 1) {
        return MYLITE_OK;
    }

    written = snprintf(
        auto_increment_text,
        sizeof(auto_increment_text),
        " AUTO_INCREMENT=%" PRId64,
        plan->table.auto_increment_next
    );
    if (written < 0 || (size_t)written >= sizeof(auto_increment_text)) {
        set_runtime_error(database, "failed to format auto-increment table option");
        return MYLITE_ERROR;
    }

    return mylite_dynamic_string_append(string, auto_increment_text);
}

static bool show_create_table_has_auto_increment(const struct planned_show_create_table *plan) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (column_descriptor_is_auto_increment(&plan->columns[column_index])) {
            return true;
        }
    }

    return false;
}

static int append_show_create_table_comment_option(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL || plan->table.comment[0] == '\0') {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " COMMENT=");
    if (rc == MYLITE_OK) {
        rc = append_mysql_quoted_text(string, plan->table.comment);
    }

    return rc;
}

static int append_show_create_table_storage_statistics_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_OK;
    }
    if (plan->table.pack_keys != -1) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " PACK_KEYS=",
            plan->table.pack_keys
        );
    }
    if (rc == MYLITE_OK && plan->table.stats_persistent != -1) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " STATS_PERSISTENT=",
            plan->table.stats_persistent
        );
    }
    if (rc == MYLITE_OK && plan->table.stats_auto_recalc != -1) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " STATS_AUTO_RECALC=",
            plan->table.stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK && plan->table.stats_sample_pages > 0) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " STATS_SAMPLE_PAGES=",
            plan->table.stats_sample_pages
        );
    }
    if (rc == MYLITE_OK && plan->table.checksum != 0) {
        rc = mylite_dynamic_string_append(string, " CHECKSUM=1");
    }
    if (rc == MYLITE_OK && plan->table.row_format_option[0] != '\0') {
        rc = mylite_dynamic_string_append(string, " ROW_FORMAT=");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, plan->table.row_format_option);
        }
    }
    if (rc == MYLITE_OK && plan->table.key_block_size > 0) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " KEY_BLOCK_SIZE=",
            plan->table.key_block_size
        );
    }

    return rc;
}

static int append_show_create_table_integer_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *prefix,
    int64_t value
) {
    char text[integer_text_capacity + sizeof(" STATS_SAMPLE_PAGES=")];
    int written = snprintf(text, sizeof(text), "%s%" PRId64, prefix, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        set_runtime_error(database, "failed to format table option value");
        return MYLITE_ERROR;
    }
    return mylite_dynamic_string_append(string, text);
}

static int append_show_create_table_index(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index,
    bool is_last_index
) {
    int rc = MYLITE_OK;

    if (index == NULL) {
        set_runtime_error(database, "invalid index descriptor");
        return MYLITE_ERROR;
    }

    rc = append_show_create_table_index_header(database, string, index);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        rc = append_show_create_table_index_part(
            string,
            &index->parts[part_index],
            part_index != 0U
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_index_options(string, index);
    }
    if (rc == MYLITE_OK && !is_last_index) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\n');
    }

    return rc;
}

static int append_show_create_table_index_header(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index
) {
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        return mylite_dynamic_string_append(string, "  PRIMARY KEY (");
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        const char *prefix = "  KEY ";
        if (index->index.is_unique) {
            prefix = "  UNIQUE KEY ";
        }
        rc = mylite_dynamic_string_append(string, prefix);
    } else if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        rc = mylite_dynamic_string_append(string, "  FULLTEXT KEY ");
    } else if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        rc = mylite_dynamic_string_append(string, "  SPATIAL KEY ");
    } else {
        set_runtime_error(database, "invalid index descriptor");
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, index->index.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }

    return rc;
}

static int append_show_create_table_index_options(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index
) {
    int rc = MYLITE_OK;

    if (index->index.show_create_explicit_btree) {
        rc = mylite_dynamic_string_append(string, " USING BTREE");
    }
    if (rc == MYLITE_OK && index->index.comment[0] != '\0') {
        rc = mylite_dynamic_string_append(string, " COMMENT ");
        if (rc == MYLITE_OK) {
            rc = append_mysql_quoted_text(string, index->index.comment);
        }
    }
    if (rc == MYLITE_OK && index->index.kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY &&
        !index->index.is_visible) {
        rc = mylite_dynamic_string_append(string, " /*!80000 INVISIBLE */");
    }

    return rc;
}

static int append_show_create_table_foreign_key(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool is_last_foreign_key
) {
    int rc = MYLITE_OK;

    if (foreign_key == NULL || foreign_key->part_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "  CONSTRAINT ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            foreign_key->foreign_key.name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FOREIGN KEY (");
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_parts(string, foreign_key, true);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") REFERENCES ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            foreign_key->parent_table.name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_parts(string, foreign_key, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_rule(
            string,
            "DELETE",
            foreign_key->foreign_key.delete_rule
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_rule(
            string,
            "UPDATE",
            foreign_key->foreign_key.update_rule
        );
    }
    if (rc == MYLITE_OK && !is_last_foreign_key) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\n');
    }

    return rc;
}

static int append_show_create_table_foreign_key_parts(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool child_columns
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const char *column_name = foreign_key->parts[part_index].parent_column.name;

        if (child_columns) {
            column_name = foreign_key->parts[part_index].child_column.name;
        }

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, column_name);
        }
    }
    return rc;
}

static int append_show_create_table_foreign_key_rule(
    struct mylite_dynamic_string *string,
    const char *timing,
    const char *rule
) {
    int rc = MYLITE_OK;

    if (strcmp(rule, "NO ACTION") == 0) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " ON ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, timing);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, rule);
    }
    return rc;
}

static int append_show_create_table_index_part(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    bool needs_comma
) {
    int rc = MYLITE_OK;

    if (needs_comma) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, part->column.name);
    }
    if (rc == MYLITE_OK && part->index_column.has_prefix_length) {
        char prefix_text[integer_text_capacity];
        int written = snprintf(
            prefix_text,
            sizeof(prefix_text),
            "%" PRId64,
            part->index_column.prefix_length
        );

        if (written < 0 || (size_t)written >= sizeof(prefix_text)) {
            return MYLITE_NOMEM;
        }
        rc = mylite_dynamic_string_append_char(string, '(');
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, prefix_text);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
    }
    if (rc == MYLITE_OK &&
        part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        rc = mylite_dynamic_string_append(string, " DESC");
    }

    return rc;
}

static int index_display_group(const struct loaded_index_info *index) {
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        return index_display_group_primary;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY && index->index.is_unique &&
        index->part_count > 0U && !index->parts[0].column.is_nullable) {
        return index_display_group_not_null_unique;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY && index->index.is_unique) {
        return index_display_group_nullable_unique;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        return index_display_group_secondary;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return index_display_group_fulltext;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return index_display_group_fulltext;
    }

    return index_display_group_unknown;
}

static int show_create_table_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text
) {
    return show_descriptor_type_text(
        database,
        logical_type,
        buffer,
        buffer_size,
        "SHOW CREATE TABLE supports only integer, BIT, string, binary string, decimal, "
        "approximate numeric, YEAR, DATE, TIME, DATETIME, and TIMESTAMP column descriptors",
        out_type_text
    );
}

static int show_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    const char *integer_text = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (logical_type == NULL || buffer == NULL || unsupported_message == NULL ||
        out_type_text == NULL) {
        set_runtime_error(database, "invalid column descriptor");
        return MYLITE_ERROR;
    }
    if (logical_type_is_char(logical_type)) {
        return format_char_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (logical_type_is_varchar(logical_type)) {
        return format_varchar_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (format_text_family_type_text(logical_type, out_type_text) == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "JSON") == 0) {
        *out_type_text = "json";
        return MYLITE_OK;
    }
    if (spatial_logical_type_display_text(logical_type, out_type_text)) {
        return MYLITE_OK;
    }
    if (strncmp(logical_type, "ENUM(", enum_logical_prefix_length) == 0) {
        return format_enum_descriptor_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (strncmp(logical_type, "SET(", set_logical_prefix_length) == 0) {
        return format_set_descriptor_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (format_binary_string_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        ) == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (strncmp(logical_type, "BIT(", bit_logical_prefix_length) == 0) {
        uint64_t width = 0U;
        int written = 0;

        rc = bit_width_for_logical_type(logical_type, &width);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        written = snprintf(buffer, buffer_size, "bit(%" PRIu64 ")", width);
        if (written < 0 || (size_t)written >= buffer_size) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        *out_type_text = buffer;
        return MYLITE_OK;
    }
    rc = show_descriptor_numeric_type_text(
        database,
        logical_type,
        buffer,
        buffer_size,
        out_type_text,
        &matched
    );
    if (rc != MYLITE_OK || matched) {
        return rc;
    }
    if (show_descriptor_temporal_type_text(logical_type, out_type_text)) {
        return MYLITE_OK;
    }

    integer_text = integer_descriptor_display_text(logical_type);
    if (integer_text != NULL) {
        *out_type_text = integer_text;
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static bool show_descriptor_temporal_type_text(
    const char *logical_type,
    const char **out_type_text
) {
    static const struct {
        const char *logical_type;
        const char *display_type;
    } temporal_types[] = {
        {"DATE", "date"},
        {"TIME", "time"},
        {"DATETIME", "datetime"},
        {"TIMESTAMP", "timestamp"},
        {"YEAR", "year"},
    };

    for (size_t index = 0U; index < sizeof(temporal_types) / sizeof(temporal_types[0]); ++index) {
        if (strcmp(logical_type, temporal_types[index].logical_type) == 0) {
            *out_type_text = temporal_types[index].display_type;
            return true;
        }
    }

    return false;
}

static bool spatial_logical_type_display_text(
    const char *logical_type,
    const char **out_type_text
) {
    static const struct {
        const char *logical_type;
        const char *display_type;
    } spatial_types[] = {
        {"geometry", "geometry"},
        {"point", "point"},
        {"linestring", "linestring"},
        {"polygon", "polygon"},
        {"multipoint", "multipoint"},
        {"multilinestring", "multilinestring"},
        {"multipolygon", "multipolygon"},
        {"geomcollection", "geomcollection"},
    };

    if (logical_type == NULL || out_type_text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(spatial_types) / sizeof(spatial_types[0]); ++index) {
        if (strcmp(logical_type, spatial_types[index].logical_type) == 0) {
            *out_type_text = spatial_types[index].display_type;
            return true;
        }
    }
    return false;
}

static int show_descriptor_numeric_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text,
    bool *out_matched
) {
    struct decimal_type_info decimal_info = {0};
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (out_matched == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = false;

    if (decimal_type_info_for_logical_type(logical_type, &decimal_info) == MYLITE_OK) {
        *out_matched = true;
        return format_decimal_descriptor_type_text(
            database,
            buffer,
            buffer_size,
            &decimal_info,
            out_type_text
        );
    }
    if (approximate_type_info_for_logical_type(logical_type, &approximate_info) == MYLITE_OK) {
        *out_matched = true;
        return format_approximate_descriptor_type_text(
            database,
            buffer,
            buffer_size,
            &approximate_info,
            out_type_text
        );
    }

    return MYLITE_OK;
}

static int format_decimal_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct decimal_type_info *info,
    const char **out_type_text
) {
    int written = snprintf(
        buffer,
        buffer_size,
        "decimal(%" PRIu64 ",%" PRIu64 ")%s",
        info->precision,
        info->scale,
        info->is_unsigned ? " unsigned" : ""
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_approximate_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct approximate_type_info *info,
    const char **out_type_text
) {
    const char *base_type = info->type_class == APPROXIMATE_TYPE_FLOAT ? "float" : "double";
    int written = 0;

    if (!info->is_unsigned) {
        *out_type_text = base_type;
        return MYLITE_OK;
    }

    written = snprintf(buffer, buffer_size, "%s unsigned", base_type);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_varchar_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    size_t length = 0U;
    int written = 0;
    int rc = parse_varchar_descriptor_length(database, logical_type, unsupported_message, &length);

    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(buffer, buffer_size, "varchar(%zu)", length);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_char_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    size_t length = 0U;
    int written = 0;
    int rc = parse_char_descriptor_length(database, logical_type, unsupported_message, &length);

    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(buffer, buffer_size, "char(%zu)", length);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_enum_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    struct enum_type_info info = {0};
    int rc = enum_type_info_for_logical_type(database, logical_type, unsupported_message, &info);
    int written = 0;

    if (rc != MYLITE_OK) {
        return rc;
    }
    written =
        snprintf(buffer, buffer_size, "enum%s", logical_type + enum_logical_prefix_length - 1U);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_set_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    struct set_type_info info = {0};
    int rc = set_type_info_for_logical_type(database, logical_type, unsupported_message, &info);
    int written = 0;

    if (rc != MYLITE_OK) {
        return rc;
    }
    written = snprintf(buffer, buffer_size, "set%s", logical_type + set_logical_prefix_length - 1U);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_text_family_type_text(const char *logical_type, const char **out_type_text) {
    const struct text_family_type_info *info = text_family_type_info_for_logical_type(logical_type);

    if (info == NULL || out_type_text == NULL) {
        return MYLITE_ERROR;
    }
    *out_type_text = info->display_type;
    return MYLITE_OK;
}

static int format_binary_string_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(logical_type, &storage);
    size_t length = 0U;
    int written = 0;

    if (info == NULL || out_type_text == NULL) {
        return MYLITE_ERROR;
    }
    if (strncmp(logical_type, "BINARY(", binary_logical_prefix_length) == 0) {
        int rc =
            parse_binary_descriptor_length(database, logical_type, unsupported_message, &length);

        if (rc != MYLITE_OK) {
            return rc;
        }
        written = snprintf(buffer, buffer_size, "binary(%zu)", length);
    } else if (strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) == 0) {
        int rc =
            parse_varbinary_descriptor_length(database, logical_type, unsupported_message, &length);

        if (rc != MYLITE_OK) {
            return rc;
        }
        written = snprintf(buffer, buffer_size, "varbinary(%zu)", length);
    } else {
        *out_type_text = info->display_type;
        return info->display_type == NULL ? MYLITE_ERROR : MYLITE_OK;
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static const char *integer_descriptor_display_text(const char *logical_type) {
    static const struct {
        const char *logical_type;
        const char *display_type;
    } renderings[] = {
        {"INT", "int"},
        {"INTEGER", "int"},
        {"TINYINT", "tinyint"},
        {"TINYINT(1)", "tinyint(1)"},
        {"SMALLINT", "smallint"},
        {"MEDIUMINT", "mediumint"},
        {"INT UNSIGNED", "int unsigned"},
        {"INTEGER UNSIGNED", "int unsigned"},
        {"TINYINT UNSIGNED", "tinyint unsigned"},
        {"SMALLINT UNSIGNED", "smallint unsigned"},
        {"MEDIUMINT UNSIGNED", "mediumint unsigned"},
        {"BIGINT", "bigint"},
        {"BIGINT UNSIGNED", "bigint unsigned"},
    };

    if (logical_type == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < sizeof(renderings) / sizeof(renderings[0]); ++index) {
        if (strcmp(logical_type, renderings[index].logical_type) == 0) {
            return renderings[index].display_type;
        }
    }

    return NULL;
}

static int build_show_create_database_sql(
    const struct mylite_catalog_schema_descriptor *schema,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE DATABASE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, schema->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " /*!40100 DEFAULT CHARACTER SET ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, schema->default_charset);
    }
    if (rc == MYLITE_OK && !charset_name_is_binary(schema->default_charset)) {
        rc = mylite_dynamic_string_append(&string, " COLLATE ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, schema->default_collation);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " */ /*!80016 DEFAULT ENCRYPTION='N' */");
    }
    if (rc == MYLITE_OK) {
        *out_sql = string.text;
        return MYLITE_OK;
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int64_t row_count_for_completed_statement(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
) {
    if (result != NULL && mylite_result_column_count(result) != 0U) {
        return -1;
    }
    if (statement == NULL) {
        return 0;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_INSERT_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SELECT_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_DELETE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT:
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT:
    case MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT:
        return result == NULL ? 0 : mylite_result_affected_rows(result);
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        return -1;
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SET_STATEMENT:
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_LOCK_TABLES_STATEMENT:
    case MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT:
    case MYLITE_SQL_AST_CREATE_VIEW_STATEMENT:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_VIEW_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
    case MYLITE_SQL_AST_DO_STATEMENT:
    case MYLITE_SQL_AST_VALUES_STATEMENT:
        return 0;
    case MYLITE_SQL_AST_SPATIAL_TYPE:
    case MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION:
    case MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER:
        break;
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT:
    case MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_REPLICA_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_REPLICAS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_VIEW_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_PLUGINS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT:
    case MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CHECK_TABLE_STATEMENT:
    case MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT:
    case MYLITE_SQL_AST_CHECKSUM_TABLE_STATEMENT:
        return -1;
    case MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST:
    case MYLITE_SQL_AST_LOCK_TABLE_TARGET:
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK:
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK:
    case MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_UNION_TERM_LIST:
    case MYLITE_SQL_AST_UNION_TERM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_USER_VARIABLE:
    case MYLITE_SQL_AST_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_EXECUTE_USING_LIST:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_QUALIFIED_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_DML_DEFAULT_VALUE:
    case MYLITE_SQL_AST_DEFAULT_FUNCTION:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
    case MYLITE_SQL_AST_SUBTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATE_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_TIME_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATEDIFF_FUNCTION:
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMEDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TIME_TO_SEC_FUNCTION:
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_DAYS_FUNCTION:
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_SECONDS_FUNCTION:
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION:
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DATE_FUNCTION:
    case MYLITE_SQL_AST_TIME_FUNCTION:
    case MYLITE_SQL_AST_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_QUARTER_FUNCTION:
    case MYLITE_SQL_AST_WEEK_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_FUNCTION:
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_WEEKOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_YEARWEEK_FUNCTION:
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_YEAR_FUNCTION:
    case MYLITE_SQL_AST_MONTH_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAY_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFMONTH_FUNCTION:
    case MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFWEEK_FUNCTION:
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DAYOFYEAR_FUNCTION:
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LAST_DAY_FUNCTION:
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_HOUR_FUNCTION:
    case MYLITE_SQL_AST_MINUTE_FUNCTION:
    case MYLITE_SQL_AST_SECOND_FUNCTION:
    case MYLITE_SQL_AST_MICROSECOND_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_POSITION_FIRST:
    case MYLITE_SQL_AST_COLUMN_POSITION_AFTER:
    case MYLITE_SQL_AST_INTEGER_TYPE:
    case MYLITE_SQL_AST_VARCHAR_TYPE:
    case MYLITE_SQL_AST_CHAR_TYPE:
    case MYLITE_SQL_AST_TEXT_TYPE:
    case MYLITE_SQL_AST_JSON_TYPE:
    case MYLITE_SQL_AST_ENUM_TYPE:
    case MYLITE_SQL_AST_ENUM_LABEL_LIST:
    case MYLITE_SQL_AST_SET_TYPE:
    case MYLITE_SQL_AST_SET_MEMBER_LIST:
    case MYLITE_SQL_AST_BINARY_STRING_TYPE:
    case MYLITE_SQL_AST_BIT_TYPE:
    case MYLITE_SQL_AST_DECIMAL_TYPE:
    case MYLITE_SQL_AST_APPROXIMATE_TYPE:
    case MYLITE_SQL_AST_YEAR_TYPE:
    case MYLITE_SQL_AST_DATE_TYPE:
    case MYLITE_SQL_AST_DATETIME_TYPE:
    case MYLITE_SQL_AST_TIME_TYPE:
    case MYLITE_SQL_AST_TIMESTAMP_TYPE:
    case MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION:
    case MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST:
    case MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION:
    case MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION:
    case MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST:
    case MYLITE_SQL_AST_SECONDARY_INDEX_PART:
    case MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION:
    case MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST:
    case MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME:
    case MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_RESTRICT:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_NO_ACTION:
    case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL:
    case MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION:
    case MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED:
    case MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED:
    case MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION:
    case MYLITE_SQL_AST_INLINE_PRIMARY_KEY:
    case MYLITE_SQL_AST_INLINE_UNIQUE_KEY:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT:
    case MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE:
    case MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE:
    case MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE:
    case MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE:
    case MYLITE_SQL_AST_GENERATED_COLUMN_VIRTUAL:
    case MYLITE_SQL_AST_GENERATED_COLUMN_STORED:
    case MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION:
    case MYLITE_SQL_AST_NULLABILITY:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
    case MYLITE_SQL_AST_IDENTIFIER_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_VALUES_ROW_LIST:
    case MYLITE_SQL_AST_VALUES_ROW:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_VALUES_REFERENCE:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_JOIN:
    case MYLITE_SQL_AST_INDEX_HINT_LIST:
    case MYLITE_SQL_AST_USE_INDEX_HINT:
    case MYLITE_SQL_AST_FORCE_INDEX_HINT:
    case MYLITE_SQL_AST_IGNORE_INDEX_HINT:
    case MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN:
    case MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY:
    case MYLITE_SQL_AST_INDEX_HINT_FOR_GROUP_BY:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_ITEM_LIST:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_COMPARISON_PREDICATE:
    case MYLITE_SQL_AST_IS_NULL_PREDICATE:
    case MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE:
    case MYLITE_SQL_AST_AND_PREDICATE:
    case MYLITE_SQL_AST_OR_PREDICATE:
    case MYLITE_SQL_AST_XOR_PREDICATE:
    case MYLITE_SQL_AST_NOT_PREDICATE:
    case MYLITE_SQL_AST_BETWEEN_PREDICATE:
    case MYLITE_SQL_AST_IN_PREDICATE:
    case MYLITE_SQL_AST_EXISTS_PREDICATE:
    case MYLITE_SQL_AST_PREDICATE_VALUE_LIST:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_DIRECTION:
    case MYLITE_SQL_AST_ROW_NUMBER_FUNCTION:
    case MYLITE_SQL_AST_WINDOW_SPEC:
    case MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_ORDER_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_TABLE_ENGINE_OPTION:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_CHARSET_OPTION:
    case MYLITE_SQL_AST_TABLE_COLLATION_OPTION:
    case MYLITE_SQL_AST_TABLE_COMMENT_OPTION:
    case MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION:
    case MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION:
    case MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION:
    case MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_TYPE_OPTION:
    case MYLITE_SQL_AST_INDEX_COMMENT_OPTION:
    case MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION:
    case MYLITE_SQL_AST_ORDER_BY_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_BY_ITEM:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST:
    case MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_INSERT_STRING_FUNCTION:
    case MYLITE_SQL_AST_REVERSE_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_FUNCTION:
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHARSET_FUNCTION:
    case MYLITE_SQL_AST_COLLATION_FUNCTION:
    case MYLITE_SQL_AST_COERCIBILITY_FUNCTION:
    case MYLITE_SQL_AST_ELT_FUNCTION:
    case MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXPORT_SET_FUNCTION:
    case MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MAKE_SET_FUNCTION:
    case MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FIELD_FUNCTION:
    case MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INTERVAL_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_MOD_FUNCTION:
    case MYLITE_SQL_AST_ABS_FUNCTION:
    case MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SIGN_FUNCTION:
    case MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CEIL_FUNCTION:
    case MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CEILING_FUNCTION:
    case MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ROUND_FUNCTION:
    case MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIN_FUNCTION:
    case MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_OCT_FUNCTION:
    case MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONV_FUNCTION:
    case MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_PI_FUNCTION:
    case MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
    case MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SQRT_FUNCTION:
    case MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_DEGREES_FUNCTION:
    case MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RADIANS_FUNCTION:
    case MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ASIN_FUNCTION:
    case MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SIN_FUNCTION:
    case MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COS_FUNCTION:
    case MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TAN_FUNCTION:
    case MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COT_FUNCTION:
    case MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ATAN_FUNCTION:
    case MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ATAN2_FUNCTION:
    case MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_EXP_FUNCTION:
    case MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LN_FUNCTION:
    case MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOG_FUNCTION:
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
    case MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POWER_FUNCTION:
    case MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
    case MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CRC32_FUNCTION:
    case MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TO_BASE64_FUNCTION:
    case MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FROM_BASE64_FUNCTION:
    case MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
    case MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
    case MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHAR_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_FORMAT_LOCALE_UNSUPPORTED:
    case MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TRUNCATE_FUNCTION:
    case MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_OCTET_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_BIT_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHAR_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ASCII_FUNCTION:
    case MYLITE_SQL_AST_ORD_FUNCTION:
    case MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LOWER_FUNCTION:
    case MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LCASE_FUNCTION:
    case MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UPPER_FUNCTION:
    case MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_UCASE_FUNCTION:
    case MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LTRIM_FUNCTION:
    case MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RTRIM_FUNCTION:
    case MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_TRIM_FUNCTION:
    case MYLITE_SQL_AST_TRIM_LEADING_FUNCTION:
    case MYLITE_SQL_AST_TRIM_TRAILING_FUNCTION:
    case MYLITE_SQL_AST_LEFT_FUNCTION:
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
    case MYLITE_SQL_AST_LPAD_FUNCTION:
    case MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_RPAD_FUNCTION:
    case MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_SPACE_FUNCTION:
    case MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
    case MYLITE_SQL_AST_LOCATE_FUNCTION:
    case MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_INSTR_FUNCTION:
    case MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_POSITION_FUNCTION:
    case MYLITE_SQL_AST_FIND_IN_SET_FUNCTION:
    case MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_STRCMP_FUNCTION:
    case MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION:
    case MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN_CLAUSE:
    case MYLITE_SQL_AST_CASE_ELSE_CLAUSE:
    case MYLITE_SQL_AST_DO_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COUNT_STAR_FUNCTION:
    case MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION:
    case MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_FUNCTION:
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET:
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET:
    case MYLITE_SQL_AST_SET_DEFAULT_VALUE:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
    case MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE:
    case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE:
    case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY:
    case MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT:
    case MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER:
    case MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER:
    case MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER:
    case MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER:
    case MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER:
        break;
    }

    return 0;
}

static bool statement_preserves_diagnostics_snapshot(const struct mylite_sql_ast_node *statement) {
    if (statement == NULL) {
        return false;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT) {
        return true;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT) {
        return true;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT) {
        return true;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT) {
        return true;
    }
    return false;
}

static int snapshot_current_diagnostics(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_diagnostics_replace(&database->previous_diagnostics, &database->diagnostics);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int finish_successful_result(
    struct mylite_db *database,
    mylite_result *result,
    mylite_result **out_result
) {
    mylite_result_set_warning_count(
        result,
        mylite_diagnostics_warning_count(mylite_connection_diagnostics(database))
    );
    *out_result = result;

    return MYLITE_OK;
}

static int finish_successful_result_with_warning_count(
    mylite_result *result,
    size_t warning_count,
    mylite_result **out_result
) {
    mylite_result_set_warning_count(result, warning_count);
    *out_result = result;

    return MYLITE_OK;
}
