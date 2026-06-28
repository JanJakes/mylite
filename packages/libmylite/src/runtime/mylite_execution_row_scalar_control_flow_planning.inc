static bool is_row_scalar_control_flow_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
        return true;
    default:
        return false;
    }
}

static int plan_row_scalar_control_flow_expression(
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
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
        return plan_row_scalar_if_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return plan_row_scalar_ifnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return plan_row_scalar_coalesce_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return plan_row_scalar_nullif_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return plan_row_scalar_isnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
        return plan_row_scalar_searched_case_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
        return plan_row_scalar_simple_case_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT control-flow functions support only IF(), IFNULL(), COALESCE(), "
        "NULLIF(), ISNULL(), and CASE"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_nested_control_flow_expression(
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
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_IF_FUNCTION:
        return plan_row_scalar_nested_if_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
        return plan_row_scalar_nested_ifnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
        return plan_row_scalar_nested_coalesce_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
        return plan_row_scalar_nested_nullif_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_ISNULL_FUNCTION:
        return plan_row_scalar_nested_isnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION:
        return plan_row_scalar_nested_searched_case_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION:
        return plan_row_scalar_nested_simple_case_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    default:
        break;
    }

    set_unsupported_error(
        database,
        "nested row-scalar SELECT control-flow functions support only IF(), IFNULL(), "
        "COALESCE(), NULLIF(), ISNULL(), and CASE"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_searched_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    size_t when_count = 0U;
    size_t argument_count = 0U;
    size_t argument_index = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        set_unsupported_error(database, "row-scalar searched CASE expression is unsupported");
        return MYLITE_ERROR;
    }
    when_list = child_at(expression, 0U);
    else_clause = child_at(expression, 1U);
    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        set_unsupported_error(database, "row-scalar searched CASE requires WHEN clauses");
        return MYLITE_ERROR;
    }

    when_count = mylite_sql_ast_node_child_count(when_list);
    argument_count = when_count * 2U;
    if (else_clause != NULL) {
        ++argument_count;
    }
    if (when_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE;
    out_expression->argument_count = argument_count;
    out_expression->case_has_else = else_clause != NULL;

    when_clause = child_at(when_list, 0U);
    while (rc == MYLITE_OK && when_clause != NULL) {
        if (when_clause->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
            argument_index + 1U >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        rc = plan_row_scalar_control_flow_condition_argument(
            database,
            child_at(when_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_control_flow_value_argument(
                database,
                child_at(when_clause, 1U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[argument_index + 1U]
            );
        }
        argument_index += 2U;
        when_clause = when_clause->next_sibling;
    }
    if (rc == MYLITE_OK && else_clause != NULL) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(else_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_count - 1U]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain = row_scalar_searched_case_result_domain(out_expression);
    }
    return rc;
}

static int plan_row_scalar_simple_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *case_value = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    size_t when_count = 0U;
    size_t argument_count = 0U;
    size_t argument_index = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION) {
        set_unsupported_error(database, "row-scalar simple CASE expression is unsupported");
        return MYLITE_ERROR;
    }
    case_value = child_at(expression, 0U);
    when_list = child_at(expression, 1U);
    else_clause = child_at(expression, 2U);
    if (case_value == NULL || when_list == NULL ||
        when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        set_unsupported_error(database, "row-scalar simple CASE requires a value and WHEN clauses");
        return MYLITE_ERROR;
    }

    when_count = mylite_sql_ast_node_child_count(when_list);
    argument_count = when_count * 2U;
    if (else_clause != NULL) {
        ++argument_count;
    }
    if (when_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE;
    out_expression->argument_count = argument_count;
    out_expression->case_has_else = else_clause != NULL;

    when_clause = child_at(when_list, 0U);
    while (rc == MYLITE_OK && when_clause != NULL) {
        if (when_clause->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
            argument_index + 1U >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        rc = plan_row_scalar_simple_case_equality_condition(
            database,
            case_value,
            child_at(when_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_control_flow_value_argument(
                database,
                child_at(when_clause, 1U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[argument_index + 1U]
            );
        }
        argument_index += 2U;
        when_clause = when_clause->next_sibling;
    }
    if (rc == MYLITE_OK && else_clause != NULL) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(else_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_count - 1U]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain = row_scalar_searched_case_result_domain(out_expression);
    }
    return rc;
}

static int plan_row_scalar_simple_case_equality_condition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *case_value,
    const struct mylite_sql_ast_node *when_value,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE;
    out_expression->argument_count = 2U;
    out_expression->arithmetic_operator = MYLITE_SQL_AST_OPERATOR_EQUAL;
    out_expression->like_uses_escape = false;

    rc = plan_row_scalar_control_flow_comparison_operand(
        database,
        case_value,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_comparison_operand(
            database,
            when_value,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_nested_searched_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    size_t when_count = 0U;
    size_t argument_count = 0U;
    size_t argument_index = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION) {
        set_unsupported_error(
            database,
            "nested row-scalar searched CASE expression is unsupported"
        );
        return MYLITE_ERROR;
    }
    when_list = child_at(expression, 0U);
    else_clause = child_at(expression, 1U);
    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        set_unsupported_error(database, "nested row-scalar searched CASE requires WHEN clauses");
        return MYLITE_ERROR;
    }

    when_count = mylite_sql_ast_node_child_count(when_list);
    argument_count = when_count * 2U;
    if (else_clause != NULL) {
        ++argument_count;
    }
    if (when_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE;
    out_expression->argument_count = argument_count;
    out_expression->case_has_else = else_clause != NULL;

    when_clause = child_at(when_list, 0U);
    while (rc == MYLITE_OK && when_clause != NULL) {
        if (when_clause->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
            argument_index + 1U >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        rc = plan_row_scalar_control_flow_leaf_condition_argument(
            database,
            child_at(when_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_control_flow_leaf_value_argument(
                database,
                child_at(when_clause, 1U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[argument_index + 1U]
            );
        }
        argument_index += 2U;
        when_clause = when_clause->next_sibling;
    }
    if (rc == MYLITE_OK && else_clause != NULL) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(else_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_count - 1U]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain = row_scalar_searched_case_result_domain(out_expression);
    }
    return rc;
}

static int plan_row_scalar_nested_simple_case_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *case_value = NULL;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_clause = NULL;
    const struct mylite_sql_ast_node *when_clause = NULL;
    size_t when_count = 0U;
    size_t argument_count = 0U;
    size_t argument_index = 0U;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION) {
        set_unsupported_error(database, "nested row-scalar simple CASE expression is unsupported");
        return MYLITE_ERROR;
    }
    case_value = child_at(expression, 0U);
    when_list = child_at(expression, 1U);
    else_clause = child_at(expression, 2U);
    if (case_value == NULL || when_list == NULL ||
        when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        set_unsupported_error(
            database,
            "nested row-scalar simple CASE requires a value and WHEN clauses"
        );
        return MYLITE_ERROR;
    }

    when_count = mylite_sql_ast_node_child_count(when_list);
    argument_count = when_count * 2U;
    if (else_clause != NULL) {
        ++argument_count;
    }
    if (when_count == 0U || argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE;
    out_expression->argument_count = argument_count;
    out_expression->case_has_else = else_clause != NULL;

    when_clause = child_at(when_list, 0U);
    while (rc == MYLITE_OK && when_clause != NULL) {
        if (when_clause->kind != MYLITE_SQL_AST_CASE_WHEN_CLAUSE ||
            argument_index + 1U >= argument_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        rc = plan_row_scalar_simple_case_equality_condition(
            database,
            case_value,
            child_at(when_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_control_flow_leaf_value_argument(
                database,
                child_at(when_clause, 1U),
                has_source,
                source_context,
                table_columns,
                table_column_count,
                &out_expression->arguments[argument_index + 1U]
            );
        }
        argument_index += 2U;
        when_clause = when_clause->next_sibling;
    }
    if (rc == MYLITE_OK && else_clause != NULL) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(else_clause, 0U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_count - 1U]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain = row_scalar_searched_case_result_domain(out_expression);
    }
    return rc;
}

static int plan_row_scalar_like_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_LIKE) {
        set_unsupported_error(database, "row-scalar CASE WHEN supports only LIKE predicates");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE;
    out_expression->argument_count = 2U;
    out_expression->arithmetic_operator = MYLITE_SQL_AST_OPERATOR_LIKE;
    out_expression->like_uses_escape = (bool
    )!session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES);

    rc = plan_row_scalar_control_flow_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_sounds_like_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE) {
        set_unsupported_error(database, "row-scalar SELECT supports only SOUNDS LIKE predicates");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE;
    out_expression->argument_count = 2U;
    out_expression->arithmetic_operator = MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE;
    out_expression->like_uses_escape = false;

    rc = plan_row_scalar_soundex_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_soundex_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_leaf_like_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_LIKE) {
        set_unsupported_error(
            database,
            "nested row-scalar CASE WHEN supports only LIKE predicates"
        );
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE;
    out_expression->argument_count = 2U;
    out_expression->arithmetic_operator = MYLITE_SQL_AST_OPERATOR_LIKE;
    out_expression->like_uses_escape = (bool
    )!session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES);

    rc = plan_row_scalar_control_flow_leaf_value_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static bool row_scalar_control_flow_logical_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
);

static int plan_row_scalar_logical_condition_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (!row_scalar_control_flow_logical_operator_is_supported(operator_kind)) {
        set_unsupported_error(database, "row-scalar CASE WHEN supports only AND/OR logic");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION;
    out_expression->argument_count = 2U;
    out_expression->arithmetic_operator = operator_kind;
    out_expression->field_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;

    rc = plan_row_scalar_control_flow_condition_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_condition_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static int plan_row_scalar_leaf_logical_condition_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (!row_scalar_control_flow_logical_operator_is_supported(operator_kind)) {
        set_unsupported_error(database, "nested row-scalar CASE WHEN supports only AND/OR logic");
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION;
    out_expression->argument_count = 2U;
    out_expression->arithmetic_operator = operator_kind;
    out_expression->field_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;

    rc = plan_row_scalar_control_flow_leaf_condition_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_leaf_condition_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static bool row_scalar_control_flow_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
);

static int plan_row_scalar_integer_comparison_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    expression = unwrap_parenthesized_expression(expression);
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NONE) {
        set_unsupported_error(
            database,
            "row-scalar CASE WHEN supports only integer comparison predicates"
        );
        return MYLITE_ERROR;
    }

    if (!row_scalar_control_flow_comparison_operator_is_supported(operator_kind)) {
        set_unsupported_error(
            database,
            "row-scalar CASE WHEN integer comparisons support only equality and ordering "
            "operators"
        );
        return MYLITE_ERROR;
    }

    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE;
    out_expression->argument_count = 2U;
    out_expression->arithmetic_operator = operator_kind;
    out_expression->like_uses_escape = false;

    rc = plan_row_scalar_control_flow_comparison_operand(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_comparison_operand(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    return rc;
}

static enum mylite_sql_ast_operator row_scalar_control_flow_condition_operator(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 2U) {
        return MYLITE_SQL_AST_OPERATOR_NONE;
    }
    if (expression->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
        expression->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return mylite_sql_ast_node_operator(expression);
    }
    return MYLITE_SQL_AST_OPERATOR_NONE;
}

static bool row_scalar_control_flow_expression_is_bare_arithmetic(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_MOD_FUNCTION) {
        return true;
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    return integer_arithmetic_operator_is_supported(mylite_sql_ast_node_operator(expression));
}

static bool row_scalar_control_flow_expression_is_greatest_least(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    return expression->kind == MYLITE_SQL_AST_GREATEST_FUNCTION ||
           expression->kind == MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR ||
           expression->kind == MYLITE_SQL_AST_LEAST_FUNCTION ||
           expression->kind == MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR;
}

static int plan_row_scalar_control_flow_greatest_least_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool require_integer_domain,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = plan_row_scalar_greatest_least_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        out_expression
    );

    if (rc == MYLITE_OK && require_integer_domain &&
        out_expression->field_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE &&
        out_expression->field_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER) {
        planned_row_scalar_expression_deinit(out_expression);
        set_unsupported_error(
            database,
            "IF() row conditions support GREATEST() and LEAST() only in integer domain"
        );
        return MYLITE_ERROR;
    }
    return rc;
}

