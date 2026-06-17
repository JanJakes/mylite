static int scalar_subquery_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *statement = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SCALAR_SUBQUERY ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "scalar subquery supports only parenthesized SELECT");
        return MYLITE_ERROR;
    }

    statement = child_at(expression, 0U);
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "scalar subquery supports only parenthesized SELECT");
        return MYLITE_ERROR;
    }

    return scalar_subquery_select_value(database, statement, out_cell);
}

static int scalar_subquery_select_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *child = NULL;

    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_options(statement) != 0U ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0 ||
        mylite_sql_ast_node_select_locking_clause(statement) !=
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        set_unsupported_error(database, "scalar subquery supports only plain inner SELECT");
        return MYLITE_ERROR;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_unsupported_error(database, "scalar subquery supports only one explicit select item");
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        if (select_statement_has_no_source_or_dual(statement)) {
            set_no_tables_used_error(database);
            return MYLITE_ERROR;
        }
        set_unsupported_error(database, "scalar subquery does not support table-backed SELECT");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_scalar_subquery_column_count_error(database);
        return MYLITE_ERROR;
    }

    child = child_at(statement, 1U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_FROM_DUAL) {
            child = child->next_sibling;
            continue;
        }
        if (child->kind == MYLITE_SQL_AST_FROM_TABLE) {
            set_unsupported_error(database, "scalar subquery does not support table-backed SELECT");
            return MYLITE_ERROR;
        }
        set_unsupported_error(
            database,
            "scalar subquery supports only no-source or DUAL inner SELECT"
        );
        return MYLITE_ERROR;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_unsupported_error(database, "scalar subquery supports only one explicit select item");
        return MYLITE_ERROR;
    }
    return scalar_subquery_inner_value(database, child_at(select_item, 0U), out_cell);
}

static int scalar_subquery_inner_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(database, "scalar subquery supports only scalar inner values");
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        if (database->session.has_selected_schema) {
            out_cell->value = database->session.selected_schema;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL:
        return literal_projection_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        if (is_scalar_projection_literal_expression(expression)) {
            return literal_projection_value(database, expression, out_cell);
        }
        break;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return system_variable_value(database, expression, out_cell);
    default:
        break;
    }

    set_unsupported_error(
        database,
        "scalar subquery supports only DATABASE(), SCHEMA(), integer, boolean, NULL, "
        "and system variable inner values"
    );
    return MYLITE_ERROR;
}

static int session_scalar_value_without_case(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
        return row_constructor_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_LITERAL:
        return literal_projection_value(database, expression, out_cell);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        if (is_row_constructor_projection_expression(expression)) {
            return row_constructor_scalar_value(database, expression, out_cell);
        }
        return session_unary_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return session_binary_scalar_value(database, expression, out_cell);
    case MYLITE_SQL_AST_NOT_PREDICATE:
        if (is_row_constructor_projection_expression(expression)) {
            return row_constructor_scalar_value(database, expression, out_cell);
        }
        return scalar_comparison_value(database, expression, out_cell);
    case MYLITE_SQL_AST_MOD_FUNCTION:
        return scalar_arithmetic_value(database, expression, out_cell);
    case MYLITE_SQL_AST_IF_FUNCTION:
        return if_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return ifnull_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return coalesce_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return nullif_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return isnull_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
        return greatest_least_function_value(database, expression, out_cell);
    case MYLITE_SQL_AST_INTERVAL_FUNCTION:
        return interval_function_value(database, expression, out_cell);
    default:
        return MYLITE_OK;
    }
}

static int session_unary_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    if (is_scalar_projection_literal_expression(expression)) {
        return literal_projection_value(database, expression, out_cell);
    }
    if (is_scalar_bitwise_unary_operator(mylite_sql_ast_node_operator(expression))) {
        return mylite_execution_scalar_bitwise_value(database, expression, out_cell);
    }
    if (is_scalar_logical_unary_operator(mylite_sql_ast_node_operator(expression))) {
        return scalar_logical_value(database, expression, out_cell);
    }
    return scalar_arithmetic_value(database, expression, out_cell);
}

static int session_binary_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    if (is_row_constructor_projection_expression(expression)) {
        return row_constructor_scalar_value(database, expression, out_cell);
    }
    if (row_scalar_expression_is_concat_operator(expression)) {
        return scalar_concat_operator_value(database, expression, out_cell);
    }
    if (mylite_execution_is_scalar_division_projection_expression(expression)) {
        return mylite_execution_scalar_division_value(database, expression, out_cell);
    }
    if (is_scalar_bitwise_projection_expression(expression)) {
        return mylite_execution_scalar_bitwise_value(database, expression, out_cell);
    }
    if (is_scalar_logical_projection_expression(expression)) {
        return scalar_logical_value(database, expression, out_cell);
    }
    if (mylite_execution_scalar_is_date_format_comparison_expression(expression)) {
        return mylite_execution_scalar_date_format_numeric_comparison_value(
            database,
            expression,
            out_cell
        );
    }
    if (is_scalar_comparison_projection_expression(expression)) {
        return scalar_comparison_value(database, expression, out_cell);
    }
    return scalar_arithmetic_value(database, expression, out_cell);
}

