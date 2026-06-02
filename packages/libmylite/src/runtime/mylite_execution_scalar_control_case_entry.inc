static int if_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int ifnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int coalesce_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int nullif_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int isnull_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return if_scalar_value(database, expression, out_cell);
}

static int case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = validate_case_expression(database, expression);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (expression->kind == MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        return searched_case_expression_value(database, expression, out_cell);
    }
    return simple_case_expression_value(database, expression, out_cell);
}

static int searched_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *when_list = child_at(expression, 0U);
    const struct mylite_sql_ast_node *else_clause = child_at(expression, 1U);
    const struct mylite_sql_ast_node *when_clause = child_at(when_list, 0U);
    size_t staged_warning_count = 0U;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && when_clause != NULL) {
        struct scalar_arithmetic_value condition = {.is_null = false, .integer = 0};

        rc = evaluate_case_arithmetic_expression(database, child_at(when_clause, 0U), &condition);
        if (rc == MYLITE_OK) {
            rc = accumulate_staged_division_by_zero_warnings(
                database,
                condition.division_by_zero_warning_count,
                &staged_warning_count
            );
        }
        if (rc == MYLITE_OK && scalar_arithmetic_truth_value(&condition)) {
            struct session_scalar_cell result = {0};

            rc = session_scalar_value_without_case(database, child_at(when_clause, 1U), &result);
            if (rc == MYLITE_OK) {
                rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
            }
            return rc;
        }
        when_clause = when_clause->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (else_clause != NULL) {
        struct session_scalar_cell result = {0};

        rc = session_scalar_value_without_case(database, child_at(else_clause, 0U), &result);
        if (rc == MYLITE_OK) {
            rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
        }
        return rc;
    }

    *out_cell = (struct session_scalar_cell){0};
    out_cell->staged_division_by_zero_warning_count = staged_warning_count;
    return MYLITE_OK;
}

static int simple_case_expression_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *case_value_node = child_at(expression, 0U);
    const struct mylite_sql_ast_node *when_list = child_at(expression, 1U);
    const struct mylite_sql_ast_node *else_clause = child_at(expression, 2U);
    const struct mylite_sql_ast_node *when_clause = child_at(when_list, 0U);
    struct scalar_arithmetic_value case_value = {.is_null = false, .integer = 0};
    size_t staged_warning_count = 0U;
    int rc = evaluate_case_arithmetic_expression(database, case_value_node, &case_value);

    if (rc == MYLITE_OK) {
        rc = accumulate_staged_division_by_zero_warnings(
            database,
            case_value.division_by_zero_warning_count,
            &staged_warning_count
        );
    }
    while (rc == MYLITE_OK && when_clause != NULL) {
        struct scalar_arithmetic_value compare_value = {.is_null = false, .integer = 0};

        rc = evaluate_case_arithmetic_expression(
            database,
            child_at(when_clause, 0U),
            &compare_value
        );
        if (rc == MYLITE_OK) {
            rc = accumulate_staged_division_by_zero_warnings(
                database,
                compare_value.division_by_zero_warning_count,
                &staged_warning_count
            );
        }
        if (rc == MYLITE_OK && case_arithmetic_values_are_equal(&case_value, &compare_value)) {
            struct session_scalar_cell result = {0};

            rc = session_scalar_value_without_case(database, child_at(when_clause, 1U), &result);
            if (rc == MYLITE_OK) {
                rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
            }
            return rc;
        }
        when_clause = when_clause->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (else_clause != NULL) {
        struct session_scalar_cell result = {0};

        rc = session_scalar_value_without_case(database, child_at(else_clause, 0U), &result);
        if (rc == MYLITE_OK) {
            rc = copy_case_result_cell(database, &result, staged_warning_count, out_cell);
        }
        return rc;
    }

    *out_cell = (struct session_scalar_cell){0};
    out_cell->staged_division_by_zero_warning_count = staged_warning_count;
    return MYLITE_OK;
}

static int evaluate_case_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
) {
    struct session_scalar_cell cell = {0};
    int rc = session_scalar_value_without_case(database, expression, &cell);

    if (rc == MYLITE_OK) {
        rc = parse_scalar_arithmetic_operand(database, &cell, out_value);
    }
    return rc;
}

static int copy_case_result_cell(
    struct mylite_db *database,
    const struct session_scalar_cell *selected_cell,
    size_t previous_warning_count,
    struct session_scalar_cell *out_cell
) {
    size_t total_warning_count = previous_warning_count;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = accumulate_staged_division_by_zero_warnings(
        database,
        selected_cell == NULL ? 0U : selected_cell->staged_division_by_zero_warning_count,
        &total_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    copy_session_scalar_cell(out_cell, selected_cell);
    out_cell->staged_division_by_zero_warning_count = total_warning_count;
    return MYLITE_OK;
}

static bool case_arithmetic_values_are_equal(
    const struct scalar_arithmetic_value *left,
    const struct scalar_arithmetic_value *right
) {
    if (left == NULL || right == NULL || left->is_null || right->is_null) {
        return false;
    }
    return left->integer == right->integer;
}
