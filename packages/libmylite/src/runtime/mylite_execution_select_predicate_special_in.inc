static int plan_tableless_row_scalar_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (source_context != NULL || table_columns != NULL || table_column_count != 0U) {
        set_unsupported_error(
            database,
            "WHERE scalar subquery IS NULL is supported only for no-source SELECT filters"
        );
        return MYLITE_ERROR;
    }

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        child_at(predicate_node, 0U),
        false,
        NULL,
        NULL,
        NULL,
        0U,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_row_scalar_expression_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_predicate_left_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static bool sql_auto_is_null_lookup_applies(
    const struct mylite_db *database,
    const struct select_predicate_plan_options *options,
    const struct planned_select_predicate_node *node
) {
    const char *value = NULL;

    if (database == NULL || options == NULL || node == NULL ||
        !options->allow_sql_auto_is_null_lookup ||
        node->operator_kind != MYLITE_SQL_AST_OPERATOR_IS_NULL ||
        !column_descriptor_is_auto_increment(&node->column) ||
        database->session.last_insert_id > (uint64_t)INT64_MAX) {
        return false;
    }

    value = session_system_variable_override_value(
        database,
        MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL
    );
    return value != NULL && strcmp(value, "1") == 0;
}

static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };

    if (child_at(predicate_node, 0U) != NULL &&
        (child_at(predicate_node, 0U)->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
         predicate_node_is_quantified_subquery(child_at(predicate_node, 0U)))) {
        return plan_comparison_result_is_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_find_in_set_expression(child_at(predicate_node, 0U))) {
        return plan_find_in_set_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_regexp_like_expression(child_at(predicate_node, 0U))) {
        return plan_regexp_like_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_valid_expression(child_at(predicate_node, 0U))) {
        return plan_json_valid_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_contains_expression(child_at(predicate_node, 0U))) {
        return plan_json_contains_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_string_length_expression(child_at(predicate_node, 0U))) {
        return plan_string_length_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_substring_expression(child_at(predicate_node, 0U))) {
        return plan_substring_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_temporal_extract_expression(child_at(predicate_node, 0U))) {
        return plan_temporal_extract_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return plan_scalar_literal_is_predicate(
            database,
            predicate_node,
            predicate,
            out_node_index
        );
    }
    if (child_at(predicate_node, 0U) != NULL &&
        child_at(predicate_node, 0U)->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        return plan_tableless_row_scalar_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_supported_row_scalar_expression(child_at(predicate_node, 0U))) {
        return plan_row_scalar_expression_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (sql_auto_is_null_lookup_applies(database, options, &node)) {
        node.kind = PLANNED_SELECT_PREDICATE_COMPARISON;
        node.operator_kind = MYLITE_SQL_AST_OPERATOR_EQUAL;
        node.value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = (int64_t)database->session.last_insert_id,
        };
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_BOOLEAN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    if (child_at(predicate_node, 0U) != NULL &&
        (child_at(predicate_node, 0U)->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
         predicate_node_is_quantified_subquery(child_at(predicate_node, 0U)))) {
        return plan_comparison_result_is_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return plan_scalar_literal_is_predicate(
            database,
            predicate_node,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_supported_row_scalar_expression(child_at(predicate_node, 0U))) {
        return plan_row_scalar_expression_is_boolean_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (rc == MYLITE_OK) {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &node.column,
            "WHERE supports only integer boolean predicate columns",
            &range
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_comparison_result_is_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);
    size_t node_index = SIZE_MAX;
    int rc = MYLITE_OK;

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_IS_NULL &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN) {
        set_unsupported_error(database, "comparison result IS predicate is not supported");
        return MYLITE_ERROR;
    }

    if (predicate_node_is_quantified_subquery(child_at(predicate_node, 0U))) {
        rc = plan_quantified_subquery_predicate(
            database,
            child_at(predicate_node, 0U),
            source_context,
            table_columns,
            table_column_count,
            options,
            predicate,
            &node_index
        );
    } else {
        rc = plan_comparison_predicate(
            database,
            child_at(predicate_node, 0U),
            source_context,
            table_columns,
            table_column_count,
            options,
            predicate,
            &node_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (node_index >= predicate->node_count) {
        return MYLITE_ERROR;
    }

    predicate->nodes[node_index].comparison_result_is_operator_kind = operator_kind;
    *out_node_index = node_index;
    return MYLITE_OK;
}

static int plan_row_scalar_expression_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_BOOLEAN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_expression(
        database,
        child_at(predicate_node, 0U),
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        node.row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_between_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_BETWEEN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support BETWEEN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support BETWEEN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_date(&node.column)) {
            rc = convert_predicate_date_between_values(
                database,
                child_at(predicate_node, 1U),
                child_at(predicate_node, 2U),
                &node.column,
                &node.value,
                &node.upper_value,
                &node.compare_date_as_datetime
            );
        } else {
            rc = plan_between_predicate_bound(
                database,
                child_at(predicate_node, 1U),
                source_context,
                table_columns,
                table_column_count,
                false,
                &node
            );
            if (rc == MYLITE_OK) {
                rc = plan_between_predicate_bound(
                    database,
                    child_at(predicate_node, 2U),
                    source_context,
                    table_columns,
                    table_column_count,
                    true,
                    &node
                );
            }
        }
    }
    if (rc != MYLITE_OK) {
        planned_select_predicate_node_deinit_runtime_values(&node);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_between_predicate_bound(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *bound_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool upper_bound,
    struct planned_select_predicate_node *node
) {
    if (predicate_node_is_supported_row_scalar_predicate_value_expression(bound_node)) {
        if (upper_bound) {
            return plan_predicate_row_scalar_upper_value_expression(
                database,
                bound_node,
                source_context,
                table_columns,
                table_column_count,
                node
            );
        }
        return plan_predicate_row_scalar_value_expression(
            database,
            bound_node,
            source_context,
            table_columns,
            table_column_count,
            node
        );
    }

    return convert_predicate_value(
        database,
        bound_node,
        &node->column,
        upper_bound ? &node->upper_value : &node->value
    );
}

static int plan_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    const struct mylite_sql_ast_node *right = NULL;
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;
    size_t outer_source_count = 1U;
    bool row_constructor_handled = false;

    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "WHERE supports only descriptor column IN predicates");
        return MYLITE_ERROR;
    }
    rc = plan_row_constructor_in_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        predicate,
        out_node_index,
        &row_constructor_handled
    );
    if (row_constructor_handled || rc != MYLITE_OK) {
        return rc;
    }
    if (predicate_node_is_literal_left_predicate_value(child_at(predicate_node, 0U)) ||
        predicate_node_is_supported_row_scalar_expression(child_at(predicate_node, 0U))) {
        return plan_row_scalar_expression_in_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (child_at(predicate_node, 0U) != NULL &&
        child_at(predicate_node, 0U)->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        return plan_scalar_subquery_in_literal_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            predicate,
            out_node_index
        );
    }
    right = child_at(predicate_node, 1U);
    if (right == NULL || right->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return plan_in_literal_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (options == NULL || !options->allow_in_subquery) {
            set_unsupported_error(database, "IN subqueries are supported only in SELECT WHERE");
            return MYLITE_ERROR;
        }
        if (source_context == NULL) {
            set_unsupported_error(database, "IN subqueries support only an outer table source");
            return MYLITE_ERROR;
        }
        if (select_source_context_is_joined(source_context)) {
            outer_source_count = source_context->source_count;
        }
        rc = plan_in_subquery_predicate(
            database,
            right,
            source_context,
            table_columns,
            table_column_count,
            outer_source_count,
            &node
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_in_subquery_deinit(node.in_subquery);
        free(node.in_subquery);
        return rc;
    }
    predicate->qualify_column_references = true;

    return rc;
}