static bool row_scalar_control_flow_logical_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_LIKE_BINARY:
    case MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        break;
    }
    return false;
}

static bool row_scalar_control_flow_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_LIKE_BINARY:
    case MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        break;
    }
    return false;
}

static int plan_row_scalar_control_flow_comparison_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "row-scalar CASE WHEN integer comparisons support only integer, boolean, NULL, "
            "and descriptor integer operands"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_unsupported_error(
            database,
            "row-scalar CASE WHEN integer comparisons support only integer, boolean, NULL, "
            "and descriptor integer operands"
        );
        return MYLITE_ERROR;
    }
    if (generic_function_name_equals(expression, "GET_LOCK")) {
        return plan_row_scalar_get_lock_function(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    if (row_scalar_numeric_function_kind_is_supported(expression->kind)) {
        return plan_row_scalar_numeric_function_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
        return plan_row_scalar_control_flow_condition_column(
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
        "row-scalar CASE WHEN integer comparisons support only integer, boolean, NULL, "
        "descriptor integer, and numeric function operands"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_if_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "IF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IF;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_control_flow_condition_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(&out_expression->arguments[1], 2U);
    }
    return rc;
}

static int plan_row_scalar_ifnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IFNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IFNULL;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(out_expression->arguments, 2U);
    }
    return rc;
}

