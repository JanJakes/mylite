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
    if (retain_planned_row_scalar_column(database, &out_expression->column, &column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
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
    if (retain_planned_row_scalar_column(database, &out_expression->column, &column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
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

static int plan_row_scalar_addtime_subtime_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const char *function_name = "ADDTIME";
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        !mylite_execution_is_time_arithmetic_function_kind(expression->kind) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_SUBTIME_FUNCTION) {
        function_name = "SUBTIME";
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME;
    } else {
        out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->argument_count = 2U;

    rc = plan_row_scalar_addtime_subtime_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        function_name,
        false,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_addtime_subtime_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            true,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_addtime_subtime_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    bool second_argument,
    struct planned_row_scalar_expression *out_expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            second_argument
                ? "ADDTIME()/SUBTIME() time argument supports only string time literals, TIME "
                  "descriptor columns, string descriptor columns, and NULL"
                : "ADDTIME()/SUBTIME() supports only string temporal literals, TIME, DATETIME, "
                  "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
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
            return plan_row_scalar_addtime_subtime_string_literal(
                database,
                expression,
                function_name,
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
        return plan_row_scalar_addtime_subtime_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            function_name,
            second_argument,
            out_expression
        );
    }

    set_unsupported_error(
        database,
        second_argument
            ? "ADDTIME()/SUBTIME() time argument supports only string time literals, TIME "
              "descriptor columns, string descriptor columns, and NULL"
            : "ADDTIME()/SUBTIME() supports only string temporal literals, TIME, DATETIME, "
              "TIMESTAMP descriptor columns, string descriptor columns, and NULL"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_addtime_subtime_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const char *function_name,
    bool second_argument,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    int rc = resolve_descriptor_column_reference(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar ADDTIME()/SUBTIME() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    (void)function_name;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (second_argument) {
        if (!column_descriptor_is_time(&column) && !column_descriptor_is_string_family(&column)) {
            set_unsupported_error(
                database,
                "ADDTIME()/SUBTIME() time argument supports only TIME and string descriptor "
                "columns"
            );
            return MYLITE_ERROR;
        }
    } else if (!column_descriptor_is_time(&column) && !column_descriptor_is_datetime(&column) &&
               !column_descriptor_is_timestamp(&column) &&
               !column_descriptor_is_string_family(&column)) {
        set_unsupported_error(
            database,
            "ADDTIME()/SUBTIME() supports only TIME, DATETIME, TIMESTAMP, and string descriptor "
            "columns"
        );
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    if (retain_planned_row_scalar_column(database, &out_expression->column, &column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int plan_row_scalar_addtime_subtime_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    struct planned_row_scalar_expression *out_expression
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char *text = NULL;
    size_t text_length = 0U;
    int written = 0;
    int rc = MYLITE_OK;

    written = snprintf(
        unsupported_message,
        sizeof(unsupported_message),
        "%s() supports only string temporal literals, descriptor columns, and NULL",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(unsupported_message)) {
        set_runtime_error(database, "failed to format time arithmetic diagnostic");
        return MYLITE_ERROR;
    }
    written = snprintf(
        nul_message,
        sizeof(nul_message),
        "%s() literals do not support NUL bytes",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(nul_message)) {
        set_runtime_error(database, "failed to format time arithmetic diagnostic");
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

    rc = mylite_execution_timestampdiff_function_unit_from_ast(
        database,
        child_at(expression, 0U),
        &unit
    );
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
    if (retain_planned_row_scalar_column(database, &out_expression->column, &column) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
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