static bool predicate_node_is_quantified_subquery(
    const struct mylite_sql_ast_node *predicate_node
) {
    return predicate_node != NULL &&
           (predicate_node->kind == MYLITE_SQL_AST_ANY_SUBQUERY_PREDICATE ||
            predicate_node->kind == MYLITE_SQL_AST_SOME_SUBQUERY_PREDICATE ||
            predicate_node->kind == MYLITE_SQL_AST_ALL_SUBQUERY_PREDICATE);
}

static int plan_quantified_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_QUANTIFIED_SUBQUERY,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .quantified_subquery_kind = predicate_node == NULL ? 0 : predicate_node->kind,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    size_t outer_source_count = 1U;
    int rc = MYLITE_OK;

    if (!predicate_node_is_quantified_subquery(predicate_node)) {
        set_unsupported_error(
            database,
            "quantified subqueries support descriptor column left operands"
        );
        return MYLITE_ERROR;
    }
    if (child_at(predicate_node, 0U) == NULL ||
        (child_at(predicate_node, 0U)->kind != MYLITE_SQL_AST_IDENTIFIER &&
         child_at(predicate_node, 0U)->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(
            database,
            "quantified subqueries support descriptor column left operands"
        );
        return MYLITE_ERROR;
    }
    if (options == NULL || !options->allow_quantified_subquery) {
        set_unsupported_error(
            database,
            "quantified subqueries are supported only in SELECT WHERE"
        );
        return MYLITE_ERROR;
    }
    if (source_context == NULL) {
        set_unsupported_error(
            database,
            "quantified subqueries support only an outer table source"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK && column_descriptor_is_enum(&node.column)) {
        set_unsupported_error(
            database,
            "WHERE enum predicates do not yet support quantified subqueries"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && column_descriptor_is_set(&node.column)) {
        set_unsupported_error(
            database,
            "WHERE set predicates do not yet support quantified subqueries"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && select_source_context_is_joined(source_context)) {
        outer_source_count = source_context->source_count;
    }
    if (rc == MYLITE_OK) {
        rc = plan_in_subquery_predicate(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            outer_source_count,
            &node
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_in_subquery_deinit(node.in_subquery);
        free(node.in_subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_row_constructor_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *left_node =
        unwrap_parenthesized_predicate(child_at(predicate_node, 0U));
    const struct mylite_sql_ast_node *value_list = child_at(predicate_node, 1U);
    size_t element_count = 0U;
    size_t value_count = 0U;
    size_t root_index = SIZE_MAX;
    int rc = MYLITE_OK;

    if (out_handled == NULL) {
        set_unsupported_error(database, "WHERE row constructor IN predicate planning failed");
        return MYLITE_ERROR;
    }
    *out_handled = predicate_node_is_row_constructor(left_node);
    if (!*out_handled) {
        return MYLITE_OK;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(
            database,
            "WHERE row constructor IN predicates support only literal row lists"
        );
        return MYLITE_ERROR;
    }

    element_count = mylite_sql_ast_node_child_count(left_node);
    value_count = mylite_sql_ast_node_child_count(value_list);
    if (value_count == 0U) {
        set_unsupported_error(database, "WHERE row constructor IN predicates require a value list");
        return MYLITE_ERROR;
    }
    for (size_t value_index = 0U; value_index < value_count && rc == MYLITE_OK; ++value_index) {
        const struct mylite_sql_ast_node *right_node =
            unwrap_parenthesized_predicate(child_at(value_list, value_index));
        size_t value_root_index = SIZE_MAX;

        if (!predicate_node_is_row_constructor(right_node)) {
            set_unsupported_error(
                database,
                "WHERE row constructor IN predicates require row constructor list values"
            );
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_child_count(right_node) != element_count) {
            set_row_constructor_operand_count_error(database, element_count);
            return MYLITE_ERROR;
        }

        rc = plan_row_constructor_equality_predicate(
            database,
            left_node,
            right_node,
            element_count,
            source_context,
            MYLITE_SQL_AST_OPERATOR_EQUAL,
            table_columns,
            table_column_count,
            predicate,
            &value_root_index
        );
        if (rc == MYLITE_OK && root_index == SIZE_MAX) {
            root_index = value_root_index;
        } else if (rc == MYLITE_OK) {
            rc = append_row_constructor_or_node(
                database,
                predicate,
                root_index,
                value_root_index,
                &root_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        predicate->qualify_column_references =
            predicate->qualify_column_references || select_source_context_is_joined(source_context);
        *out_node_index = root_index;
    }
    return rc;
}

static int plan_row_scalar_expression_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    const struct mylite_sql_ast_node *right = child_at(predicate_node, 1U);
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_IN,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    struct mylite_catalog_column_descriptor context_column = {0};
    bool has_context_column = false;
    int rc = MYLITE_OK;

    if (right == NULL || right->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "row-scalar IN predicates support only literal lists");
        return MYLITE_ERROR;
    }

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (predicate_node_is_literal_left_predicate_value(child_at(predicate_node, 0U))) {
        size_t value_count = mylite_sql_ast_node_child_count(right);

        for (size_t value_index = 0U;
             rc == MYLITE_OK && !has_context_column && value_index < value_count;
             ++value_index) {
            rc = resolve_literal_left_predicate_context_column(
                database,
                child_at(right, value_index),
                source_context,
                table_columns,
                table_column_count,
                &context_column,
                &has_context_column
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_literal_left_predicate_row_scalar_expression(
            database,
            child_at(predicate_node, 0U),
            has_context_column ? &context_column : NULL,
            source_context,
            table_columns,
            table_column_count,
            node.row_scalar_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_in_value_list(
            database,
            right,
            has_context_column ? &context_column : NULL,
            source_context,
            table_columns,
            table_column_count,
            &node
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_array_deinit(node.values, node.value_count);
        free(node.values);
        if (node.row_scalar_value_expressions != NULL) {
            for (size_t value_index = 0U; value_index < node.value_count; ++value_index) {
                planned_row_scalar_expression_deinit(&node.row_scalar_value_expressions[value_index]
                );
            }
        }
        free(node.row_scalar_value_expressions);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_row_scalar_predicate_left_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_predicate(expression);

    if (predicate_node_is_scalar_literal_expression(node)) {
        if (node != NULL && node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
            return plan_row_scalar_integer_value(database, node, out_expression);
        }
        return plan_row_scalar_literal_value(database, node, out_expression);
    }
    return plan_row_scalar_expression(
        database,
        expression,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        out_expression
    );
}

static int plan_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "WHERE supports only descriptor column IN predicates");
        return MYLITE_ERROR;
    }
    if (child_at(predicate_node, 1U) != NULL &&
        child_at(predicate_node, 1U)->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "IN subqueries are supported only in SELECT WHERE");
        return MYLITE_ERROR;
    }

    rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_set(&node.column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support IN");
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_date(&node.column)) {
            rc = convert_predicate_date_in_value_list(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.values,
                &node.value_count,
                &node.compare_date_as_datetime
            );
        } else if (predicate_value_list_contains_row_scalar_predicate_value(
                       child_at(predicate_node, 1U)
                   )) {
            rc = plan_descriptor_in_value_list(
                database,
                child_at(predicate_node, 1U),
                source_context,
                table_columns,
                table_column_count,
                &node
            );
        } else {
            rc = convert_predicate_in_value_list(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.values,
                &node.value_count
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_array_deinit(node.values, node.value_count);
        free(node.values);
        if (node.row_scalar_value_expressions != NULL) {
            for (size_t value_index = 0U; value_index < node.value_count; ++value_index) {
                planned_row_scalar_expression_deinit(&node.row_scalar_value_expressions[value_index]
                );
            }
        }
        free(node.row_scalar_value_expressions);
        return rc;
    }

    return rc;
}

static bool predicate_value_list_contains_row_scalar_predicate_value(
    const struct mylite_sql_ast_node *value_list
) {
    size_t value_count = 0U;

    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        return false;
    }

    value_count = mylite_sql_ast_node_child_count(value_list);
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        if (predicate_node_is_supported_row_scalar_predicate_value_expression(
                child_at(value_list, value_index)
            )) {
            return true;
        }
    }
    return false;
}

static int plan_descriptor_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    size_t value_count = 0U;
    int rc = MYLITE_OK;

    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    value_count = mylite_sql_ast_node_child_count(value_list);
    if (value_count == 0U) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    if (value_count > SIZE_MAX / sizeof(*node->values) ||
        value_count > SIZE_MAX / sizeof(*node->row_scalar_value_expressions)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    node->values = calloc(value_count, sizeof(*node->values));
    node->row_scalar_value_expressions =
        calloc(value_count, sizeof(*node->row_scalar_value_expressions));
    if (node->values == NULL || node->row_scalar_value_expressions == NULL) {
        set_nomem_error(database);
        planned_value_array_deinit(node->values, value_count);
        free(node->values);
        free(node->row_scalar_value_expressions);
        node->values = NULL;
        node->row_scalar_value_expressions = NULL;
        return MYLITE_NOMEM;
    }
    node->value_count = value_count;

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        rc = plan_descriptor_in_value(
            database,
            child_at(value_list, value_index),
            source_context,
            table_columns,
            table_column_count,
            node,
            value_index
        );
    }
    return rc;
}

static int plan_descriptor_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node,
    size_t value_index
) {
    if (predicate_node_is_supported_row_scalar_predicate_value_expression(value_node)) {
        return plan_row_scalar_expression(
            database,
            value_node,
            true,
            source_context,
            NULL,
            table_columns,
            table_column_count,
            &node->row_scalar_value_expressions[value_index]
        );
    }

    return convert_predicate_in_value(
        database,
        value_node,
        &node->column,
        &node->values[value_index]
    );
}

static int plan_scalar_subquery_in_literal_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    const struct mylite_sql_ast_node *left = child_at(predicate_node, 0U);
    const struct mylite_sql_ast_node *right = child_at(predicate_node, 1U);
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_SCALAR_SUBQUERY_IN,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;
    size_t outer_source_count = 1U;

    if (options == NULL || !options->allow_scalar_subquery_in_literal) {
        set_unsupported_error(
            database,
            "scalar subquery IN predicates are supported only in SELECT WHERE"
        );
        return MYLITE_ERROR;
    }
    if (source_context == NULL) {
        set_unsupported_error(
            database,
            "scalar subquery IN predicates support only an outer table source"
        );
        return MYLITE_ERROR;
    }
    if (right == NULL || right->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(
            database,
            "scalar subquery IN predicates support only literal value lists"
        );
        return MYLITE_ERROR;
    }
    if (left == NULL || mylite_sql_ast_node_child_count(left) != 1U) {
        set_unsupported_error(database, "scalar subquery supports only parenthesized SELECT");
        return MYLITE_ERROR;
    }
    if (select_source_context_is_joined(source_context)) {
        outer_source_count = source_context->source_count;
    }

    node.in_subquery = calloc(1U, sizeof(*node.in_subquery));
    if (node.in_subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_in_subquery(
        database,
        child_at(left, 0U),
        source_context,
        table_columns,
        table_column_count,
        outer_source_count,
        node.in_subquery
    );
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_enum(&node.in_subquery->column)) {
            set_unsupported_error(database, "WHERE enum predicates do not yet support IN");
            rc = MYLITE_ERROR;
        } else if (column_descriptor_is_set(&node.in_subquery->column)) {
            set_unsupported_error(database, "WHERE set predicates do not yet support IN");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        if (column_descriptor_is_date(&node.in_subquery->column)) {
            rc = convert_predicate_date_in_value_list(
                database,
                right,
                &node.in_subquery->column,
                &node.values,
                &node.value_count,
                &node.compare_date_as_datetime
            );
        } else {
            rc = convert_predicate_in_value_list(
                database,
                right,
                &node.in_subquery->column,
                &node.values,
                &node.value_count
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_array_deinit(node.values, node.value_count);
        free(node.values);
        planned_in_subquery_deinit(node.in_subquery);
        free(node.in_subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_in_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    size_t outer_source_count,
    struct planned_select_predicate_node *node
) {
    struct planned_in_subquery *subquery = NULL;
    int rc = MYLITE_OK;

    subquery = calloc(1U, sizeof(*subquery));
    if (subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_in_subquery(
        database,
        select_statement,
        outer_source_context,
        outer_columns,
        outer_column_count,
        outer_source_count,
        subquery
    );
    if (rc == MYLITE_OK && !in_subquery_columns_are_compatible(&node->column, &subquery->column)) {
        set_unsupported_error(
            database,
            "IN subqueries support same-family integer or string descriptor columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        planned_in_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    node->in_subquery = subquery;
    return MYLITE_OK;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): outer columns and sources are distinct count
// domains.
static int plan_in_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    size_t outer_source_count,
    struct planned_in_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_in_subquery){0};
    out_subquery->inner_source_index = outer_source_count;
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "IN subqueries support only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT &&
        mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT) {
        set_unsupported_error(database, "IN subqueries support only optional DISTINCT modifiers");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "IN subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    for (const struct mylite_sql_ast_node *clause = optional_clause; clause != NULL;
         clause = clause->next_sibling) {
        if (clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            set_in_subquery_limit_error(database);
            return MYLITE_ERROR;
        }
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(database, "IN subqueries support only WHERE");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    rc = plan_in_subquery_table_source(database, from_clause, out_subquery);
    if (rc == MYLITE_OK) {
        rc = plan_in_subquery_select_column(
            database,
            select_list,
            out_subquery,
            &out_subquery->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_in_subquery_inner_predicate(
            database,
            where_clause,
            outer_source_context,
            outer_columns,
            outer_column_count,
            out_subquery
        );
    }

    return rc;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int plan_in_subquery_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_in_subquery *out_subquery
) {
    struct planned_select joined_plan = {0};
    struct joined_select_temp_nodes temp_nodes = {0};
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_JOIN) {
            set_unsupported_error(database, "IN subqueries support one descriptor table source");
            return MYLITE_ERROR;
        }

        rc = prepare_joined_select_plan(database, from_clause, &joined_plan, &temp_nodes);
        if (rc == MYLITE_OK) {
            rc = reject_duplicate_select_source_reference(
                database,
                joined_plan.sources,
                joined_plan.source_count
            );
        }
        if (rc == MYLITE_OK) {
            for (size_t join_index = 0U; join_index < joined_plan.join_count; ++join_index) {
                enum mylite_sql_ast_join_kind join_kind = joined_plan.join_kinds[join_index];

                if (join_kind != MYLITE_SQL_AST_JOIN_KIND_INNER &&
                    join_kind != MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER) {
                    set_unsupported_error(
                        database,
                        "IN joined subqueries support only INNER JOIN or LEFT JOIN sources"
                    );
                    rc = MYLITE_ERROR;
                    break;
                }
            }
        }
        if (rc == MYLITE_OK) {
            rc = plan_joined_select_conditions(
                database,
                temp_nodes.join_condition_nodes,
                temp_nodes.join_condition_node_count,
                &joined_plan
            );
        }
        if (rc == MYLITE_OK) {
            for (size_t join_index = 0U; join_index < joined_plan.join_count; ++join_index) {
                const struct planned_select_join_condition *condition =
                    &joined_plan.join_conditions[join_index];

                if (!condition->has_condition) {
                    set_unsupported_error(
                        database,
                        "IN joined subqueries require descriptor equality join conditions"
                    );
                    rc = MYLITE_ERROR;
                    break;
                }
                if (condition->right_value_is_row_scalar_expression) {
                    set_unsupported_error(
                        database,
                        "IN joined subqueries support only descriptor equality join conditions"
                    );
                    rc = MYLITE_ERROR;
                    break;
                }
            }
        }
        if (rc == MYLITE_OK) {
            out_subquery->sources = joined_plan.sources;
            out_subquery->source_count = joined_plan.source_count;
            out_subquery->join_kinds = joined_plan.join_kinds;
            out_subquery->join_conditions = joined_plan.join_conditions;
            out_subquery->join_count = joined_plan.join_count;

            joined_plan.sources = NULL;
            joined_plan.source_count = 0U;
            joined_plan.join_kinds = NULL;
            joined_plan.join_conditions = NULL;
            joined_plan.join_count = 0U;
        }
        joined_select_temp_nodes_deinit(&temp_nodes);
        if (rc != MYLITE_OK) {
            planned_select_deinit(&joined_plan);
        }
        return rc;
    }

    return plan_joined_select_source(database, from_clause, &out_subquery->source);
}

static int plan_in_subquery_select_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct planned_in_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    struct select_source_context source_context = {0};
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_scalar_subquery_column_count_error(database);
        return MYLITE_ERROR;
    }
    item = child_at(select_list, 0U);
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    expression = child_at(item, 0U);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
        expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        set_unsupported_error(
            database,
            "IN subqueries support one explicit descriptor select column"
        );
        return MYLITE_ERROR;
    }

    if (subquery->source_count > 0U) {
        rc = init_join_select_source_context(
            subquery->sources,
            subquery->source_count,
            &source_context
        );
    } else {
        source_context = (struct select_source_context){
            .source = &subquery->source.source,
            .has_alias = subquery->source.has_alias,
        };
        memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        expression,
        &source_context,
        COLUMN_REFERENCE_FIELD,
        "IN subqueries support one explicit descriptor select column",
        subquery->source_count > 0U ? NULL : subquery->source.columns,
        subquery->source_count > 0U ? 0U : subquery->source.column_count,
        out_column,
        &source_index
    );
    subquery->column_source_index = subquery->inner_source_index + source_index;
    return rc;
}

static bool in_subquery_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *outer_column,
    const struct mylite_catalog_column_descriptor *inner_column
) {
    return join_condition_columns_are_compatible(outer_column, inner_column);
}

static int plan_in_subquery_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *subquery
) {
    struct select_source_context inner_source_context = {0};
    const struct mylite_catalog_column_descriptor *inner_columns = NULL;
    size_t inner_column_count = 0U;
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .allow_sql_auto_is_null_lookup = true,
        .exists_inner_source_index = subquery->inner_source_index,
        .exists_inner_source_count = subquery->source_count == 0U ? 1U : subquery->source_count,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };
    int rc = MYLITE_OK;

    if (subquery->source_count > 0U) {
        rc = init_join_select_source_context(
            subquery->sources,
            subquery->source_count,
            &inner_source_context
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    } else {
        inner_source_context = (struct select_source_context){
            .source = &subquery->source.source,
            .has_alias = subquery->source.has_alias,
        };
        memcpy(
            inner_source_context.alias,
            subquery->source.alias,
            sizeof(inner_source_context.alias)
        );
        inner_columns = subquery->source.columns;
        inner_column_count = subquery->source.column_count;
    }

    subquery->predicate = (struct planned_select_predicate){0};
    subquery->predicate.qualify_column_references =
        select_source_context_is_joined(&inner_source_context);
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node_without_exists(
        database,
        child_at(where_clause, 0U),
        &inner_source_context,
        inner_columns,
        inner_column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }
    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(
            &subquery->predicate,
            subquery->inner_source_index
        );
    }
    return rc;
}

static int plan_row_scalar_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *context_column,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    size_t value_count = 0U;
    int rc = MYLITE_OK;

    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    value_count = mylite_sql_ast_node_child_count(value_list);
    if (value_count == 0U) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    if (value_count > SIZE_MAX / sizeof(*node->values) ||
        value_count > SIZE_MAX / sizeof(*node->row_scalar_value_expressions)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    node->values = calloc(value_count, sizeof(*node->values));
    node->row_scalar_value_expressions =
        calloc(value_count, sizeof(*node->row_scalar_value_expressions));
    if (node->values == NULL || node->row_scalar_value_expressions == NULL) {
        set_nomem_error(database);
        planned_value_array_deinit(node->values, value_count);
        free(node->values);
        free(node->row_scalar_value_expressions);
        node->values = NULL;
        node->row_scalar_value_expressions = NULL;
        return MYLITE_NOMEM;
    }
    node->value_count = value_count;

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        rc = plan_row_scalar_in_value(
            database,
            child_at(value_list, value_index),
            context_column,
            source_context,
            table_columns,
            table_column_count,
            node,
            value_index
        );
    }
    return rc;
}