static int plan_row_scalar_coalesce_expression(
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

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COALESCE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        set_parse_error(database, NULL);
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
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COALESCE;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        row_scalar_control_flow_common_domain(out_expression->arguments, argument_count);
    return MYLITE_OK;
}

static int plan_row_scalar_nullif_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_row_scalar_field_domain left_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    enum planned_row_scalar_field_domain right_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_NULLIF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_NULLIF;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    left_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[0]);
    right_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[1]);
    if (left_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE &&
        right_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE && left_domain != right_domain) {
        set_unsupported_error(
            database,
            "NULLIF() row projection does not support mixed string and numeric arguments"
        );
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        left_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE ? right_domain : left_domain;
    return MYLITE_OK;
}

static int plan_row_scalar_isnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_ISNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_ISNULL;
    out_expression->argument_count = 1U;
    out_expression->field_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;

    rc = plan_row_scalar_control_flow_value_argument(
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

static int plan_row_scalar_nested_if_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        set_native_function_parameter_count_error(database, "IF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(3U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IF;
    out_expression->argument_count = 3U;

    rc = plan_row_scalar_control_flow_leaf_condition_argument(
        database,
        child_at(expression, 0U),
        has_source,
        source_context,
        table_columns,
        table_column_count,
        &out_expression->arguments[0]
    );
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, 1U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[1]
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, 2U),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[2]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(&out_expression->arguments[1], 2U);
    }
    return rc;
}

