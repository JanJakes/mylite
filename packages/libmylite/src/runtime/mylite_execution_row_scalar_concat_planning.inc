static int plan_row_scalar_concat_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONCAT;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        const struct mylite_sql_ast_node *unwrapped_argument =
            unwrap_parenthesized_expression(argument);

        if (unwrapped_argument == NULL) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
        } else if (unwrapped_argument->kind == MYLITE_SQL_AST_CONCAT_FUNCTION) {
            set_unsupported_error(
                database,
                "row-scalar SELECT CONCAT() does not support nested CONCAT()"
            );
            rc = MYLITE_ERROR;
        } else {
            if (mylite_execution_string_length_function_kind(unwrapped_argument->kind) !=
                PLANNED_STRING_LENGTH_FUNCTION_NONE) {
                rc = plan_row_scalar_string_length_expression(
                    database,
                    argument,
                    has_source,
                    source_context,
                    table_columns,
                    table_column_count,
                    COLUMN_REFERENCE_FIELD,
                    &out_expression->arguments[argument_index]
                );
            } else if (
                mylite_execution_string_slice_function_kind(unwrapped_argument->kind) !=
                PLANNED_STRING_SLICE_FUNCTION_NONE
            ) {
                rc = plan_row_scalar_string_slice_expression(
                    database,
                    argument,
                    has_source,
                    source_context,
                    table_columns,
                    table_column_count,
                    COLUMN_REFERENCE_FIELD,
                    &out_expression->arguments[argument_index]
                );
            } else {
                rc = plan_row_scalar_non_concat_expression(
                    database,
                    argument,
                    has_source,
                    source_context,
                    table_columns,
                    table_column_count,
                    true,
                    &out_expression->arguments[argument_index]
                );
            }
        }
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_row_scalar_concat_operator_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    size_t argument_count = 0U;
    size_t next_argument = 0U;
    int rc = count_row_scalar_concat_operator_arguments(database, expression, &argument_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONCAT;
    out_expression->argument_count = argument_count;

    rc = plan_row_scalar_concat_operator_arguments(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression->arguments,
        argument_count,
        &next_argument
    );
    if (rc == MYLITE_OK && next_argument != argument_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return rc;
}

static int count_row_scalar_concat_operator_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    size_t *inout_argument_count
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || inout_argument_count == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK && stack.count > 0U) {
        const struct mylite_sql_ast_node *current = NULL;

        --stack.count;
        current = unwrap_parenthesized_expression(stack.items[stack.count]);
        if (current == NULL) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        if (row_scalar_expression_is_concat_operator(current)) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, 1U)) ||
                !scalar_arithmetic_node_stack_push(&stack, child_at(current, 0U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
            continue;
        }
        if (*inout_argument_count == SIZE_MAX) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
            break;
        }
        ++(*inout_argument_count);
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int plan_row_scalar_concat_operator_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *arguments,
    size_t argument_count,
    size_t *inout_next_argument
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || arguments == NULL || inout_next_argument == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK && stack.count > 0U) {
        const struct mylite_sql_ast_node *current = NULL;

        --stack.count;
        current = unwrap_parenthesized_expression(stack.items[stack.count]);
        if (current == NULL) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        if (row_scalar_expression_is_concat_operator(current)) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, 1U)) ||
                !scalar_arithmetic_node_stack_push(&stack, child_at(current, 0U))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
            continue;
        }
        if (current->kind == MYLITE_SQL_AST_CONCAT_FUNCTION ||
            current->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION) {
            set_unsupported_error(
                database,
                "row-scalar SELECT || does not support nested CONCAT()"
            );
            rc = MYLITE_ERROR;
            break;
        }
        if (*inout_next_argument >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }

        if (mylite_execution_string_length_function_kind(current->kind) !=
            PLANNED_STRING_LENGTH_FUNCTION_NONE) {
            rc = plan_row_scalar_string_length_expression(
                database,
                current,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                COLUMN_REFERENCE_FIELD,
                &arguments[*inout_next_argument]
            );
        } else if (
            mylite_execution_string_slice_function_kind(current->kind) !=
            PLANNED_STRING_SLICE_FUNCTION_NONE
        ) {
            rc = plan_row_scalar_string_slice_expression(
                database,
                current,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                COLUMN_REFERENCE_FIELD,
                &arguments[*inout_next_argument]
            );
        } else {
            rc = plan_row_scalar_non_concat_expression(
                database,
                current,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                true,
                &arguments[*inout_next_argument]
            );
        }
        if (rc == MYLITE_OK) {
            ++(*inout_next_argument);
        }
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static bool row_scalar_expression_is_concat_operator(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    return mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_CONCAT;
}

static int plan_row_scalar_concat_ws_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *arguments = child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_WS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        const struct mylite_sql_ast_node *unwrapped_argument =
            unwrap_parenthesized_expression(argument);

        if (unwrapped_argument != NULL &&
            (unwrapped_argument->kind == MYLITE_SQL_AST_CONCAT_FUNCTION ||
             unwrapped_argument->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION)) {
            set_unsupported_error(
                database,
                "row-scalar SELECT CONCAT_WS() does not support nested CONCAT() or CONCAT_WS()"
            );
            rc = MYLITE_ERROR;
        } else {
            rc = plan_row_scalar_concat_ws_argument(
                database,
                argument,
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[argument_index]
            );
        }
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_row_scalar_concat_ws_argument(
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
            "CONCAT_WS() supports only string, integer, boolean, NULL, session scalar, "
            "system variable, scalar subquery, and descriptor column arguments"
        );
        return MYLITE_ERROR;
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
        return plan_row_scalar_concat_ws_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    return plan_row_scalar_non_concat_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        true,
        out_expression
    );
}

static int plan_row_scalar_concat_ws_column(
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
        "row-scalar SELECT CONCAT_WS() supports only descriptor columns",
        table_columns,
        table_column_count,
        &column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!concat_ws_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    return MYLITE_OK;
}

static bool concat_ws_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_string_family(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column) ||
        column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CONCAT_WS() does not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT CONCAT_WS() does not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT CONCAT_WS() supports only integer, DECIMAL, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}
