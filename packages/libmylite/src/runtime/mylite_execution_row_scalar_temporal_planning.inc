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
    int rc = get_format_function_value(database, expression, &cell);

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

    rc = get_format_function_value(database, expression, &cell);
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
    if (!str_to_date_child_is_null_literal(value_expression) &&
        !str_to_date_child_is_null_literal(format_expression)) {
        return MYLITE_OK;
    }

    if (str_to_date_child_is_null_literal(value_expression)) {
        reference_expression = format_expression;
    } else if (str_to_date_child_is_null_literal(format_expression)) {
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
        if (str_to_date_child_is_identifier_reference(current)) {
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

    if (!row_scalar_date_format_numeric_equal_sides(expression, &date_format, &numeric)) {
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

static bool row_scalar_date_format_numeric_equal_sides(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_date_format,
    const struct mylite_sql_ast_node **out_numeric
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (out_date_format == NULL || out_numeric == NULL || expression == NULL ||
        expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        return false;
    }
    left = unwrap_parenthesized_expression(child_at(expression, 0U));
    right = unwrap_parenthesized_expression(child_at(expression, 1U));
    if (left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
        date_format_numeric_literal_expression(right)) {
        *out_date_format = left;
        *out_numeric = right;
        return true;
    }
    if (right != NULL && right->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
        date_format_numeric_literal_expression(left)) {
        *out_date_format = right;
        *out_numeric = left;
        return true;
    }
    return false;
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

static bool date_format_numeric_literal_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    return (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER ||
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_DECIMAL) != 0;
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
    if (!date_format_numeric_literal_expression(expression) || literal == NULL) {
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

static int plan_row_scalar_date_interval_second_expression(
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
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    const char *function_name = "DATE_ADD";
    const char *input_kind_name = NULL;
    const char *unit_name = NULL;
    int subtract_flag = 0;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_date_interval_second_function_kind(expression->kind)) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    function_name = date_interval_second_function_name(expression->kind);
    rc = validate_date_interval_second_function_shape(database, expression, function_name);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (expression->kind != MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        rc = date_interval_unit_from_ast(
            database,
            date_interval_unit_node(expression),
            function_name,
            &unit
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    unit_name = mylite_date_interval_unit_name(unit);
    if (unit_name == NULL) {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, and SECOND interval "
            "units"
        );
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(date_interval_planned_argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND;
    out_expression->argument_count = date_interval_planned_argument_count;

    rc = plan_row_scalar_date_interval_second_temporal_argument(
        database,
        date_interval_second_temporal_node(expression),
        function_name,
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
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_date_interval_second_interval_argument(
            database,
            date_interval_second_interval_node(expression),
            function_name,
            unit,
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_text_value(database, unit_name, &out_expression->arguments[3].value);
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK) {
        if (date_interval_second_function_subtracts(expression->kind)) {
            subtract_flag = 1;
        }
        out_expression->arguments[4].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[4].value =
            (struct planned_value){.is_null = false, .integer = subtract_flag};
    }
    return rc;
}

static int plan_row_scalar_date_interval_second_temporal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
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
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
            "columns, string descriptor columns, and NULL"
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_date_interval_second_literal(
            database,
            expression,
            function_name,
            out_expression
        );
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
        return plan_row_scalar_date_interval_second_column(
            database,
            expression,
            function_name,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    return set_row_scalar_date_interval_second_unsupported_error(
        database,
        function_name,
        "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor columns, "
        "string descriptor columns, and NULL"
    );
}

static int plan_row_scalar_date_interval_second_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
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
        "row-scalar SELECT temporal interval functions support only descriptor columns",
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
    } else if (column_descriptor_is_time(&column)) {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "does not yet support TIME values"
        );
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    } else {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only DATE, DATETIME, TIMESTAMP, string descriptor columns, string temporal "
            "literals, and NULL"
        );
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_date_interval_second_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[date_interval_nul_diagnostic_capacity];
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return set_row_scalar_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
            "columns, string descriptor columns, and NULL"
        );
    }
    if (!date_interval_second_message(
            unsupported_message,
            sizeof(unsupported_message),
            function_name,
            "supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
            "columns, string descriptor columns, and NULL"
        ) ||
        !date_interval_second_message(
            nul_message,
            sizeof(nul_message),
            function_name,
            "date literals do not support NUL bytes"
        )) {
        set_runtime_error(database, "failed to format temporal function diagnostic");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, nul_message);
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

static int plan_row_scalar_date_interval_second_interval_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    enum mylite_date_interval_unit unit,
    struct planned_row_scalar_expression *out_expression
) {
    int64_t interval_seconds = 0;
    bool interval_is_null = false;
    int rc = date_interval_second_interval_argument(
        database,
        function_name,
        expression,
        unit,
        &interval_seconds,
        &interval_is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    if (interval_is_null) {
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
    } else {
        out_expression->value =
            (struct planned_value){.is_null = false, .integer = interval_seconds};
    }
    return MYLITE_OK;
}

static int set_row_scalar_date_interval_second_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (!date_interval_second_message(message, sizeof(message), function_name, suffix)) {
        set_runtime_error(database, "failed to format temporal function diagnostic");
        return MYLITE_ERROR;
    }
    set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_temporal_extract_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    enum mylite_temporal_extract_input_kind input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_STRING;
    const struct mylite_sql_ast_node *argument = NULL;
    const char *extract_kind_name = NULL;
    const char *input_kind_name = NULL;
    int mode = 0;
    int rc = MYLITE_OK;

    rc = resolve_temporal_extract_call(database, expression, &extract_kind, &argument, &mode);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT;
    out_expression->temporal_extract_kind = extract_kind;
    out_expression->argument_count = 4U;

    rc = plan_row_scalar_temporal_extract_argument(
        database,
        argument,
        extract_kind,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &input_kind
    );
    extract_kind_name = mylite_temporal_extract_kind_name(extract_kind);
    input_kind_name = mylite_temporal_extract_input_kind_name(input_kind);
    if (rc == MYLITE_OK && extract_kind_name != NULL) {
        rc = copy_text_value(database, extract_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && input_kind_name != NULL) {
        rc = copy_text_value(database, input_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK) {
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value =
            (struct planned_value){.is_null = false, .integer = mode};
    }
    return rc;
}

static int plan_row_scalar_temporal_extract_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_temporal_extract_input_kind *out_input_kind
) {
    const int is_calendar_name = (int)mylite_temporal_extract_kind_is_calendar_name(extract_kind);
    const int is_calendar_date = (int)mylite_temporal_extract_kind_is_calendar_date(extract_kind);
    const int is_week_temporal = (int)mylite_temporal_extract_kind_is_week_temporal(extract_kind);
    const char *unsupported_argument_message =
        "temporal extract functions support only string temporal literals, descriptor columns, and "
        "NULL";

    if (is_calendar_name != 0) {
        unsupported_argument_message =
            "calendar name functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
    } else if (is_week_temporal != 0) {
        unsupported_argument_message =
            "week temporal functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
    } else if (is_calendar_date != 0) {
        unsupported_argument_message =
            "calendar date functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
    }

    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_argument_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_temporal_extract_literal_value(
                database,
                expression,
                extract_kind,
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
        return plan_row_scalar_temporal_extract_column(
            database,
            expression,
            extract_kind,
            source_context,
            table_columns,
            table_column_count,
            out_expression,
            out_input_kind
        );
    }

    set_unsupported_error(database, unsupported_argument_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_temporal_extract_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    const int is_calendar_name = (int)mylite_temporal_extract_kind_is_calendar_name(extract_kind);
    const int is_calendar_date = (int)mylite_temporal_extract_kind_is_calendar_date(extract_kind);
    const int is_week_temporal = (int)mylite_temporal_extract_kind_is_week_temporal(extract_kind);
    const char *unsupported_message =
        "temporal extract functions support only string temporal literals, descriptor columns, and "
        "NULL";
    const char *nul_message = "temporal extract literals do not support NUL bytes";
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (is_calendar_name != 0) {
        unsupported_message =
            "calendar name functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
        nul_message = "calendar name function literals do not support NUL bytes";
    } else if (is_week_temporal != 0) {
        unsupported_message =
            "week temporal functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
        nul_message = "week temporal function literals do not support NUL bytes";
    } else if (is_calendar_date != 0) {
        unsupported_message =
            "calendar date functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL";
        nul_message = "calendar date function literals do not support NUL bytes";
    }

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        out_expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, nul_message);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = assign_text_value(database, text, text_length, &out_expression->value);
    }
    return rc;
}

static int plan_row_scalar_temporal_extract_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_temporal_extract_kind extract_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_temporal_extract_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT temporal extract functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        if (mylite_temporal_extract_kind_is_time_part(extract_kind) &&
            extract_kind != MYLITE_TEMPORAL_EXTRACT_MICROSECOND &&
            extract_kind != MYLITE_TEMPORAL_EXTRACT_SIGNED_MICROSECOND) {
            return reject_date_column_temporal_extract_time_part(database, extract_kind);
        }
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_DATE;
    } else if (column_descriptor_is_time(&column)) {
        if (mylite_temporal_extract_kind_is_date_part(extract_kind)) {
            return reject_time_column_temporal_extract_date_part(database, extract_kind);
        }
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_TIME;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        if (extract_kind == MYLITE_TEMPORAL_EXTRACT_TIME) {
            set_unsupported_error(
                database,
                "TIME() does not yet support string descriptor columns"
            );
            return MYLITE_ERROR;
        }
        *out_input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_STRING;
    } else {
        return reject_unsupported_temporal_extract_column(database, extract_kind);
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int reject_date_column_temporal_extract_time_part(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
) {
    if (extract_kind == MYLITE_TEMPORAL_EXTRACT_HOUR ||
        extract_kind == MYLITE_TEMPORAL_EXTRACT_MINUTE ||
        extract_kind == MYLITE_TEMPORAL_EXTRACT_SECOND) {
        set_unsupported_error(database, "HOUR()/MINUTE()/SECOND() do not yet support DATE values");
    } else {
        set_unsupported_error(database, "EXTRACT() time units do not yet support DATE values");
    }
    return MYLITE_ERROR;
}

static int reject_time_column_temporal_extract_date_part(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
) {
    if (mylite_temporal_extract_kind_is_calendar_name(extract_kind)) {
        set_unsupported_error(database, "calendar name functions do not yet support TIME values");
    } else if (mylite_temporal_extract_kind_is_week_temporal(extract_kind)) {
        set_unsupported_error(database, "week temporal functions do not yet support TIME values");
    } else if (mylite_temporal_extract_kind_is_calendar_date(extract_kind)) {
        set_unsupported_error(database, "calendar date functions do not yet support TIME values");
    } else if (extract_kind == MYLITE_TEMPORAL_EXTRACT_QUARTER) {
        set_unsupported_error(
            database,
            "DATE()/YEAR()/QUARTER()/MONTH()/DAY()/DAYOFMONTH() do not support TIME values"
        );
    } else if (extract_kind == MYLITE_TEMPORAL_EXTRACT_YEAR_MONTH) {
        set_unsupported_error(database, "EXTRACT() date units do not support TIME values");
    } else {
        set_unsupported_error(
            database,
            "DATE()/YEAR()/MONTH()/DAY()/DAYOFMONTH() do not support TIME values"
        );
    }
    return MYLITE_ERROR;
}

static int reject_unsupported_temporal_extract_column(
    struct mylite_db *database,
    enum mylite_temporal_extract_kind extract_kind
) {
    if (mylite_temporal_extract_kind_is_calendar_name(extract_kind)) {
        set_unsupported_error(
            database,
            "calendar name functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
        );
    } else if (mylite_temporal_extract_kind_is_week_temporal(extract_kind)) {
        set_unsupported_error(
            database,
            "week temporal functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
        );
    } else if (mylite_temporal_extract_kind_is_calendar_date(extract_kind)) {
        set_unsupported_error(
            database,
            "calendar date functions support only string temporal literals, DATE, DATETIME, "
            "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
        );
    } else {
        set_unsupported_error(
            database,
            "temporal extract functions support only DATE, TIME, DATETIME, TIMESTAMP, string, "
            "and NULL values"
        );
    }
    return MYLITE_ERROR;
}

static int plan_row_scalar_sec_to_time_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "SEC_TO_TIME");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_sec_to_time_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_sec_to_time_argument(
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
            "SEC_TO_TIME() supports only signed integer, boolean, NULL, and integer descriptor "
            "arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return plan_row_scalar_integer_value(database, expression, out_expression);
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
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
        return plan_row_scalar_sec_to_time_column(
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
        "SEC_TO_TIME() supports only signed integer, boolean, NULL, and integer descriptor "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_sec_to_time_column(
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
        "row-scalar SELECT SEC_TO_TIME() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(column.physical_type, "INTEGER") != 0) {
        set_unsupported_error(database, "SEC_TO_TIME() supports only integer descriptor columns");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_from_unixtime_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FROM_UNIXTIME_FUNCTION) {
        set_native_function_parameter_count_error(database, "FROM_UNIXTIME");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count == 2U) {
        set_unsupported_error(database, "FROM_UNIXTIME() format arguments are not yet supported");
        return MYLITE_ERROR;
    }
    if (child_count != 1U) {
        set_native_function_parameter_count_error(database, "FROM_UNIXTIME");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME;
    out_expression->argument_count = 1U;

    rc = plan_row_scalar_from_unixtime_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    return rc;
}

static int plan_row_scalar_from_unixtime_argument(
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
            "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer "
            "descriptor arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return plan_row_scalar_from_unixtime_integer_value(
                database,
                expression,
                out_expression
            );
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_from_unixtime_integer_value(database, expression, out_expression);
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
        return plan_row_scalar_from_unixtime_column(
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
        "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer descriptor "
        "arguments"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_from_unixtime_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer "
                "descriptor arguments"
            );
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "FROM_UNIXTIME() supports only signed integer, boolean, NULL, and integer descriptor "
            "arguments"
        );
        return MYLITE_ERROR;
    }

    rc = from_unixtime_scalar_integer_literal_value(database, literal, is_negative, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    out_expression->value = (struct planned_value){.is_null = false, .integer = value};
    return MYLITE_OK;
}

static int plan_row_scalar_from_unixtime_column(
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
        "row-scalar SELECT FROM_UNIXTIME() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(column.physical_type, "INTEGER") != 0) {
        set_unsupported_error(database, "FROM_UNIXTIME() supports only integer descriptor columns");
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_temporal_constructor_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t argument_count = 0U;
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || !is_temporal_constructor_function_kind(expression->kind)) {
        set_unsupported_error(database, "temporal constructor function is not supported");
        return MYLITE_ERROR;
    }

    argument_count = temporal_constructor_function_argument_count(expression->kind);
    function_name = temporal_constructor_function_name(expression->kind);
    if (mylite_sql_ast_node_child_count(expression) != argument_count) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = temporal_constructor_planned_kind(expression->kind);
    out_expression->argument_count = argument_count;

    for (size_t index = 0U; rc == MYLITE_OK && index < argument_count; ++index) {
        rc = plan_row_scalar_temporal_constructor_argument(
            database,
            child_at(expression, index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            &out_expression->arguments[index]
        );
    }
    return rc;
}

static int plan_row_scalar_temporal_constructor_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (snprintf(
            unsupported_message,
            sizeof(unsupported_message),
            "%s() supports only signed integer, boolean, NULL, and integer descriptor arguments",
            function_name == NULL ? "temporal constructor" : function_name
        ) < 0) {
        set_runtime_error(database, "failed to format temporal constructor diagnostic");
        return MYLITE_ERROR;
    }

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 1};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = false, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
            return MYLITE_OK;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return plan_row_scalar_integer_value(database, expression, out_expression);
        }
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
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
        return plan_row_scalar_temporal_constructor_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            out_expression
        );
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_temporal_constructor_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT temporal constructors support only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (strcmp(column.physical_type, "INTEGER") != 0) {
        char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

        if (snprintf(
                message,
                sizeof(message),
                "%s() supports only integer descriptor columns",
                function_name == NULL ? "temporal constructor" : function_name
            ) < 0) {
            set_runtime_error(database, "failed to format temporal constructor diagnostic");
            return MYLITE_ERROR;
        }
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static size_t temporal_constructor_function_argument_count(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
        return 1U;
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
        return 2U;
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return 3U;
    default:
        break;
    }
    return 0U;
}

