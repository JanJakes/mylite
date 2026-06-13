static int plan_row_scalar_unix_timestamp_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_unix_timestamp_input_kind input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_STRING;
    struct session_scalar_cell cell = {0};
    const char *input_kind_name = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_UNIX_TIMESTAMP_FUNCTION) {
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 0U) {
        rc = session_scalar_value(database, expression, &cell);
        if (rc == MYLITE_OK) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
        session_scalar_cell_deinit(&cell);
        return rc;
    }
    if (child_count != 1U) {
        set_native_function_parameter_count_error(database, "UNIX_TIMESTAMP");
        return MYLITE_ERROR;
    }
    if (unix_timestamp_argument_is_sysdate_function(child_at(expression, 0U))) {
        return plan_row_scalar_unix_timestamp_sysdate_expression(
            database,
            child_at(expression, 0U),
            out_expression
        );
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_unix_timestamp_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    input_kind_name = mylite_unix_timestamp_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_unix_timestamp_sysdate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    uint64_t precision = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind == MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR) {
        set_native_function_parameter_count_error(database, "SYSDATE");
        return MYLITE_ERROR;
    }

    rc = validate_supported_temporal_fractional_precision(
        database,
        expression,
        "sysdate",
        &precision
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    (void)precision;
    *out_expression = (struct planned_row_scalar_expression){
        .kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE,
        .value =
            (struct planned_value){.is_null = false, .integer = current_wall_clock_epoch()},
    };
    return MYLITE_OK;
}

static bool unix_timestamp_argument_is_sysdate_function(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    return expression != NULL &&
           (expression->kind == MYLITE_SQL_AST_SYSDATE_FUNCTION ||
            expression->kind == MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR);
}

static int plan_row_scalar_unix_timestamp_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_unix_timestamp_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            return plan_row_scalar_unix_timestamp_string_literal(
                database,
                expression,
                out_expression
            );
        }
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;
            int rc = collect_column_reference_parts(database, expression, parts, &part_count);

            if (rc == MYLITE_OK) {
                rc = format_column_reference_name(
                    database,
                    parts,
                    part_count,
                    column_name,
                    sizeof(column_name)
                );
            }
            if (rc != MYLITE_OK) {
                return rc;
            }
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
        return plan_row_scalar_unix_timestamp_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_unix_timestamp_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_unix_timestamp_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT UNIX_TIMESTAMP() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() does not yet support string descriptor columns"
        );
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "UNIX_TIMESTAMP() supports only DATE, DATETIME, TIMESTAMP descriptor columns, "
            "string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_unix_timestamp_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "UNIX_TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL",
        "UNIX_TIMESTAMP() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "UNIX_TIMESTAMP() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_timestamp_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    const char *input_kind_name = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIMESTAMP_FUNCTION) {
        set_native_function_parameter_count_error(database, "TIMESTAMP");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        set_native_function_parameter_count_error(database, "TIMESTAMP");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP;
    out_expression->argument_count = 4U;

    rc = plan_row_scalar_timestamp_temporal_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    input_kind_name = mylite_date_interval_second_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && child_count == 2U) {
        rc = plan_row_scalar_timestamp_time_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2]
        );
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value = (struct planned_value){.is_null = false, .integer = 1};
    } else if (rc == MYLITE_OK) {
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[2].value = (struct planned_value){.is_null = true, .integer = 0};
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value = (struct planned_value){.is_null = false, .integer = 0};
    }
    return rc;
}

static int plan_row_scalar_timestamp_temporal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_timestamp_temporal_literal(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;
            int rc = collect_column_reference_parts(database, expression, parts, &part_count);

            if (rc == MYLITE_OK) {
                rc = format_column_reference_name(
                    database,
                    parts,
                    part_count,
                    column_name,
                    sizeof(column_name)
                );
            }
            if (rc != MYLITE_OK) {
                return rc;
            }
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
        return plan_row_scalar_timestamp_temporal_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(
        database,
        "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
        "columns, string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timestamp_temporal_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_interval_second_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIMESTAMP() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "TIMESTAMP() does not yet support TIME as first argument");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only DATE, DATETIME, TIMESTAMP, string descriptor columns, "
            "string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timestamp_temporal_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        expression,
        "TIMESTAMP() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
        "columns, string descriptor columns, and NULL",
        "TIMESTAMP() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMP() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_timestamp_time_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMESTAMP() time argument supports only string time literals, TIME descriptor "
            "columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_timestamp_time_literal(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
            size_t part_count = 0U;
            int rc = collect_column_reference_parts(database, expression, parts, &part_count);

            if (rc == MYLITE_OK) {
                rc = format_column_reference_name(
                    database,
                    parts,
                    part_count,
                    column_name,
                    sizeof(column_name)
                );
            }
            if (rc != MYLITE_OK) {
                return rc;
            }
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
        return plan_row_scalar_timestamp_time_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    set_unsupported_error(
        database,
        "TIMESTAMP() time argument supports only string time literals, TIME descriptor columns, "
        "string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timestamp_time_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIMESTAMP() time argument supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!column_descriptor_is_time(&column) && !column_descriptor_is_string_family(&column)) {
        set_unsupported_error(
            database,
            "TIMESTAMP() time argument supports only TIME and string descriptor columns"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timestamp_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIMESTAMP() time argument supports only string time literals, TIME descriptor "
            "columns, string descriptor columns, and NULL"
        );
        return MYLITE_ERROR;
    }
    rc = decode_sql_string_literal(
        database,
        expression,
        "TIMESTAMP() time argument supports only string time literals, TIME descriptor columns, "
        "string descriptor columns, and NULL",
        "TIMESTAMP() time literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMP() time literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}