static int scalar_concat_operator_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct mylite_string_concat_argument *arguments = NULL;
    struct session_scalar_cell *cells = NULL;
    char **owned_texts = NULL;
    char *result = NULL;
    size_t argument_count = 0U;
    size_t next_argument = 0U;
    size_t result_length = 0U;
    int rc = count_row_scalar_concat_operator_arguments(database, expression, &argument_count);

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count == 0U || argument_count > SIZE_MAX / sizeof(*arguments) ||
        argument_count > SIZE_MAX / sizeof(*cells) ||
        argument_count > SIZE_MAX / sizeof(*owned_texts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    arguments = (struct mylite_string_concat_argument *)calloc(argument_count, sizeof(*arguments));
    cells = (struct session_scalar_cell *)calloc(argument_count, sizeof(*cells));
    owned_texts = (char **)calloc(argument_count, sizeof(*owned_texts));
    if (arguments == NULL || cells == NULL || owned_texts == NULL) {
        free(arguments);
        free(cells);
        free((void *)owned_texts);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = evaluate_concat_operator_scalar_arguments(
        database,
        expression,
        cells,
        owned_texts,
        arguments,
        argument_count,
        &next_argument
    );
    if (rc == MYLITE_OK && next_argument != argument_count) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = scalar_concat_operator_result(arguments, argument_count, &result, &result_length);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    if (rc == MYLITE_OK && result != NULL) {
        out_cell->owned_text = result;
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = result_length;
        out_cell->has_value_size = true;
        result = NULL;
    }

    free(result);
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        free(owned_texts[argument_index]);
        session_scalar_cell_deinit(&cells[argument_index]);
    }
    free((void *)owned_texts);
    free(cells);
    free(arguments);
    return rc;
}

int mylite_execution_scalar_concat_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return scalar_concat_function_value(database, expression, out_cell);
}

static int scalar_concat_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct mylite_string_concat_argument *arguments = NULL;
    struct session_scalar_cell *cells = NULL;
    char **owned_texts = NULL;
    char *result = NULL;
    size_t argument_count = 0U;
    size_t next_argument = 0U;
    size_t result_length = 0U;
    int rc = count_row_scalar_concat_function_arguments(database, expression, &argument_count);

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument_count == 0U || argument_count > SIZE_MAX / sizeof(*arguments) ||
        argument_count > SIZE_MAX / sizeof(*cells) ||
        argument_count > SIZE_MAX / sizeof(*owned_texts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    arguments = (struct mylite_string_concat_argument *)calloc(argument_count, sizeof(*arguments));
    cells = (struct session_scalar_cell *)calloc(argument_count, sizeof(*cells));
    owned_texts = (char **)calloc(argument_count, sizeof(*owned_texts));
    if (arguments == NULL || cells == NULL || owned_texts == NULL) {
        free(arguments);
        free(cells);
        free((void *)owned_texts);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = evaluate_concat_function_scalar_arguments(
        database,
        expression,
        cells,
        owned_texts,
        arguments,
        argument_count,
        &next_argument
    );
    if (rc == MYLITE_OK && next_argument != argument_count) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = scalar_concat_operator_result(arguments, argument_count, &result, &result_length);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    if (rc == MYLITE_OK && result != NULL) {
        out_cell->owned_text = result;
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = result_length;
        out_cell->has_value_size = true;
        result = NULL;
    }

    free(result);
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        free(owned_texts[argument_index]);
        session_scalar_cell_deinit(&cells[argument_index]);
    }
    free((void *)owned_texts);
    free(cells);
    free(arguments);
    return rc;
}