static const char *temporal_constructor_function_name(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
        return "FROM_DAYS";
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
        return "MAKEDATE";
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return "MAKETIME";
    default:
        break;
    }
    return "temporal constructor";
}

static enum planned_row_scalar_expression_kind temporal_constructor_planned_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS;
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE;
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME;
    default:
        break;
    }
    return PLANNED_ROW_SCALAR_EXPRESSION_NONE;
}

static bool is_temporal_constructor_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_FROM_DAYS_FUNCTION:
    case MYLITE_SQL_AST_MAKEDATE_FUNCTION:
    case MYLITE_SQL_AST_MAKETIME_FUNCTION:
        return true;
    default:
        break;
    }
    return false;
}

static int plan_row_scalar_period_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    function_name =
        period_function_name(expression == NULL ? MYLITE_SQL_AST_SCRIPT : expression->kind);
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_PERIOD_ADD_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = period_planned_kind(expression->kind);
    out_expression->argument_count = 2U;

    for (size_t index = 0U; rc == MYLITE_OK && index < 2U; ++index) {
        rc = plan_row_scalar_temporal_constructor_argument(
            database,
            child_at(expression, index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            &out_expression->arguments[index]
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static const char *period_function_name(enum mylite_sql_ast_node_kind ast_kind) {
    if (ast_kind == MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION) {
        return "PERIOD_DIFF";
    }
    return "PERIOD_ADD";
}

static enum planned_row_scalar_expression_kind period_planned_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    if (ast_kind == MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION) {
        return PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF;
    }
    return PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD;
}

static int plan_row_scalar_convert_tz_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const char *temporal_message =
        "CONVERT_TZ() supports only string, DATE, DATETIME, TIMESTAMP, and NULL datetime values";
    const char *zone_message = "CONVERT_TZ() supports only string and NULL time-zone values";
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONVERT_TZ_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "CONVERT_TZ");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_convert_tz_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        temporal_message,
        true,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_convert_tz_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            zone_message,
            false,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_convert_tz_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            zone_message,
            false,
            &out_expression->arguments[2]
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int plan_row_scalar_convert_tz_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    bool temporal_argument,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, unsupported_message);
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
        return plan_row_scalar_convert_tz_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            unsupported_message,
            temporal_argument,
            out_expression
        );
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_convert_tz_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *unsupported_message,
    bool temporal_argument,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT CONVERT_TZ() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_string_family(&column) ||
        (temporal_argument &&
         (column_descriptor_is_date(&column) || column_descriptor_is_datetime(&column) ||
          column_descriptor_is_timestamp(&column)))) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
        out_expression->column = column;
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int plan_row_scalar_weight_string_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    struct session_scalar_cell cell = {0};
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION)) {
        set_unsupported_error(database, "WEIGHT_STRING() expression is unsupported");
        return MYLITE_ERROR;
    }
    argument_count = expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION ? 1U : 2U;
    if (mylite_sql_ast_node_child_count(expression) != argument_count) {
        set_unsupported_error(database, "WEIGHT_STRING() expression is unsupported");
        return MYLITE_ERROR;
    }

    argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (has_source && argument != NULL &&
        (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
         argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        out_expression->arguments = (struct planned_row_scalar_expression *)
            calloc(argument_count, sizeof(*out_expression->arguments));
        if (out_expression->arguments == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        out_expression->kind = expression->kind == MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION
                                   ? PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING
                                   : PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY;
        out_expression->argument_count = argument_count;

        rc = plan_row_scalar_weight_string_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[0]
        );
        if (rc == MYLITE_OK && argument_count == 2U) {
            rc = plan_row_scalar_integer_value(
                database,
                child_at(expression, 1U),
                &out_expression->arguments[1]
            );
        }
        if (rc != MYLITE_OK) {
            planned_row_scalar_expression_deinit(out_expression);
        }
        return rc;
    }

    rc = mylite_execution_scalar_weight_string_function_value(database, expression, &cell);
    if (rc == MYLITE_OK) {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        if (cell.value == NULL) {
            out_expression->value = (struct planned_value){.is_null = true, .integer = 0};
        } else {
            rc = copy_blob_value(database, cell.value, cell.value_size, &out_expression->value);
        }
    }
    session_scalar_cell_deinit(&cell);
    return rc;
}