static int plan_row_scalar_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *context_column,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node,
    size_t value_index
) {
    if (predicate_node_is_supported_row_scalar_predicate_value_expression(value_node)) {
        return plan_row_scalar_expression(
            database,
            value_node,
            true,
            source_context,
            NULL,
            table_columns,
            table_column_count,
            &node->row_scalar_value_expressions[value_index]
        );
    }

    if (context_column != NULL && predicate_node_is_string_literal_predicate_value(value_node)) {
        return convert_literal_left_predicate_value_for_context(
            database,
            value_node,
            context_column,
            &node->values[value_index]
        );
    }

    return plan_where_row_scalar_predicate_literal_value(
        database,
        node->row_scalar_expression,
        value_node,
        &node->values[value_index]
    );
}

static int convert_predicate_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
) {
    struct planned_value *values = NULL;
    size_t value_count = mylite_sql_ast_node_child_count(value_list);
    int rc = MYLITE_OK;

    *out_values = NULL;
    *out_value_count = 0U;
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST ||
        value_count == 0U) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_year(column)) {
        return convert_predicate_year_in_value_list(
            database,
            value_list,
            column,
            out_values,
            out_value_count
        );
    }
    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        rc = convert_predicate_in_value(
            database,
            child_at(value_list, value_index),
            column,
            &values[value_index]
        );
    }
    if (rc != MYLITE_OK) {
        for (size_t cleanup_index = 0U; cleanup_index < value_count; ++cleanup_index) {
            planned_value_deinit(&values[cleanup_index]);
        }
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static int convert_predicate_in_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .integer = 0};
        return MYLITE_OK;
    }

    return convert_predicate_value(database, value_node, column, out_value);
}

static int convert_predicate_year_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count
) {
    struct planned_value *values = NULL;
    size_t value_count = mylite_sql_ast_node_child_count(value_list);
    bool uses_string_conversion =
        year_in_value_list_uses_string_conversion(value_list, value_count);
    int rc = MYLITE_OK;

    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, value_index);

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            values[value_index] = (struct planned_value){.is_null = true, .integer = 0};
        } else if (uses_string_conversion) {
            rc = convert_predicate_year_literal(database, value_node, column, &values[value_index]);
        } else {
            rc = convert_year_direct_value(database, value_node, &values[value_index]);
        }
    }
    if (rc != MYLITE_OK) {
        for (size_t cleanup_index = 0U; cleanup_index < value_count; ++cleanup_index) {
            planned_value_deinit(&values[cleanup_index]);
        }
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static bool year_in_value_list_uses_string_conversion(
    const struct mylite_sql_ast_node *value_list,
    size_t value_count
) {
    if (value_count == 1U) {
        return true;
    }
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node =
            unwrap_parenthesized_expression(child_at(value_list, value_index));

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            continue;
        }
        if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
            return false;
        }
    }
    return true;
}