static int evaluate_concat_function_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cells,
    char **owned_texts,
    struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    size_t *inout_next_argument
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || cells == NULL || owned_texts == NULL || arguments == NULL ||
        inout_next_argument == NULL) {
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
        if (current->kind == MYLITE_SQL_AST_CONCAT_FUNCTION) {
            const struct mylite_sql_ast_node *function_arguments = child_at(current, 0U);
            size_t function_argument_count = 0U;

            if (mylite_sql_ast_node_child_count(current) != 1U || function_arguments == NULL ||
                function_arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
                set_unsupported_error(database, "CONCAT() requires one or more arguments");
                rc = MYLITE_ERROR;
                break;
            }
            function_argument_count = mylite_sql_ast_node_child_count(function_arguments);
            if (function_argument_count == 0U) {
                set_unsupported_error(database, "CONCAT() requires one or more arguments");
                rc = MYLITE_ERROR;
                break;
            }
            for (size_t index = 0U; index < function_argument_count; ++index) {
                const struct mylite_sql_ast_node *argument =
                    child_at(function_arguments, function_argument_count - index - 1U);

                if (!scalar_arithmetic_node_stack_push(&stack, argument)) {
                    set_nomem_error(database);
                    rc = MYLITE_NOMEM;
                    break;
                }
            }
            continue;
        }
        if (*inout_next_argument >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }

        rc = evaluate_concat_operator_scalar_argument(
            database,
            current,
            &cells[*inout_next_argument],
            &owned_texts[*inout_next_argument],
            &arguments[*inout_next_argument].text,
            &arguments[*inout_next_argument].text_length,
            &arguments[*inout_next_argument].is_null
        );
        if (rc == MYLITE_OK) {
            ++(*inout_next_argument);
        }
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int evaluate_concat_operator_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *cells,
    char **owned_texts,
    struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    size_t *inout_next_argument
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (expression == NULL || cells == NULL || owned_texts == NULL || arguments == NULL ||
        inout_next_argument == NULL) {
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
        if (*inout_next_argument >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }

        rc = evaluate_concat_operator_scalar_argument(
            database,
            current,
            &cells[*inout_next_argument],
            &owned_texts[*inout_next_argument],
            &arguments[*inout_next_argument].text,
            &arguments[*inout_next_argument].text_length,
            &arguments[*inout_next_argument].is_null
        );
        if (rc == MYLITE_OK) {
            ++(*inout_next_argument);
        }
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int evaluate_concat_operator_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = unwrap_parenthesized_expression(expression);
    if (!concat_operator_scalar_argument_is_admitted(expression)) {
        set_unsupported_error(
            database,
            "|| supports only string, integer, boolean, NULL, session scalar, scalar subquery, "
            "system variable, and limited integer arithmetic operands"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal(
                database,
                expression,
                "|| supports only string literals",
                "|| string literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = literal_projection_value(database, expression, inout_cell);
    } else if (concat_operator_scalar_argument_is_integer_arithmetic_expression(expression)) {
        rc = concat_operator_scalar_arithmetic_argument_value(database, expression, inout_cell);
    } else if (expression->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        rc = scalar_subquery_value(database, expression, inout_cell);
    } else if (concat_operator_scalar_argument_is_nested_scalar_value(expression)) {
        rc = session_scalar_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    *out_text = inout_cell->value;
    *out_text_length =
        inout_cell->has_value_size ? inout_cell->value_size : strlen(inout_cell->value);
    return MYLITE_OK;
}

static int concat_operator_scalar_arithmetic_argument_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = evaluate_integer_default_expression(database, expression, &value);

    if (rc != MYLITE_OK) {
        set_scalar_arithmetic_unsupported_error(database);
        return rc;
    }
    if (value.is_null) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }

    int written =
        snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, value.integer);

    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format || arithmetic operand");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int scalar_concat_operator_result(
    const struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    char **out_text,
    size_t *out_text_length
) {
    char *result = NULL;
    size_t total_length = 0U;
    size_t offset = 0U;

    if (arguments == NULL || argument_count == 0U || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;

    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        const struct mylite_string_concat_argument *argument = &arguments[argument_index];

        if (argument->is_null) {
            return MYLITE_OK;
        }
        if (argument->text == NULL && argument->text_length != 0U) {
            return MYLITE_MISUSE;
        }
        if (total_length > SIZE_MAX - argument->text_length) {
            return MYLITE_NOMEM;
        }
        total_length += argument->text_length;
    }

    if (total_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    result = (char *)malloc(total_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        const struct mylite_string_concat_argument *argument = &arguments[argument_index];

        if (argument->text_length != 0U) {
            memcpy(&result[offset], argument->text, argument->text_length);
            offset += argument->text_length;
        }
    }
    result[offset] = '\0';
    *out_text = result;
    *out_text_length = total_length;
    return MYLITE_OK;
}

static bool concat_operator_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        return true;
    }
    if (concat_operator_scalar_argument_is_integer_arithmetic_expression(expression)) {
        return true;
    }
    if (concat_operator_scalar_argument_is_nested_scalar_value(expression)) {
        return true;
    }
    return mylite_execution_string_length_scalar_argument_is_admitted(expression);
}

static bool concat_operator_scalar_argument_is_nested_scalar_value(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
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
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
        return true;
    default:
        return false;
    }
}

static bool concat_operator_scalar_argument_is_integer_arithmetic_expression(
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    bool saw_arithmetic = false;
    bool result = true;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return false;
    }
    while (stack.count != 0U && result) {
        result = concat_operator_scalar_arithmetic_node_is_admitted(
            stack.items[--stack.count],
            &saw_arithmetic,
            &stack
        );
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    return (result && saw_arithmetic) != 0;
}

