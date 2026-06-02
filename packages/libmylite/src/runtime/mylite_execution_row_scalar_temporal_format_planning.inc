static int plan_row_scalar_date_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_date_format_input_kind input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    const char *input_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_DATE_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "DATE_FORMAT");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_date_format_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_date_format_format_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    input_kind_name = mylite_date_format_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_get_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    struct session_scalar_cell cell = {0};
    int rc = mylite_execution_scalar_get_format_function_value(database, expression, &cell);

    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else {
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_date_format_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
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
        return plan_row_scalar_date_format_column(
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
        "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_date_format_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT DATE_FORMAT() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "DATE_FORMAT() does not yet support TIME values");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_date_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return plan_row_scalar_get_format_format_argument(
            database,
            expression,
            "DATE_FORMAT() literals do not support NUL bytes",
            mylite_date_format_validate_format,
            out_expression
        );
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "DATE_FORMAT() supports only string format literals and NULL",
        "DATE_FORMAT() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "DATE_FORMAT() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_date_format_validate_format(database, text, text_length);
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_get_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *literal_nul_message,
    int (*validate_format)(struct mylite_db *, const char *, size_t),
    struct planned_row_scalar_expression *out_expression
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    if (out_expression == NULL || validate_format == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_execution_scalar_get_format_function_value(database, expression, &cell);
    if (rc == MYLITE_OK && cell.value == NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
    } else if (rc == MYLITE_OK) {
        size_t text_length = strlen(cell.value);

        if (memchr(cell.value, '\0', text_length) != NULL) {
            set_unsupported_error(database, literal_nul_message);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = validate_format(database, cell.value, text_length);
        }
        if (rc == MYLITE_OK) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            rc = copy_text_value(database, cell.value, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_time_format_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_date_format_input_kind input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    const char *input_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIME_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "TIME_FORMAT");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_time_format_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_time_format_format_argument(
            database,
            child_at(expression, 1U),
            &out_expression->arguments[1]
        );
    }
    input_kind_name = mylite_date_format_input_kind_name(input_kind);
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_time_format_value_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
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
        return plan_row_scalar_time_format_column(
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
        "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, TIMESTAMP, and NULL values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_time_format_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_date_format_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIME_FORMAT() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_time(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_TIME;
    } else if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    } else {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, TIMESTAMP, and NULL values"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_time_format_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return plan_row_scalar_get_format_format_argument(
            database,
            expression,
            "TIME_FORMAT() literals do not support NUL bytes",
            mylite_time_format_validate_format,
            out_expression
        );
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIME_FORMAT() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "TIME_FORMAT() supports only string format literals and NULL",
        "TIME_FORMAT() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIME_FORMAT() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_time_format_validate_format(database, text, text_length);
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_str_to_date_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *value_expression = NULL;
    const struct mylite_sql_ast_node *format_expression = NULL;
    bool handled = false;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_STR_TO_DATE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "STR_TO_DATE");
        return MYLITE_ERROR;
    }

    value_expression = child_at(expression, 0U);
    format_expression = child_at(expression, 1U);
    rc = plan_row_scalar_str_to_date_null_short_circuit(
        database,
        value_expression,
        format_expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &handled,
        out_expression
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_str_to_date_value_argument(
        database,
        value_expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_str_to_date_format_argument(
            database,
            format_expression,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_str_to_date_null_short_circuit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_expression,
    const struct mylite_sql_ast_node *format_expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool *out_handled,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *reference_expression = NULL;
    int rc = MYLITE_OK;

    if (out_handled == NULL || out_expression == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_str_to_date_child_is_null_literal(value_expression) &&
        !mylite_execution_str_to_date_child_is_null_literal(format_expression)) {
        return MYLITE_OK;
    }

    if (mylite_execution_str_to_date_child_is_null_literal(value_expression)) {
        reference_expression = format_expression;
    } else if (mylite_execution_str_to_date_child_is_null_literal(format_expression)) {
        reference_expression = value_expression;
    }
    if (reference_expression != NULL) {
        rc = plan_row_scalar_str_to_date_resolve_short_circuit_references(
            database,
            reference_expression,
            has_source,
            source_context,
            table_columns,
            table_column_count
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
    *out_handled = true;
    return MYLITE_OK;
}

static int plan_row_scalar_str_to_date_resolve_short_circuit_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct mylite_catalog_column_descriptor column = {0};

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

    return resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT STR_TO_DATE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );
}

static int plan_row_scalar_str_to_date_resolve_short_circuit_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (mylite_execution_str_to_date_child_is_identifier_reference(current)) {
            rc = plan_row_scalar_str_to_date_resolve_short_circuit_reference(
                database,
                current,
                has_source,
                source_context,
                table_columns,
                table_column_count
            );
            continue;
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t index = child_count; rc == MYLITE_OK && index > 0U; --index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, index - 1U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int plan_row_scalar_str_to_date_value_argument(
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
        set_unsupported_error(database, "STR_TO_DATE() supports only string and NULL values");
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
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
        return plan_row_scalar_str_to_date_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    set_unsupported_error(database, "STR_TO_DATE() supports only string and NULL values");
    return MYLITE_ERROR;
}

static int plan_row_scalar_str_to_date_column(
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
        "row-scalar SELECT STR_TO_DATE() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!column_descriptor_is_string_family(&column)) {
        set_unsupported_error(database, "STR_TO_DATE() supports only nonbinary string columns");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_str_to_date_format_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return plan_row_scalar_get_format_format_argument(
            database,
            expression,
            "STR_TO_DATE() literals do not support NUL bytes",
            mylite_str_to_date_validate_format,
            out_expression
        );
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "STR_TO_DATE() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "STR_TO_DATE() supports only string format literals and NULL"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        "STR_TO_DATE() supports only string format literals and NULL",
        "STR_TO_DATE() literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "STR_TO_DATE() literals do not support NUL bytes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_str_to_date_validate_format(database, text, text_length);
    }
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        rc = assign_text_value(database, text, text_length, &out_expression->value);
        text = NULL;
    }
    free(text);
    return rc;
}

static int plan_row_scalar_date_format_numeric_equal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *date_format = NULL;
    const struct mylite_sql_ast_node *numeric = NULL;
    int rc = MYLITE_OK;

    if (!mylite_execution_date_format_numeric_equal_sides(expression, &date_format, &numeric)) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format) = "
            "numeric_literal"
        );
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL;
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_date_format_expression(
        database,
        date_format,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK &&
        !planned_date_format_numeric_equal_format_is_supported(&out_expression->arguments[0])) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i') = "
            "numeric_literal"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_date_format_numeric_literal(
            database,
            numeric,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static bool row_scalar_date_format_equal_attempt(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        return false;
    }
    left = unwrap_parenthesized_expression(child_at(expression, 0U));
    right = unwrap_parenthesized_expression(child_at(expression, 1U));
    return ((left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION) ||
            (right != NULL && right->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION)) != 0;
}

static int plan_row_scalar_date_format_numeric_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    bool is_negative = false;
    char *text = NULL;
    size_t text_length = 0U;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "DATE_FORMAT() numeric comparison requires a literal");
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL)) {
        set_unsupported_error(database, "DATE_FORMAT() numeric comparison requires a literal");
        return MYLITE_ERROR;
    }
    text_length = literal->span.length;
    if (is_negative) {
        ++text_length;
    }
    text = (char *)malloc(text_length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        text[0] = '-';
        memcpy(text + 1, literal->span.text, literal->span.length);
    } else {
        memcpy(text, literal->span.text, literal->span.length);
    }
    text[text_length] = '\0';
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    return assign_text_value(database, text, text_length, &out_expression->value);
}

static bool planned_date_format_numeric_equal_format_is_supported(
    const struct planned_row_scalar_expression *expression
) {
    const struct planned_row_scalar_expression *format = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT ||
        expression->argument_count != 3U || expression->arguments == NULL) {
        return false;
    }

    format = &expression->arguments[1];
    if (format->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE || format->value.is_null ||
        !format->value.is_text) {
        return false;
    }

    return mylite_execution_date_format_numeric_equal_format_is_supported(
        format->value.text,
        format->value.text_length
    );
}