static int plan_row_scalar_nested_ifnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IFNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "IFNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_IFNULL;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc == MYLITE_OK) {
        out_expression->field_domain =
            row_scalar_control_flow_common_domain(out_expression->arguments, 2U);
    }
    return rc;
}

static int plan_row_scalar_nested_coalesce_expression(
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

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COALESCE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        set_parse_error(database, NULL);
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
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COALESCE;
    out_expression->argument_count = argument_count;

    argument = child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            argument,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
        argument = argument->next_sibling;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        row_scalar_control_flow_common_domain(out_expression->arguments, argument_count);
    return MYLITE_OK;
}

static int plan_row_scalar_nested_nullif_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum planned_row_scalar_field_domain left_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    enum planned_row_scalar_field_domain right_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_NULLIF_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        set_native_function_parameter_count_error(database, "NULLIF");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(2U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_NULLIF;
    out_expression->argument_count = 2U;

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = plan_row_scalar_control_flow_leaf_value_argument(
            database,
            child_at(expression, argument_index),
            has_source,
            source_context,
            table_columns,
            table_column_count,
            &out_expression->arguments[argument_index]
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    left_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[0]);
    right_domain = row_scalar_control_flow_argument_domain(&out_expression->arguments[1]);
    if (left_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE &&
        right_domain != PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE && left_domain != right_domain) {
        set_unsupported_error(
            database,
            "NULLIF() row projection does not support mixed string and numeric arguments"
        );
        return MYLITE_ERROR;
    }
    out_expression->field_domain =
        left_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE ? right_domain : left_domain;
    return MYLITE_OK;
}