static bool concat_operator_scalar_arithmetic_node_is_admitted(
    const struct mylite_sql_ast_node *expression,
    bool *inout_saw_arithmetic,
    struct scalar_arithmetic_node_stack *stack
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind =
            mylite_sql_ast_node_literal_kind(expression);

        return (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        if (mylite_sql_ast_node_child_count(expression) != 2U) {
            return false;
        }
        *inout_saw_arithmetic = true;
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    if (expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        if (!integer_default_binary_operator_is_supported(mylite_sql_ast_node_operator(expression)
            )) {
            return false;
        }
        *inout_saw_arithmetic = true;
        if (!scalar_arithmetic_node_stack_push(stack, child_at(expression, 1U))) {
            return false;
        }
        return scalar_arithmetic_node_stack_push(stack, child_at(expression, 0U));
    }
    return false;
}

static int elt_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *selected_value = NULL;
    struct field_scalar_argument value = {0};
    int64_t selector = 0;
    bool selector_is_null = false;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_ELT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "ELT() supports only ELT(index, value, ...)");
        return MYLITE_ERROR;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, "ELT");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        set_native_function_parameter_count_error(database, "ELT");
        return MYLITE_ERROR;
    }

    rc = evaluate_elt_scalar_index(database, child_at(arguments, 0U), &selector, &selector_is_null);
    if (rc != MYLITE_OK || selector_is_null || selector < 1) {
        return rc;
    }
    if ((uint64_t)selector > (uint64_t)(argument_count - 1U)) {
        return MYLITE_OK;
    }

    selected_value = child_at(arguments, (size_t)selector);
    rc = evaluate_elt_scalar_value(database, selected_value, &value);
    if (rc == MYLITE_OK) {
        rc = format_elt_scalar_result(database, &value, out_cell);
    }
    field_scalar_argument_deinit(&value);
    return rc;
}

static int evaluate_elt_scalar_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_index,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool has_sign = false;
    bool is_negative = false;
    uint64_t magnitude = 0U;

    if (out_index == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index = 0;
    *out_is_null = false;

    if (concat_operator_scalar_argument_is_integer_arithmetic_expression(literal)) {
        struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
        int rc = evaluate_integer_default_expression(database, literal, &value);

        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "ELT() index supports only signed integer, boolean, and NULL literals"
            );
            return MYLITE_ERROR;
        }
        *out_is_null = value.is_null;
        *out_index = value.integer;
        return MYLITE_OK;
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "ELT() index supports only signed integer, boolean, and NULL literals"
            );
            return MYLITE_ERROR;
        }
        has_sign = true;
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "ELT() index supports only signed integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_index = 1;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_index = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "ELT() index supports only signed integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(database, "ELT() index literals must fit the signed 64-bit range");
        return MYLITE_ERROR;
    }

    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        *out_index = INT64_MIN;
    } else if (is_negative) {
        *out_index = -(int64_t)magnitude;
    } else {
        *out_index = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int evaluate_elt_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct field_scalar_argument){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "ELT() supports only string, integer, boolean, and NULL value arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return evaluate_elt_scalar_integer_value(database, expression, out_argument);
    }
    if (concat_operator_scalar_argument_is_integer_arithmetic_expression(expression)) {
        struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
        int rc = evaluate_integer_default_expression(database, expression, &value);

        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "ELT() supports only string, integer, boolean, and NULL value arguments"
            );
            return MYLITE_ERROR;
        }
        if (value.is_null) {
            out_argument->is_null = true;
        } else {
            out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
            out_argument->integer = value.integer;
        }
        return MYLITE_OK;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "ELT() supports only string, integer, boolean, and NULL value arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING:
        return evaluate_elt_scalar_string_value(database, expression, out_argument);
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return evaluate_elt_scalar_integer_value(database, expression, out_argument);
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 1;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_argument->is_null = true;
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "ELT() supports only string, integer, boolean, and NULL value arguments"
    );
    return MYLITE_ERROR;
}

static int evaluate_elt_scalar_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "ELT() supports only string literals",
        "ELT() string literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    out_argument->text = text;
    out_argument->text_length = text_length;
    return MYLITE_OK;
}

static int evaluate_elt_scalar_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    uint64_t magnitude = 0U;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "ELT() supports only string, integer, boolean, and NULL value arguments"
            );
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "ELT() supports only string, integer, boolean, and NULL value arguments"
        );
        return MYLITE_ERROR;
    }

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        out_argument->integer = INT64_MIN;
    } else if (is_negative) {
        out_argument->integer = -(int64_t)magnitude;
    } else {
        out_argument->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int format_elt_scalar_result(
    struct mylite_db *database,
    const struct field_scalar_argument *value,
    struct session_scalar_cell *out_cell
) {
    if (value == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->is_null) {
        return MYLITE_OK;
    }
    if (value->domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) {
        out_cell->owned_text = (char *)malloc(value->text_length + 1U);
        if (out_cell->owned_text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(out_cell->owned_text, value->text, value->text_length);
        out_cell->owned_text[value->text_length] = '\0';
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = value->text_length;
        out_cell->has_value_size = true;
        return MYLITE_OK;
    }
    if (value->domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER) {
        return MYLITE_OK;
    }

    int written = snprintf(
        out_cell->integer_text,
        sizeof(out_cell->integer_text),
        "%" PRId64,
        value->integer
    );

    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format ELT() value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int field_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    struct field_scalar_argument *values = NULL;
    struct field_scalar_argument_list field_arguments = {0};
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FIELD_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "FIELD() supports only FIELD(search, value, ...)");
        return MYLITE_ERROR;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        set_native_function_parameter_count_error(database, "FIELD");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (struct field_scalar_argument *)calloc(argument_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = evaluate_field_scalar_arguments(database, arguments, values, argument_count, &domain);
    if (rc == MYLITE_OK) {
        field_arguments.values = values;
        field_arguments.count = argument_count;
        rc = format_field_scalar_result_position(
            database,
            field_scalar_result_position(&field_arguments, domain),
            out_cell
        );
    }

    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        field_scalar_argument_deinit(&values[argument_index]);
    }
    free(values);
    return rc;
}

int mylite_execution_scalar_greatest_least_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return greatest_least_function_value(database, expression, out_cell);
}