static int plan_row_scalar_weight_string_argument(
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
            "WEIGHT_STRING() supports only scalar literals and descriptor columns"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (!has_source) {
            return mylite_execution_set_unknown_column_reference_error(database, expression);
        }
        return plan_row_scalar_weight_string_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return plan_row_scalar_literal_value(database, expression, out_expression);
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }

    set_unsupported_error(
        database,
        "WEIGHT_STRING() supports only scalar literals and descriptor columns"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_weight_string_column(
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
        "row-scalar SELECT WEIGHT_STRING() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!weight_string_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool weight_string_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column) ||
        column_descriptor_is_binary_string_family(column) ||
        column_descriptor_is_binary_blob_family(column)) {
        return true;
    }

    set_unsupported_error(
        database,
        "WEIGHT_STRING() supports only integer, string, binary string, and BLOB columns"
    );
    return false;
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

    return date_format_numeric_equal_format_is_supported(
        format->value.text,
        format->value.text_length
    );
}

static int plan_row_scalar_datediff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_datediff_input_kind left_kind = MYLITE_DATEDIFF_INPUT_STRING;
    enum mylite_datediff_input_kind right_kind = MYLITE_DATEDIFF_INPUT_STRING;
    const char *left_kind_name = NULL;
    const char *right_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_DATEDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "DATEDIFF");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF;
    out_expression->argument_count = 4U;

    rc = plan_row_scalar_datediff_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0],
        &left_kind
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_datediff_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2],
            &right_kind
        );
    }
    left_kind_name = mylite_datediff_input_kind_name(left_kind);
    right_kind_name = mylite_datediff_input_kind_name(right_kind);
    if (rc == MYLITE_OK && left_kind_name != NULL) {
        rc = copy_text_value(database, left_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && right_kind_name != NULL) {
        rc = copy_text_value(database, right_kind_name, &out_expression->arguments[3].value);
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_datediff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_datediff_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_DATEDIFF_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
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
            return plan_row_scalar_datediff_string_literal(database, expression, out_expression);
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
        return plan_row_scalar_datediff_column(
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
        "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor "
        "columns, string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_datediff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_datediff_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT DATEDIFF() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "DATEDIFF() does not yet support TIME values");
        return MYLITE_ERROR;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_DATEDIFF_INPUT_STRING;
    } else {
        set_unsupported_error(
            database,
            "DATEDIFF() supports only DATE, DATETIME, TIMESTAMP, string descriptor columns, "
            "string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_datediff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "DATEDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL",
        "DATEDIFF() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "DATEDIFF() literals do not support NUL bytes");
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

static int plan_row_scalar_timediff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_timediff_input_kind left_kind = MYLITE_TIMEDIFF_INPUT_STRING;
    enum mylite_timediff_input_kind right_kind = MYLITE_TIMEDIFF_INPUT_STRING;
    const struct mylite_sql_ast_node *left_argument = NULL;
    const char *left_kind_name = NULL;
    const char *right_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIMEDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "TIMEDIFF");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(4U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF;
    out_expression->argument_count = 4U;

    left_argument = unwrap_parenthesized_expression(child_at(expression, 0U));
    if (left_argument != NULL && left_argument->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(left_argument) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[0].value = (struct planned_value){.is_null = true, .integer = 0};
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[2].value = (struct planned_value){.is_null = true, .integer = 0};
    } else {
        rc = plan_row_scalar_timediff_argument(
            database,
            child_at(expression, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[0],
            &left_kind
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_timediff_argument(
                database,
                child_at(expression, 1U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[2],
                &right_kind
            );
        }
    }
    left_kind_name = mylite_timediff_input_kind_name(left_kind);
    right_kind_name = mylite_timediff_input_kind_name(right_kind);
    if (rc == MYLITE_OK && left_kind_name != NULL) {
        rc = copy_text_value(database, left_kind_name, &out_expression->arguments[1].value);
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && right_kind_name != NULL) {
        rc = copy_text_value(database, right_kind_name, &out_expression->arguments[3].value);
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_timediff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timediff_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_TIMEDIFF_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
            "descriptor columns, and NULL"
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
            return plan_row_scalar_timediff_string_literal(database, expression, out_expression);
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
        return plan_row_scalar_timediff_column(
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
        "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timediff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timediff_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIMEDIFF() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_DATE;
    } else if (column_descriptor_is_time(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_TIME;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_TIMEDIFF_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_string_family(&column)) {
        set_unsupported_error(
            database,
            "TIMEDIFF() does not yet support string descriptor columns"
        );
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(
            database,
            "TIMEDIFF() supports only DATE, TIME, DATETIME, TIMESTAMP descriptor columns, string "
            "temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timediff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "TIMEDIFF() supports only string temporal literals, DATE, TIME, DATETIME, TIMESTAMP "
        "descriptor columns, and NULL",
        "TIMEDIFF() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMEDIFF() literals do not support NUL bytes");
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

static int plan_row_scalar_timestampdiff_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_timestampdiff_unit unit = MYLITE_TIMESTAMPDIFF_UNIT_DAY;
    enum mylite_timestampdiff_input_kind left_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    enum mylite_timestampdiff_input_kind right_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    const struct mylite_sql_ast_node *left_argument = NULL;
    const char *unit_name = NULL;
    const char *left_kind_name = NULL;
    const char *right_kind_name = NULL;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIMESTAMPDIFF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_runtime_error(database, "invalid TIMESTAMPDIFF() syntax");
        return MYLITE_ERROR;
    }

    rc = timestampdiff_function_unit_from_ast(database, child_at(expression, 0U), &unit);
    if (rc == MYLITE_OK && unit == MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND) {
        set_unsupported_error(database, "TIMESTAMPDIFF() does not yet support MICROSECOND");
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(planned_row_scalar_timestampdiff_argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF;
    out_expression->argument_count = planned_row_scalar_timestampdiff_argument_count;

    unit_name = mylite_timestampdiff_unit_name(unit);
    if (unit_name != NULL) {
        rc = copy_text_value(database, unit_name, &out_expression->arguments[0].value);
        out_expression->arguments[0].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    left_argument = unwrap_parenthesized_expression(child_at(expression, 1U));
    if (rc == MYLITE_OK && left_argument != NULL && left_argument->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(left_argument) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_expression->arguments[1].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[1].value = (struct planned_value){.is_null = true, .integer = 0};
        out_expression->arguments[3].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
        out_expression->arguments[3].value = (struct planned_value){.is_null = true, .integer = 0};
    } else if (rc == MYLITE_OK) {
        rc = plan_row_scalar_timestampdiff_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1],
            &left_kind
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_timestampdiff_argument(
                database,
                child_at(expression, 2U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[3],
                &right_kind
            );
        }
    }
    left_kind_name = mylite_timestampdiff_input_kind_name(left_kind);
    right_kind_name = mylite_timestampdiff_input_kind_name(right_kind);
    if (rc == MYLITE_OK && left_kind_name != NULL) {
        rc = copy_text_value(database, left_kind_name, &out_expression->arguments[2].value);
        out_expression->arguments[2].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    if (rc == MYLITE_OK && right_kind_name != NULL) {
        rc = copy_text_value(database, right_kind_name, &out_expression->arguments[4].value);
        out_expression->arguments[4].kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    }
    return rc;
}

static int plan_row_scalar_timestampdiff_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timestampdiff_input_kind *out_input_kind
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_input_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
            "descriptor columns, string descriptor columns, and NULL"
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
            return plan_row_scalar_timestampdiff_string_literal(
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
        return plan_row_scalar_timestampdiff_column(
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
        "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_timestampdiff_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression,
    enum mylite_timestampdiff_input_kind *out_input_kind
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT TIMESTAMPDIFF() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_date(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_DATE;
    } else if (column_descriptor_is_datetime(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_DATETIME;
    } else if (column_descriptor_is_timestamp(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_TIMESTAMP;
    } else if (column_descriptor_is_time(&column)) {
        set_unsupported_error(database, "TIMESTAMPDIFF() does not yet support TIME values");
        return MYLITE_ERROR;
    } else if (column_descriptor_is_string_family(&column)) {
        *out_input_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    } else {
        set_unsupported_error(
            database,
            "TIMESTAMPDIFF() supports only DATE, DATETIME, TIMESTAMP, string descriptor "
            "columns, string temporal literals, and NULL"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static int plan_row_scalar_timestampdiff_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_row_scalar_expression *out_expression
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        expression,
        "TIMESTAMPDIFF() supports only string temporal literals, DATE, DATETIME, TIMESTAMP "
        "descriptor columns, string descriptor columns, and NULL",
        "TIMESTAMPDIFF() literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "TIMESTAMPDIFF() literals do not support NUL bytes");
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