static int plan_row_scalar_nested_isnull_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_ISNULL_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        set_native_function_parameter_count_error(database, "ISNULL");
        return MYLITE_ERROR;
    }
    out_expression->arguments =
        (struct planned_row_scalar_expression *)calloc(1U, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_ISNULL;
    out_expression->argument_count = 1U;
    out_expression->field_domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;

    rc = plan_row_scalar_control_flow_leaf_value_argument(
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

static int plan_row_scalar_control_flow_value_argument(
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
            "row-scalar SELECT control-flow functions support only descriptor columns, scalar "
            "literals, session scalar values, system variables, and nested control-flow functions"
        );
        return MYLITE_ERROR;
    }
    if (is_row_scalar_control_flow_expression(expression)) {
        return plan_row_scalar_nested_control_flow_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
        return plan_row_scalar_control_flow_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    if (has_source && row_scalar_control_flow_expression_is_bare_arithmetic(expression)) {
        return plan_row_scalar_integer_arithmetic_expression(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && row_scalar_control_flow_expression_is_greatest_least(expression)) {
        return plan_row_scalar_control_flow_greatest_least_argument(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            false,
            out_expression
        );
    }
    return plan_row_scalar_non_control_flow_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        false,
        out_expression
    );
}

static int plan_row_scalar_control_flow_condition_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "IF() row conditions support only integer, boolean, NULL, descriptor integer column, "
            "numeric function, row arithmetic, ISNULL(), LIKE, and comparison values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_unsupported_error(
            database,
            "IF() row conditions support only integer, boolean, NULL, descriptor integer column, "
            "numeric function, row arithmetic, ISNULL(), LIKE, and comparison values"
        );
        return MYLITE_ERROR;
    }
    if (generic_function_name_equals(expression, "GET_LOCK")) {
        return plan_row_scalar_get_lock_function(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    if (row_scalar_numeric_function_kind_is_supported(expression->kind)) {
        return plan_row_scalar_numeric_function_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && row_scalar_control_flow_expression_is_bare_arithmetic(expression)) {
        return plan_row_scalar_integer_arithmetic_expression(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && row_scalar_control_flow_expression_is_greatest_least(expression)) {
        return plan_row_scalar_control_flow_greatest_least_argument(
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
    if (expression->kind == MYLITE_SQL_AST_ISNULL_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR) {
        return plan_row_scalar_isnull_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return plan_row_scalar_like_predicate_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_control_flow_logical_operator_is_supported(operator_kind)) {
        return plan_row_scalar_logical_condition_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NONE) {
        return plan_row_scalar_integer_comparison_predicate_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
        return plan_row_scalar_control_flow_condition_column(
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
        "IF() row conditions support only integer, boolean, NULL, descriptor integer column, "
        "numeric function, row arithmetic, ISNULL(), LIKE, and comparison values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_control_flow_leaf_value_argument(
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
            "nested row-scalar SELECT control-flow functions support only descriptor columns "
            "and scalar literals"
        );
        return MYLITE_ERROR;
    }
    if (is_row_scalar_control_flow_expression(expression)) {
        set_unsupported_error(
            database,
            "nested row-scalar SELECT control-flow functions support only one nested "
            "control-flow layer"
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
        return plan_row_scalar_control_flow_column(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }

    if (has_source && row_scalar_control_flow_expression_is_bare_arithmetic(expression)) {
        return plan_row_scalar_integer_arithmetic_expression(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && row_scalar_control_flow_expression_is_greatest_least(expression)) {
        return plan_row_scalar_control_flow_greatest_least_argument(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            false,
            out_expression
        );
    }
    return plan_row_scalar_non_control_flow_expression(
        database,
        expression,
        has_source,
        source_context,
        table_columns,
        table_column_count,
        false,
        out_expression
    );
}

static int plan_row_scalar_control_flow_leaf_condition_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "nested IF() row conditions support only integer, boolean, NULL, and descriptor "
            "integer column, numeric function, row arithmetic, LIKE, and comparison values"
        );
        return MYLITE_ERROR;
    }
    if (is_row_scalar_control_flow_expression(expression)) {
        set_unsupported_error(
            database,
            "nested row-scalar SELECT control-flow functions support only one nested "
            "control-flow layer"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return plan_row_scalar_literal_value(database, expression, out_expression);
        }
        set_unsupported_error(
            database,
            "nested IF() row conditions support only integer, boolean, NULL, and descriptor "
            "integer column, numeric function, row arithmetic, LIKE, and comparison values"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return plan_row_scalar_integer_value(database, expression, out_expression);
    }
    if (row_scalar_numeric_function_kind_is_supported(expression->kind)) {
        return plan_row_scalar_numeric_function_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && row_scalar_control_flow_expression_is_bare_arithmetic(expression)) {
        return plan_row_scalar_integer_arithmetic_expression(
            database,
            expression,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (has_source && row_scalar_control_flow_expression_is_greatest_least(expression)) {
        return plan_row_scalar_control_flow_greatest_least_argument(
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
    operator_kind = row_scalar_control_flow_condition_operator(expression);
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        return plan_row_scalar_leaf_like_predicate_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (row_scalar_control_flow_logical_operator_is_supported(operator_kind)) {
        return plan_row_scalar_leaf_logical_condition_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            out_expression
        );
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NONE) {
        return plan_row_scalar_integer_comparison_predicate_expression(
            database,
            expression,
            has_source,
            source_context,
            table_columns,
            table_column_count,
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
        return plan_row_scalar_control_flow_condition_column(
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
        "nested IF() row conditions support only integer, boolean, NULL, descriptor integer "
        "column, numeric function, row arithmetic, LIKE, and comparison values"
    );
    return MYLITE_ERROR;
}

static int plan_row_scalar_control_flow_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "row-scalar SELECT control-flow functions support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!control_flow_value_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
    return MYLITE_OK;
}

static int plan_row_scalar_control_flow_condition_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "IF() row conditions support only descriptor columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!control_flow_condition_column_descriptor_is_supported(database, &column)) {
        return MYLITE_ERROR;
    }

    out_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_COLUMN;
    out_expression->column = column;
    out_expression->column_has_source_index = select_source_context_is_joined(source_context);
    out_expression->column_source_index = source_index;
    return MYLITE_OK;
}

static bool control_flow_value_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_string_family(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return true;
    }
    if (column_descriptor_is_decimal(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT control-flow functions do not support DECIMAL columns"
        );
        return false;
    }
    if (column_descriptor_is_binary_string_family(column) || column_descriptor_is_bit(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT control-flow functions do not support binary string or BIT columns"
        );
        return false;
    }
    if (column_descriptor_is_approximate(column)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT control-flow functions do not support approximate numeric columns"
        );
        return false;
    }

    set_unsupported_error(
        database,
        "row-scalar SELECT control-flow functions support only integer, nonbinary string, YEAR, "
        "and temporal columns"
    );
    return false;
}

static bool control_flow_condition_column_descriptor_is_supported(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return true;
    }
    set_unsupported_error(database, "IF() row conditions support only integer descriptor columns");
    return false;
}

static enum planned_row_scalar_field_domain row_scalar_control_flow_argument_domain(
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    }
    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        if (expression->value.is_null) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
        }
        if (expression->value.is_text) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
        }
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return row_scalar_control_flow_column_domain(&expression->column);
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE:
        return expression->field_domain;
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_BITWISE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK:
    case PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS:
    case PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK:
    case PLANNED_ROW_SCALAR_EXPRESSION_SLEEP:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE:
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    case PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE:
    case PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML:
    case PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY:
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    case PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION:
        if (mylite_spatial_function_returns_integer(expression->spatial_function_kind) ||
            mylite_spatial_function_returns_double(expression->spatial_function_kind)) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        }
        if (mylite_spatial_function_returns_text(expression->spatial_function_kind)) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA:
        if (expression->numeric_extra_kind == PLANNED_ROW_SCALAR_NUMERIC_EXTRA_FORMAT) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
        }
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    case PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION:
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        if (expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_SIGNED ||
            expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
        }
        if (expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_CHAR ||
            expression->conversion_kind == PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLLATE:
        if (expression->arguments != NULL && expression->argument_count == 1U) {
            return row_scalar_control_flow_argument_domain(&expression->arguments[0]);
        }
        break;
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_ELT:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS:
    case PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON:
    case PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET:
    case PLANNED_ROW_SCALAR_EXPRESSION_DIGEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
        break;
    }
    return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
}