static int greatest_least_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const char *function_name = greatest_least_function_name(expression);
    const struct mylite_sql_ast_node *arguments = NULL;
    struct field_scalar_argument *values = NULL;
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    size_t argument_count = 0U;
    size_t numeric_scale = 0U;
    size_t result_index = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (function_name == NULL || expression == NULL ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_unsupported_error(database, "GREATEST() and LEAST() support only flat argument lists");
        return MYLITE_ERROR;
    }

    arguments = child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (struct field_scalar_argument *)calloc(argument_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = evaluate_greatest_least_scalar_arguments(
        database,
        arguments,
        values,
        argument_count,
        &domain,
        &numeric_scale
    );
    if (rc == MYLITE_OK) {
        for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
            if (values[argument_index].is_null) {
                goto done;
            }
        }
        result_index = greatest_least_scalar_result_index(
            values,
            (struct greatest_least_scalar_selection){
                .argument_count = argument_count,
                .domain = domain,
                .is_greatest = expression->kind == MYLITE_SQL_AST_GREATEST_FUNCTION,
            }
        );
        rc = format_greatest_least_scalar_result(
            database,
            &values[result_index],
            numeric_scale,
            out_cell
        );
    }

done:
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        field_scalar_argument_deinit(&values[argument_index]);
    }
    free(values);
    return rc;
}

static int interval_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *thresholds = NULL;
    struct field_scalar_argument search = {0};
    int64_t result = 0;
    size_t threshold_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_INTERVAL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_unsupported_error(database, "INTERVAL() supports only INTERVAL(search, value, ...)");
        return MYLITE_ERROR;
    }

    thresholds = child_at(expression, 1U);
    if (thresholds == NULL || thresholds->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    threshold_count = mylite_sql_ast_node_child_count(thresholds);
    if (threshold_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = evaluate_interval_scalar_search_argument(database, child_at(expression, 0U), &search);
    if (rc != MYLITE_OK) {
        goto done;
    }
    if (rc == MYLITE_OK && search.is_null) {
        rc = format_interval_scalar_result(database, -1, out_cell);
        goto done;
    }

    rc = evaluate_interval_scalar_thresholds(
        database,
        thresholds,
        &search,
        threshold_count,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = format_interval_scalar_result(database, result, out_cell);
    }

done:
    field_scalar_argument_deinit(&search);
    return rc;
}

static int evaluate_interval_scalar_thresholds(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *thresholds,
    const struct field_scalar_argument *search,
    size_t threshold_count,
    int64_t *out_result
) {
    const struct mylite_sql_ast_node *threshold = NULL;
    int64_t previous_threshold = 0;

    if (thresholds == NULL || search == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    threshold = child_at(thresholds, 0U);
    for (size_t threshold_index = 0U; threshold_index < threshold_count; ++threshold_index) {
        struct field_scalar_argument value = {0};
        int rc = MYLITE_OK;

        if (threshold == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = evaluate_interval_scalar_threshold_argument(database, threshold, &value);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (value.is_null) {
            set_unsupported_error(database, "INTERVAL() threshold arguments cannot be NULL");
            return MYLITE_ERROR;
        }
        if (threshold_index != 0U && value.integer < previous_threshold) {
            set_unsupported_error(
                database,
                "INTERVAL() threshold arguments must be sorted ascending"
            );
            return MYLITE_ERROR;
        }
        if (search->integer >= value.integer) {
            *out_result = (int64_t)threshold_index + 1;
        }
        previous_threshold = value.integer;
        threshold = threshold->next_sibling;
    }
    if (threshold != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int evaluate_interval_scalar_search_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct field_scalar_argument){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer, boolean, and NULL search arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return evaluate_interval_scalar_integer_argument(
            database,
            expression,
            out_argument,
            "INTERVAL() supports only signed integer search arguments"
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer, boolean, and NULL search arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return evaluate_interval_scalar_integer_argument(
            database,
            expression,
            out_argument,
            "INTERVAL() supports only signed integer search arguments"
        );
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 1;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_argument->is_null = true;
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "INTERVAL() supports only integer, boolean, and NULL search arguments"
    );
    return MYLITE_ERROR;
}

static int evaluate_interval_scalar_threshold_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct field_scalar_argument){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer and boolean threshold arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return evaluate_interval_scalar_integer_argument(
            database,
            expression,
            out_argument,
            "INTERVAL() supports only signed integer threshold arguments"
        );
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "INTERVAL() supports only integer and boolean threshold arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return evaluate_interval_scalar_integer_argument(
            database,
            expression,
            out_argument,
            "INTERVAL() supports only signed integer threshold arguments"
        );
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 1;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_argument->is_null = true;
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "INTERVAL() supports only integer and boolean threshold arguments"
    );
    return MYLITE_ERROR;
}

static int evaluate_interval_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument,
    const char *unsupported_message
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    uint64_t magnitude = 0U;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "INTERVAL() integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        out_argument->integer = INT64_MIN;
    } else if (is_negative) {
        out_argument->integer = -(int64_t)magnitude;
    } else {
        out_argument->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int format_interval_scalar_result(
    struct mylite_db *database,
    int64_t value,
    struct session_scalar_cell *out_cell
) {
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, value);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format INTERVAL() value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static const char *greatest_least_function_name(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return NULL;
    }
    if (expression->kind == MYLITE_SQL_AST_GREATEST_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR) {
        return "GREATEST";
    }
    if (expression->kind == MYLITE_SQL_AST_LEAST_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR) {
        return "LEAST";
    }
    return NULL;
}

