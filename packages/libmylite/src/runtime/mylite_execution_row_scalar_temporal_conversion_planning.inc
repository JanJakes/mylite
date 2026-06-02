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

    rc = mylite_execution_from_unixtime_scalar_integer_literal_value(
        database,
        literal,
        is_negative,
        &value
    );
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