static enum planned_row_scalar_field_domain row_scalar_control_flow_column_domain(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER;
    }
    if (column_descriptor_is_string_family(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING;
    }
    return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
}

static enum planned_row_scalar_field_domain row_scalar_control_flow_common_domain(
    const struct planned_row_scalar_expression *arguments,
    size_t argument_count
) {
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;

    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        enum planned_row_scalar_field_domain argument_domain =
            row_scalar_control_flow_argument_domain(&arguments[argument_index]);

        if (argument_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            continue;
        }
        if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            domain = argument_domain;
        } else if (domain != argument_domain) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
        }
    }

    return domain;
}

static enum planned_row_scalar_field_domain row_scalar_searched_case_result_domain(
    const struct planned_row_scalar_expression *expression
) {
    enum planned_row_scalar_field_domain domain = PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    size_t result_argument_limit = 0U;

    if (expression == NULL || expression->arguments == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE ||
        expression->argument_count < 2U) {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    }
    if ((expression->argument_count % 2U) == 0U) {
        result_argument_limit = expression->argument_count;
    } else if (expression->case_has_else) {
        result_argument_limit = expression->argument_count - 1U;
    } else {
        return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
    }
    for (size_t argument_index = 1U; argument_index < result_argument_limit; argument_index += 2U) {
        enum planned_row_scalar_field_domain argument_domain =
            row_scalar_control_flow_argument_domain(&expression->arguments[argument_index]);

        if (argument_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            continue;
        }
        if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            domain = argument_domain;
        } else if (domain != argument_domain) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
        }
    }
    if (expression->case_has_else) {
        enum planned_row_scalar_field_domain argument_domain =
            row_scalar_control_flow_argument_domain(
                &expression->arguments[expression->argument_count - 1U]
            );

        if (argument_domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            return domain;
        }
        if (domain == PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE) {
            return argument_domain;
        }
        if (domain != argument_domain) {
            return PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE;
        }
    }
    return domain;
}