static int evaluate_greatest_least_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct field_scalar_argument *values,
    size_t argument_count,
    enum planned_row_scalar_field_domain *out_domain,
    size_t *out_numeric_scale
) {
    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    size_t numeric_scale = 0U;

    if (values == NULL || out_domain == NULL || out_numeric_scale == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        int rc = MYLITE_OK;

        if (argument == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = evaluate_greatest_least_scalar_argument(database, argument, &values[argument_index]);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (values[argument_index].has_numeric_real &&
            values[argument_index].numeric_scale > numeric_scale) {
            numeric_scale = values[argument_index].numeric_scale;
        }
        rc = merge_greatest_least_domain(database, values[argument_index].domain, &domain);
        if (rc != MYLITE_OK) {
            return rc;
        }
        argument = argument->next_sibling;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    *out_domain = domain;
    *out_numeric_scale = numeric_scale;
    return MYLITE_OK;
}

static int evaluate_greatest_least_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct field_scalar_argument){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(expression, 0U));

        if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_DECIMAL ||
             mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_FLOAT)) {
            return evaluate_greatest_least_scalar_decimal_argument(
                database,
                expression,
                out_argument
            );
        }
        return evaluate_greatest_least_scalar_integer_argument(database, expression, out_argument);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING:
        return evaluate_greatest_least_scalar_string_argument(database, expression, out_argument);
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return evaluate_greatest_least_scalar_integer_argument(database, expression, out_argument);
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return evaluate_greatest_least_scalar_decimal_argument(database, expression, out_argument);
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 1;
        out_argument->numeric_real = 1.0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 0;
        out_argument->numeric_real = 0.0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_argument->is_null = true;
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
    );
    return MYLITE_ERROR;
}

static int evaluate_greatest_least_scalar_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "GREATEST() and LEAST() support only string literals",
        "GREATEST() and LEAST() string literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!field_ascii_text_is_supported(text, text_length)) {
        free(text);
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() string literals support only ASCII values"
        );
        return MYLITE_ERROR;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    out_argument->text = text;
    out_argument->text_length = text_length;
    return MYLITE_OK;
}

static int evaluate_greatest_least_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "GREATEST() and LEAST() support only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only signed integer literals"
        );
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
    if (rc != MYLITE_OK || (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        out_argument->integer = INT64_MIN;
    } else if (is_negative) {
        out_argument->integer = -(int64_t)magnitude;
    } else {
        out_argument->integer = (int64_t)magnitude;
    }
    out_argument->numeric_real = (double)out_argument->integer;
    return MYLITE_OK;
}

static int evaluate_greatest_least_scalar_decimal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    const struct mylite_sql_source_span *span = NULL;
    char *text = NULL;
    char *parse_end = NULL;
    size_t prefix_size = 0U;
    size_t numeric_scale = 0U;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        operator_kind = mylite_sql_ast_node_operator(expression);
        if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "GREATEST() and LEAST() support only signed decimal literals"
            );
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
        prefix_size = 1U;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_FLOAT)) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    span = &literal->span;
    if (span->text == NULL || span->length == 0U || span->length > SIZE_MAX - prefix_size - 1U) {
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < span->length; ++index) {
        if (span->text[index] == 'e' || span->text[index] == 'E') {
            break;
        }
        if (span->text[index] == '.') {
            for (size_t scale_index = index + 1U; scale_index < span->length; ++scale_index) {
                if (span->text[scale_index] == 'e' || span->text[scale_index] == 'E') {
                    break;
                }
                ++numeric_scale;
            }
            break;
        }
    }

    text = (char *)malloc(prefix_size + span->length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (prefix_size != 0U) {
        text[0] = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? '-' : '+';
    }
    memcpy(&text[prefix_size], span->text, span->length);
    text[prefix_size + span->length] = '\0';

    errno = 0;
    out_argument->numeric_real = strtod(text, &parse_end);
    if (errno != 0 || parse_end == text || parse_end == NULL || *parse_end != '\0') {
        free(text);
        set_unsupported_error(
            database,
            "GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    out_argument->has_numeric_real = true;
    out_argument->text = text;
    out_argument->text_length = prefix_size + span->length;
    out_argument->numeric_scale = numeric_scale;
    return MYLITE_OK;
}

static int merge_greatest_least_domain(
    struct mylite_db *database,
    enum planned_row_scalar_field_domain incoming,
    enum planned_row_scalar_field_domain *inout_domain
) {
    if (inout_domain == NULL) {
        return MYLITE_MISUSE;
    }
    if (incoming == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
        return MYLITE_OK;
    }
    if (*inout_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
        *inout_domain = incoming;
        return MYLITE_OK;
    }
    if (*inout_domain == incoming) {
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "GREATEST() and LEAST() do not support mixed string and numeric arguments"
    );
    return MYLITE_ERROR;
}

static size_t greatest_least_scalar_result_index(
    const struct field_scalar_argument *values,
    struct greatest_least_scalar_selection selection
) {
    size_t result_index = 0U;

    if (values == NULL || selection.argument_count == 0U) {
        return 0U;
    }
    for (size_t argument_index = 1U; argument_index < selection.argument_count; ++argument_index) {
        int comparison = compare_greatest_least_scalar_arguments(
            &values[argument_index],
            &values[result_index],
            selection.domain
        );

        if ((selection.is_greatest && comparison >= 0) ||
            (!selection.is_greatest && comparison < 0)) {
            result_index = argument_index;
        }
    }
    return result_index;
}

static int compare_greatest_least_scalar_arguments(
    const struct field_scalar_argument *left,
    const struct field_scalar_argument *right,
    enum planned_row_scalar_field_domain domain
) {
    if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) {
        return field_ascii_text_compare_case_insensitive(
            left->text,
            left->text_length,
            right->text,
            right->text_length
        );
    }
    if (left->has_numeric_real || right->has_numeric_real) {
        double left_value = left->has_numeric_real ? left->numeric_real : (double)left->integer;
        double right_value = right->has_numeric_real ? right->numeric_real : (double)right->integer;

        if (left_value < right_value) {
            return -1;
        }
        if (left_value > right_value) {
            return 1;
        }
        return 0;
    }
    if (left->integer < right->integer) {
        return -1;
    }
    if (left->integer > right->integer) {
        return 1;
    }
    return 0;
}

static int field_ascii_text_compare_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
) {
    size_t common_length = left_length < right_length ? left_length : right_length;

    if (left == NULL || right == NULL) {
        return 0;
    }
    for (size_t index = 0U; index < common_length; ++index) {
        unsigned char left_byte = (unsigned char)ascii_lower((unsigned char)left[index]);
        unsigned char right_byte = (unsigned char)ascii_lower((unsigned char)right[index]);

        if (left_byte < right_byte) {
            return -1;
        }
        if (left_byte > right_byte) {
            return 1;
        }
    }
    if (left_length < right_length) {
        return -1;
    }
    if (left_length > right_length) {
        return 1;
    }
    return 0;
}

static int format_greatest_least_scalar_result(
    struct mylite_db *database,
    const struct field_scalar_argument *value,
    size_t numeric_scale,
    struct session_scalar_cell *out_cell
) {
    if (value == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) {
        out_cell->owned_text = (char *)malloc(value->text_length + 1U);
        if (out_cell->owned_text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(out_cell->owned_text, value->text, value->text_length);
        out_cell->owned_text[value->text_length] = '\0';
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = value->text_length;
        out_cell->has_value_size = true;
        return MYLITE_OK;
    }

    if (value->has_numeric_real && value->text != NULL) {
        out_cell->owned_text = (char *)malloc(value->text_length + 1U);
        if (out_cell->owned_text == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(out_cell->owned_text, value->text, value->text_length);
        out_cell->owned_text[value->text_length] = '\0';
        out_cell->value = out_cell->owned_text;
        out_cell->value_size = value->text_length;
        out_cell->has_value_size = true;
        return MYLITE_OK;
    }

    if (numeric_scale != 0U) {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64 ".",
            value->integer
        );

        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text) ||
            numeric_scale >= sizeof(out_cell->integer_text) - (size_t)written) {
            set_runtime_error(database, "failed to format GREATEST() or LEAST() value");
            return MYLITE_ERROR;
        }
        memset(&out_cell->integer_text[written], '0', numeric_scale);
        out_cell->integer_text[(size_t)written + numeric_scale] = '\0';
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }

    int written = snprintf(
        out_cell->integer_text,
        sizeof(out_cell->integer_text),
        "%" PRId64,
        value->integer
    );

    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format GREATEST() or LEAST() value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static int evaluate_field_scalar_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    struct field_scalar_argument *values,
    size_t argument_count,
    enum planned_row_scalar_field_domain *out_domain
) {
    const struct mylite_sql_ast_node *argument = child_at(arguments, 0U);
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;

    if (values == NULL || out_domain == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        int rc = MYLITE_OK;

        if (argument == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = evaluate_field_scalar_argument(database, argument, &values[argument_index]);
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = merge_field_domain(database, values[argument_index].domain, &domain);
        if (rc != MYLITE_OK) {
            return rc;
        }
        argument = argument->next_sibling;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    *out_domain = domain;
    return MYLITE_OK;
}

static int evaluate_field_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct field_scalar_argument){0};
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "FIELD() supports only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return evaluate_field_scalar_integer_argument(database, expression, out_argument);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "FIELD() supports only string, integer, boolean, and NULL arguments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_STRING:
        return evaluate_field_scalar_string_argument(database, expression, out_argument);
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return evaluate_field_scalar_integer_argument(database, expression, out_argument);
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 1;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        out_argument->integer = 0;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_argument->is_null = true;
        return MYLITE_OK;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "FIELD() supports only string, integer, boolean, and NULL arguments"
    );
    return MYLITE_ERROR;
}

static int evaluate_field_scalar_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct field_scalar_argument *out_argument
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = decode_sql_string_literal(
        database,
        literal,
        "FIELD() supports only string literals",
        "FIELD() string literals do not support NUL bytes",
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!field_ascii_text_is_supported(text, text_length)) {
        free(text);
        set_unsupported_error(database, "FIELD() string literals support only ASCII values");
        return MYLITE_ERROR;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    out_argument->text = text;
    out_argument->text_length = text_length;
    return MYLITE_OK;
}

static int evaluate_field_scalar_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct field_scalar_argument *out_argument
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "FIELD() supports only signed integer literals");
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(expression, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "FIELD() supports only signed integer literals");
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
    if (rc != MYLITE_OK || (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT integer literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    }

    out_argument->domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        out_argument->integer = INT64_MIN;
    } else if (is_negative) {
        out_argument->integer = -(int64_t)magnitude;
    } else {
        out_argument->integer = (int64_t)magnitude;
    }
    return MYLITE_OK;
}

static int merge_field_domain(
    struct mylite_db *database,
    enum planned_row_scalar_field_domain incoming,
    enum planned_row_scalar_field_domain *inout_domain
) {
    if (inout_domain == NULL) {
        return MYLITE_MISUSE;
    }
    if (incoming == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
        return MYLITE_OK;
    }
    if (*inout_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
        *inout_domain = incoming;
        return MYLITE_OK;
    }
    if (*inout_domain == incoming) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, "FIELD() does not support mixed string and numeric arguments");
    return MYLITE_ERROR;
}

static bool field_ascii_text_is_supported(const char *text, size_t text_length) {
    if (text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        if ((unsigned char)text[index] > ascii_text_max_byte) {
            return false;
        }
    }
    return true;
}

static bool field_ascii_text_equals_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
) {
    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    for (size_t index = 0U; index < left_length; ++index) {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static size_t field_scalar_result_position(
    const struct field_scalar_argument_list *arguments,
    enum planned_row_scalar_field_domain domain
) {
    if (arguments == NULL || arguments->values == NULL || arguments->count < 2U ||
        arguments->values[0].is_null) {
        return 0U;
    }
    for (size_t argument_index = 1U; argument_index < arguments->count; ++argument_index) {
        if (field_scalar_arguments_match(
                &arguments->values[0],
                &arguments->values[argument_index],
                domain
            )) {
            return argument_index;
        }
    }
    return 0U;
}

static bool field_scalar_arguments_match(
    const struct field_scalar_argument *left,
    const struct field_scalar_argument *right,
    enum planned_row_scalar_field_domain domain
) {
    if (left == NULL || right == NULL || right->is_null) {
        return false;
    }
    if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING) {
        return field_ascii_text_equals_case_insensitive(
            left->text,
            left->text_length,
            right->text,
            right->text_length
        );
    }
    if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER) {
        return left->integer == right->integer;
    }
    return false;
}

static int format_field_scalar_result_position(
    struct mylite_db *database,
    size_t result_position,
    struct session_scalar_cell *out_cell
) {
    int written =
        snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%zu", result_position);

    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        set_runtime_error(database, "failed to format FIELD() value");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static void field_scalar_argument_deinit(struct field_scalar_argument *argument) {
    if (argument == NULL) {
        return;
    }
    free(argument->text);
    *argument = (struct field_scalar_argument){0};
}
