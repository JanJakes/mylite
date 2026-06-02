static int plan_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
) {
    const struct select_predicate_plan_options options = {
        .allow_exists = false,
    };

    return plan_select_predicate_with_options(
        database,
        where_clause,
        source_context,
        table_columns,
        table_column_count,
        &options,
        out_predicate
    );
}

static int plan_joined_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    struct planned_select_predicate *out_predicate
) {
    const struct select_predicate_plan_options options = {
        .allow_exists = false,
        .allow_same_scope_column_reference_rhs = true,
        .allow_date_format_numeric_predicate = true,
    };

    return plan_select_predicate_with_options(
        database,
        where_clause,
        source_context,
        NULL,
        0U,
        &options,
        out_predicate
    );
}

static int plan_select_predicate_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    int rc = MYLITE_OK;

    *out_predicate = (struct planned_select_predicate){0};
    out_predicate->qualify_column_references = select_source_context_is_joined(source_context);
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node(
        database,
        child_at(where_clause, 0U),
        source_context,
        table_columns,
        table_column_count,
        options,
        out_predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit(out_predicate);
    }

    return rc;
}

static void planned_select_predicate_deinit(struct planned_select_predicate *predicate) {
    if (predicate == NULL) {
        return;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        planned_value_deinit(&predicate->nodes[node_index].value);
        planned_value_deinit(&predicate->nodes[node_index].upper_value);
        for (size_t value_index = 0U; value_index < predicate->nodes[node_index].value_count;
             ++value_index) {
            planned_value_deinit(&predicate->nodes[node_index].values[value_index]);
        }
        free(predicate->nodes[node_index].values);
        if (predicate->nodes[node_index].row_scalar_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_expression);
        planned_exists_subquery_deinit(predicate->nodes[node_index].exists_subquery);
        free(predicate->nodes[node_index].exists_subquery);
        planned_in_subquery_deinit(predicate->nodes[node_index].in_subquery);
        free(predicate->nodes[node_index].in_subquery);
    }
    free(predicate->nodes);
    *predicate = (struct planned_select_predicate){0};
}

static void planned_select_predicate_deinit_without_exists(
    struct planned_select_predicate *predicate
) {
    if (predicate == NULL) {
        return;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        planned_value_deinit(&predicate->nodes[node_index].value);
        planned_value_deinit(&predicate->nodes[node_index].upper_value);
        for (size_t value_index = 0U; value_index < predicate->nodes[node_index].value_count;
             ++value_index) {
            planned_value_deinit(&predicate->nodes[node_index].values[value_index]);
        }
        free(predicate->nodes[node_index].values);
        if (predicate->nodes[node_index].row_scalar_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_expression);
    }
    free(predicate->nodes);
    *predicate = (struct planned_select_predicate){0};
}

static void planned_exists_subquery_deinit(struct planned_exists_subquery *subquery) {
    if (subquery == NULL) {
        return;
    }

    free(subquery->source.columns);
    planned_select_predicate_deinit_without_exists(&subquery->predicate);
    *subquery = (struct planned_exists_subquery){.has_table_source = false};
}

static void planned_in_subquery_deinit(struct planned_in_subquery *subquery) {
    if (subquery == NULL) {
        return;
    }

    free(subquery->source.columns);
    planned_select_predicate_deinit_without_exists(&subquery->predicate);
    *subquery = (struct planned_in_subquery){0};
}

static int plan_select_predicate_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    struct predicate_work_item *items = NULL;
    size_t item_count = 0U;
    size_t *result_indexes = NULL;
    size_t result_index_count = 0U;
    int rc = append_predicate_work_node(database, &items, &item_count, predicate_node);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_work_item item = items[--item_count];
        rc = plan_select_predicate_work_item(
            database,
            item,
            &items,
            &item_count,
            &result_indexes,
            &result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    if (rc == MYLITE_OK && result_index_count == 1U) {
        out_predicate->root_index = result_indexes[0];
        out_predicate->has_root = true;
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        rc = MYLITE_ERROR;
    }

    free(result_indexes);
    free(items);
    return rc;
}

static int plan_select_predicate_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    struct predicate_work_item *items = NULL;
    size_t item_count = 0U;
    size_t *result_indexes = NULL;
    size_t result_index_count = 0U;
    int rc = append_predicate_work_node(database, &items, &item_count, predicate_node);

    while (rc == MYLITE_OK && item_count > 0U) {
        struct predicate_work_item item = items[--item_count];
        rc = plan_select_predicate_work_item_without_exists(
            database,
            item,
            &items,
            &item_count,
            &result_indexes,
            &result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    if (rc == MYLITE_OK && result_index_count == 1U) {
        out_predicate->root_index = result_indexes[0];
        out_predicate->has_root = true;
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        rc = MYLITE_ERROR;
    }

    free(result_indexes);
    free(items);
    return rc;
}

static int plan_select_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    switch (item.kind) {
    case PREDICATE_WORK_DEPRECATED_AND_WARNING:
        return append_deprecated_logical_and_warning(database);
    case PREDICATE_WORK_DEPRECATED_OR_WARNING:
        return append_deprecated_logical_or_warning(database);
    case PREDICATE_WORK_FINISH_LOGICAL:
        return finish_planned_select_logical_predicate(
            database,
            item.operator_kind,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_FINISH_NOT:
        return finish_planned_select_not_predicate(
            database,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_NODE:
        return plan_select_predicate_ast_node(
            database,
            item.node,
            items,
            item_count,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static int plan_select_predicate_work_item_without_exists(
    struct mylite_db *database,
    struct predicate_work_item item,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    switch (item.kind) {
    case PREDICATE_WORK_DEPRECATED_AND_WARNING:
        return append_deprecated_logical_and_warning(database);
    case PREDICATE_WORK_DEPRECATED_OR_WARNING:
        return append_deprecated_logical_or_warning(database);
    case PREDICATE_WORK_FINISH_LOGICAL:
        return finish_planned_select_logical_predicate(
            database,
            item.operator_kind,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_FINISH_NOT:
        return finish_planned_select_not_predicate(
            database,
            result_indexes,
            result_index_count,
            out_predicate
        );
    case PREDICATE_WORK_NODE:
        return plan_select_predicate_ast_node_without_exists(
            database,
            item.node,
            items,
            item_count,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static int finish_planned_select_logical_predicate(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    size_t **result_indexes,
    size_t *result_index_count,
    struct planned_select_predicate *out_predicate
) {
    enum planned_select_predicate_kind kind = PLANNED_SELECT_PREDICATE_NONE;
    size_t left_index = 0U;
    size_t right_index = 0U;
    size_t node_index = 0U;
    struct planned_select_predicate_node logical_node = {0};
    int rc = MYLITE_OK;

    if (!planned_predicate_kind_for_operator(operator_kind, &kind)) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return MYLITE_ERROR;
    }

    rc = pop_predicate_result_index(*result_indexes, result_index_count, &right_index);
    if (rc == MYLITE_OK) {
        rc = pop_predicate_result_index(*result_indexes, result_index_count, &left_index);
    }
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return rc;
    }

    logical_node = (struct planned_select_predicate_node){
        .kind = kind,
        .operator_kind = operator_kind,
        .left_index = left_index,
        .right_index = right_index,
    };
    rc = append_planned_select_predicate_node(database, out_predicate, &logical_node, &node_index);
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static int finish_planned_select_not_predicate(
    struct mylite_db *database,
    size_t **result_indexes,
    size_t *result_index_count,
    struct planned_select_predicate *out_predicate
) {
    size_t child_index = 0U;
    size_t node_index = 0U;
    struct planned_select_predicate_node not_node = {0};
    int rc = pop_predicate_result_index(*result_indexes, result_index_count, &child_index);

    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
        return rc;
    }

    not_node = (struct planned_select_predicate_node){
        .kind = PLANNED_SELECT_PREDICATE_NOT,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT,
        .left_index = child_index,
        .right_index = SIZE_MAX,
    };
    rc = append_planned_select_predicate_node(database, out_predicate, &not_node, &node_index);
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static int plan_select_predicate_ast_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(predicate_node);

    if (current != NULL && current->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return append_select_predicate_not_work(database, current, items, item_count);
    }
    if (is_logical_predicate_node(current)) {
        return append_select_predicate_logical_work(database, current, items, item_count);
    }
    if (current != NULL && (predicate_node_is_find_in_set_expression(current) ||
                            predicate_node_is_regexp_like_expression(current) ||
                            predicate_node_is_json_valid_expression(current) ||
                            predicate_node_is_json_contains_expression(current) ||
                            predicate_node_is_string_length_expression(current) ||
                            (options != NULL && options->allow_date_format_numeric_predicate &&
                             predicate_node_is_date_format_numeric_predicate_expression(current)) ||
                            predicate_node_is_temporal_extract_expression(current) ||
                            predicate_node_is_scalar_literal_expression(current) ||
                            current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_EXISTS_PREDICATE)) {
        return plan_select_predicate_leaf_node(
            database,
            current,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static int plan_select_predicate_ast_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(predicate_node);

    if (current != NULL && current->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return append_select_predicate_not_work(database, current, items, item_count);
    }
    if (is_logical_predicate_node(current)) {
        return append_select_predicate_logical_work(database, current, items, item_count);
    }
    if (current != NULL && (predicate_node_is_find_in_set_expression(current) ||
                            predicate_node_is_regexp_like_expression(current) ||
                            predicate_node_is_json_valid_expression(current) ||
                            predicate_node_is_json_contains_expression(current) ||
                            predicate_node_is_string_length_expression(current) ||
                            (options != NULL && options->allow_date_format_numeric_predicate &&
                             predicate_node_is_date_format_numeric_predicate_expression(current)) ||
                            predicate_node_is_temporal_extract_expression(current) ||
                            predicate_node_is_scalar_literal_expression(current) ||
                            current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IN_PREDICATE)) {
        return plan_select_predicate_leaf_node_without_exists(
            database,
            current,
            result_indexes,
            result_index_count,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only descriptor column WHERE predicates");
    return MYLITE_ERROR;
}

static const struct mylite_sql_ast_node *unwrap_parenthesized_predicate(
    const struct mylite_sql_ast_node *node
) {
    const struct mylite_sql_ast_node *current = node;

    while (current != NULL && current->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        current = child_at(current, 0U);
    }

    return current;
}

static bool is_logical_predicate_node(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }

    if (node->kind == MYLITE_SQL_AST_AND_PREDICATE) {
        return true;
    }
    if (node->kind == MYLITE_SQL_AST_OR_PREDICATE) {
        return true;
    }
    if (node->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        return true;
    }
    return false;
}

static int append_select_predicate_logical_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate_node);
    int rc = append_predicate_work_finish_logical(database, items, item_count, operator_kind);

    if (rc == MYLITE_OK) {
        rc = append_predicate_work_node(database, items, item_count, child_at(predicate_node, 1U));
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_deprecated_warning_work(
            database,
            operator_kind,
            items,
            item_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_predicate_work_node(database, items, item_count, child_at(predicate_node, 0U));
    }

    return rc;
}

static int append_select_predicate_not_work(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct predicate_work_item **items,
    size_t *item_count
) {
    int rc = append_predicate_work_finish_not(database, items, item_count);

    if (rc == MYLITE_OK) {
        rc = append_predicate_work_node(database, items, item_count, child_at(predicate_node, 0U));
    }

    return rc;
}

static int append_select_predicate_deprecated_warning_work(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    struct predicate_work_item **items,
    size_t *item_count
) {
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND) {
        return append_predicate_work_deprecated_and_warning(database, items, item_count);
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR) {
        return append_predicate_work_deprecated_or_warning(database, items, item_count);
    }

    return MYLITE_OK;
}

static int plan_select_predicate_leaf_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    size_t node_index = 0U;
    int rc = MYLITE_OK;

    if (predicate_node_is_find_in_set_expression(predicate_node)) {
        rc = plan_find_in_set_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_regexp_like_expression(predicate_node)) {
        rc = plan_regexp_like_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_valid_expression(predicate_node)) {
        rc = plan_json_valid_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_contains_expression(predicate_node)) {
        rc = plan_json_contains_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_string_length_expression(predicate_node)) {
        rc = plan_string_length_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_date_format_numeric_predicate_expression(predicate_node)) {
        rc = plan_date_format_numeric_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_temporal_extract_expression(predicate_node)) {
        rc = plan_temporal_extract_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_scalar_literal_expression(predicate_node)) {
        rc = plan_scalar_literal_truth_predicate(
            database,
            predicate_node,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = plan_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = plan_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE) {
        rc = plan_is_boolean_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = plan_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = plan_in_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else {
        rc = plan_exists_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static int plan_select_predicate_leaf_node_without_exists(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    size_t **result_indexes,
    size_t *result_index_count,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    size_t node_index = 0U;
    int rc = MYLITE_OK;

    if (predicate_node_is_find_in_set_expression(predicate_node)) {
        rc = plan_find_in_set_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_regexp_like_expression(predicate_node)) {
        rc = plan_regexp_like_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_valid_expression(predicate_node)) {
        rc = plan_json_valid_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_json_contains_expression(predicate_node)) {
        rc = plan_json_contains_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_string_length_expression(predicate_node)) {
        rc = plan_string_length_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_date_format_numeric_predicate_expression(predicate_node)) {
        rc = plan_date_format_numeric_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_temporal_extract_expression(predicate_node)) {
        rc = plan_temporal_extract_truth_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node_is_scalar_literal_expression(predicate_node)) {
        rc = plan_scalar_literal_truth_predicate(
            database,
            predicate_node,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = plan_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            options,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = plan_is_null_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE) {
        rc = plan_is_boolean_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else if (predicate_node->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        rc = plan_between_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    } else {
        rc = plan_in_literal_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            out_predicate,
            &node_index
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_predicate_result_index(database, result_indexes, result_index_count, node_index);
    }

    return rc;
}

static bool planned_predicate_kind_for_operator(
    enum mylite_sql_ast_operator operator_kind,
    enum planned_select_predicate_kind *out_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        *out_kind = PLANNED_SELECT_PREDICATE_AND;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        *out_kind = PLANNED_SELECT_PREDICATE_OR;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
        *out_kind = PLANNED_SELECT_PREDICATE_XOR;
        return true;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
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
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
    case MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_AST_OPERATOR_CONCAT:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        break;
    }

    return false;
}

static int plan_comparison_predicate(
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
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    bool row_scalar_function_handled = false;
    int row_scalar_function_rc = plan_row_scalar_function_comparison_predicate(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        predicate,
        out_node_index,
        &row_scalar_function_handled
    );

    if (row_scalar_function_handled || row_scalar_function_rc != MYLITE_OK) {
        return row_scalar_function_rc;
    }

    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 1U))) {
            return plan_scalar_literal_comparison_predicate(
                database,
                predicate_node,
                predicate,
                out_node_index
            );
        }
        if (predicate_node_is_column_reference(child_at(predicate_node, 1U))) {
            return plan_literal_left_column_comparison_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                predicate,
                out_node_index
            );
        }
    }

    if (comparison_predicate_rhs_is_column_reference(predicate_node)) {
        if (options != NULL && options->allow_same_scope_column_reference_rhs) {
            return plan_same_scope_column_comparison_predicate(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                predicate,
                out_node_index
            );
        }
        if (options == NULL || !options->allow_column_reference_rhs) {
            set_unsupported_error(
                database,
                "WHERE column-to-column predicates are supported only inside EXISTS"
            );
            return MYLITE_ERROR;
        }
        return plan_column_comparison_predicate(
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
        rc = validate_comparison_predicate_column(database, &node);
    }
    if (rc == MYLITE_OK) {
        if (comparison_operator_is_like(node.operator_kind)) {
            node.like_uses_escape = (bool)!session_sql_mode_has(
                &database->session,
                MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES
            );
            rc = convert_predicate_string_pattern_literal(
                database,
                child_at(predicate_node, 1U),
                &node.value
            );
        } else if (comparison_operator_is_regexp(node.operator_kind)) {
            rc = convert_predicate_regexp_pattern_literal(
                database,
                child_at(predicate_node, 1U),
                &node.value
            );
        } else if (predicate_comparison_value_is_null(child_at(predicate_node, 1U))) {
            node.value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        } else {
            rc = convert_predicate_value_with_date_comparison_mode(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.value,
                &node.compare_date_as_datetime
            );
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_row_scalar_function_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate_node, 0U);

    *out_handled = true;
    if (predicate_node_is_find_in_set_expression(left_node)) {
        return plan_find_in_set_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_regexp_like_expression(left_node)) {
        return plan_regexp_like_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_valid_expression(left_node)) {
        return plan_json_valid_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_json_contains_expression(left_node)) {
        return plan_json_contains_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_string_length_expression(left_node)) {
        return plan_string_length_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_substring_expression(left_node)) {
        return plan_substring_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }
    if (predicate_node_is_temporal_extract_expression(left_node)) {
        return plan_temporal_extract_comparison_predicate(
            database,
            predicate_node,
            source_context,
            table_columns,
            table_column_count,
            predicate,
            out_node_index
        );
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int plan_scalar_literal_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_where_scalar_literal_expression(database, predicate_node, &node);

    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_scalar_literal_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_where_scalar_literal_expression(database, child_at(predicate_node, 0U), &node);

    if (rc == MYLITE_OK) {
        rc = plan_where_scalar_literal_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_literal_left_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = comparison_operator_flipped(mylite_sql_ast_node_operator(predicate_node)),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = resolve_predicate_column_with_source_index(
        database,
        child_at(predicate_node, 1U),
        source_context,
        table_columns,
        table_column_count,
        &node.column,
        &node.column_source_index
    );

    if (rc == MYLITE_OK) {
        rc = validate_comparison_predicate_column(database, &node);
    }
    if (rc == MYLITE_OK) {
        if (predicate_comparison_value_is_null(child_at(predicate_node, 0U))) {
            node.value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        } else {
            rc = convert_predicate_value_with_date_comparison_mode(
                database,
                child_at(predicate_node, 0U),
                &node.column,
                &node.value,
                &node.compare_date_as_datetime
            );
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_same_scope_column_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
        .value_is_column_reference = true,
    };
    int rc = MYLITE_OK;

    if (node.operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        set_unsupported_error(database, "WHERE column-to-column predicates support only =");
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
        rc = resolve_predicate_column_with_source_index(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            &node.value_column,
            &node.value_column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!same_scope_column_comparison_is_supported(&node)) {
        set_unsupported_error(
            database,
            "WHERE column-to-column predicates support only same-family integer or string "
            "descriptor equality"
        );
        return MYLITE_ERROR;
    }

    predicate->qualify_column_references = true;
    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static bool same_scope_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
) {
    return join_condition_columns_are_compatible(&node->column, &node->value_column);
}

static int plan_scalar_literal_is_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_value value = {.is_null = false, .is_text = false, .integer = 0};
    int rc = plan_where_scalar_literal_value(database, child_at(predicate_node, 0U), &value);
    bool truth = false;

    if (rc == MYLITE_OK) {
        truth = scalar_literal_is_result(&value, mylite_sql_ast_node_operator(predicate_node));
        rc = plan_constant_truth_predicate(database, truth, predicate, out_node_index);
    }
    planned_value_deinit(&value);
    return rc;
}

static int plan_constant_truth_predicate(
    struct mylite_db *database,
    bool truth,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    node.row_scalar_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    node.row_scalar_expression->value =
        (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
    if (truth) {
        node.row_scalar_expression->value.integer = 1;
    }

    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_where_scalar_literal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    node->row_scalar_expression->kind = PLANNED_ROW_SCALAR_EXPRESSION_VALUE;
    rc = plan_where_scalar_literal_value(database, expression, &node->row_scalar_expression->value);
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_where_scalar_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "WHERE scalar literal predicates support only integer, boolean, and NULL literals",
        "WHERE scalar literal integer literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = is_null, .is_text = false, .integer = value};
    if (is_null) {
        out_value->integer = 0;
    }
    return MYLITE_OK;
}

static bool predicate_node_is_scalar_literal_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_predicate(predicate_node);
    const struct mylite_sql_ast_node *literal = node;

    if (node != NULL && node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        literal = unwrap_parenthesized_predicate(child_at(node, 0U));
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    switch (mylite_sql_ast_node_literal_kind(literal)) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_NULL:
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_column_reference(const struct mylite_sql_ast_node *node) {
    return (node != NULL && (node->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) != 0;
}

static bool predicate_comparison_value_is_null(const struct mylite_sql_ast_node *value_node) {
    return (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static enum mylite_sql_ast_operator comparison_operator_flipped(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return MYLITE_SQL_AST_OPERATOR_GREATER;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return MYLITE_SQL_AST_OPERATOR_LESS;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_LESS_EQUAL;
    default:
        return operator_kind;
    }
}

static bool scalar_literal_is_result(
    const struct planned_value *value,
    enum mylite_sql_ast_operator operator_kind
) {
    bool is_true = false;
    bool is_false = false;
    bool is_unknown = false;

    if (value == NULL || value->is_null) {
        is_unknown = true;
    } else if (value->integer == 0) {
        is_false = true;
    } else {
        is_true = true;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
        return is_true;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
        if (is_true) {
            return false;
        }
        return true;
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
        return is_false;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        if (is_false) {
            return false;
        }
        return true;
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return is_unknown;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        if (is_unknown) {
            return false;
        }
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_find_in_set_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            predicate_node->kind == MYLITE_SQL_AST_FIND_IN_SET_FUNCTION) != 0;
}

static bool predicate_node_is_json_valid_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL && predicate_node->kind == MYLITE_SQL_AST_JSON_VALID_FUNCTION) !=
           0;
}

static bool predicate_node_is_json_contains_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            (predicate_node->kind == MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION ||
             predicate_node->kind == MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION)) != 0;
}

static bool predicate_node_is_regexp_like_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            predicate_node->kind == MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION) != 0;
}

static bool predicate_node_is_string_length_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL && mylite_execution_is_string_length_function_kind(predicate_node->kind)) != 0;
}

static bool predicate_node_is_substring_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    if (predicate_node == NULL) {
        return false;
    }

    switch (predicate_node->kind) {
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
        return true;
    default:
        return false;
    }
}

static bool predicate_node_is_temporal_extract_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    return (predicate_node != NULL &&
            (predicate_node->kind == MYLITE_SQL_AST_EXTRACT_FUNCTION ||
             mylite_execution_is_temporal_extract_function_kind(predicate_node->kind))) != 0;
}

static bool predicate_node_is_date_format_numeric_predicate_expression(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *left = NULL;

    predicate_node = unwrap_parenthesized_predicate(predicate_node);
    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }

    left = unwrap_parenthesized_expression(child_at(predicate_node, 0U));
    return (left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION) != 0;
}

static int plan_date_format_numeric_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "DATE_FORMAT() numeric predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node.row_scalar_expression = calloc(1U, sizeof(*node.row_scalar_expression));
    if (node.row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_date_format_numeric_equal_expression(
        database,
        predicate_node,
        true,
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

static int plan_temporal_extract_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_temporal_extract_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_temporal_extract_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_temporal_extract_predicate_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_temporal_extract_is_null_predicate(
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
    int rc = plan_temporal_extract_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_temporal_extract_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    const struct mylite_sql_ast_node *argument = NULL;
    int mode = 0;
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "temporal extract function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_resolve_temporal_extract_call(
        database,
        expression,
        &extract_kind,
        &argument,
        &mode
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (argument == NULL) {
        return MYLITE_MISUSE;
    }
    (void)mode;
    if (!temporal_extract_predicate_kind_is_numeric(extract_kind)) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only numeric temporal extractor functions"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_temporal_extract_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_temporal_extract_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int64_t value = 0;
    bool is_null = false;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        }
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only integer, boolean, and NULL comparison "
            "literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        is_null = true;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        value = 1;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        value = 0;
    } else if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "temporal extract predicates support only integer, boolean, and NULL comparison "
            "literals"
        );
        return MYLITE_ERROR;
    } else if (
        parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)
    ) {
        set_unsupported_error(
            database,
            "temporal extract predicate comparison literals must fit the signed 64-bit range"
        );
        return MYLITE_ERROR;
    } else if (is_negative && magnitude == (uint64_t)INT64_MAX + 1U) {
        value = INT64_MIN;
    } else if (is_negative) {
        value = -(int64_t)magnitude;
    } else {
        value = (int64_t)magnitude;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static bool temporal_extract_predicate_kind_is_numeric(enum mylite_temporal_extract_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_QUARTER:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_YEAR_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_WEEK:
    case MYLITE_TEMPORAL_EXTRACT_WEEKDAY:
    case MYLITE_TEMPORAL_EXTRACT_WEEKOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_YEARWEEK:
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_MICROSECOND:
    case MYLITE_TEMPORAL_EXTRACT_SIGNED_MICROSECOND:
    case MYLITE_TEMPORAL_EXTRACT_DAY_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_DAY_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_DAY_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_HOUR_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_HOUR_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
    case MYLITE_TEMPORAL_EXTRACT_TO_DAYS:
    case MYLITE_TEMPORAL_EXTRACT_TO_SECONDS:
        return true;
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
    case MYLITE_TEMPORAL_EXTRACT_DAYNAME:
    case MYLITE_TEMPORAL_EXTRACT_MONTHNAME:
        return false;
    }
    return false;
}

static int plan_string_length_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_string_length_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_string_length_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_string_length_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc =
            plan_string_length_predicate_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_string_length_is_null_predicate(
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
    int rc = plan_string_length_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_string_length_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "string length function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_string_length_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_string_length_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "string length predicates support only integer, boolean, and NULL comparison literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
        literal_kind != MYLITE_SQL_AST_LITERAL_TRUE &&
        literal_kind != MYLITE_SQL_AST_LITERAL_FALSE &&
        literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
        set_unsupported_error(
            database,
            "string length predicates support only integer, boolean, and NULL comparison literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "string length predicates support only integer, boolean, and NULL comparison literals",
        "string length predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    int64_t planned_integer = value;
    if (is_null) {
        planned_integer = 0;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = planned_integer,
    };
    return MYLITE_OK;
}

static int plan_substring_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_substring_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_substring_predicate_value(database, child_at(predicate_node, 1U), &node.value);
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_substring_is_null_predicate(
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
    int rc = plan_substring_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_substring_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "SUBSTRING function predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_string_slice_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_substring_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    if (predicate_comparison_value_is_null(expression)) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }

    return convert_predicate_string_literal(database, expression, out_value);
}

static int plan_find_in_set_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_find_in_set_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_find_in_set_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_find_in_set_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_find_in_set_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_find_in_set_is_null_predicate(
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
    int rc = plan_find_in_set_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_find_in_set_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "FIND_IN_SET() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_find_in_set_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_find_in_set_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    int64_t planned_integer = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "FIND_IN_SET() predicates support only integer and boolean comparison literals",
        "FIND_IN_SET() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!is_null) {
        planned_integer = value;
    }
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = planned_integer,
    };
    return MYLITE_OK;
}

static int plan_regexp_like_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_regexp_like_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_regexp_like_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_regexp_like_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_regexp_like_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_regexp_like_is_null_predicate(
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
    int rc = plan_regexp_like_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_regexp_like_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "REGEXP_LIKE() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_regexp_like_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_regexp_like_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "REGEXP_LIKE() predicates support only integer and boolean comparison literals",
        "REGEXP_LIKE() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static int plan_json_valid_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_valid_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_json_valid_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_valid_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_json_valid_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_valid_is_null_predicate(
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
    int rc = plan_json_valid_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_json_valid_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "JSON_VALID() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_json_valid_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_json_valid_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "JSON_VALID() predicates support only integer and boolean comparison literals",
        "JSON_VALID() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static int plan_json_contains_truth_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_contains_predicate_expression(
        database,
        predicate_node,
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_json_contains_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    int rc = plan_json_contains_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
    );

    if (rc == MYLITE_OK) {
        rc = plan_json_contains_predicate_integer_value(
            database,
            child_at(predicate_node, 1U),
            &node.value
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_row_scalar_expression_deinit(node.row_scalar_expression);
        free(node.row_scalar_expression);
    }
    return rc;
}

static int plan_json_contains_is_null_predicate(
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
    int rc = plan_json_contains_predicate_expression(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &node
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

static int plan_json_contains_predicate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate_node *node
) {
    int rc = MYLITE_OK;

    if (select_source_context_is_joined(source_context)) {
        set_unsupported_error(
            database,
            "JSON_CONTAINS() predicates support only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    node->row_scalar_expression = calloc(1U, sizeof(*node->row_scalar_expression));
    if (node->row_scalar_expression == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_row_scalar_json_expression(
        database,
        expression,
        true,
        source_context,
        table_columns,
        table_column_count,
        COLUMN_REFERENCE_WHERE,
        node->row_scalar_expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(node->row_scalar_expression);
        free(node->row_scalar_expression);
        node->row_scalar_expression = NULL;
    }
    return rc;
}

static int plan_json_contains_predicate_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_value *out_value
) {
    int64_t value = 0;
    bool is_null = false;
    int rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "JSON_CONTAINS() predicates support only integer and boolean comparison literals",
        "JSON_CONTAINS() predicate comparison literals must fit the signed 64-bit range",
        &value,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){
        .is_null = is_null,
        .is_text = false,
        .integer = value,
    };
    return MYLITE_OK;
}

static bool comparison_predicate_rhs_is_column_reference(
    const struct mylite_sql_ast_node *predicate_node
) {
    const struct mylite_sql_ast_node *right = child_at(predicate_node, 1U);

    return (right != NULL && (right->kind == MYLITE_SQL_AST_IDENTIFIER ||
                              right->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) != 0;
}

static int validate_comparison_predicate_column(
    struct mylite_db *database,
    const struct planned_select_predicate_node *node
) {
    if (comparison_operator_is_like(node->operator_kind) &&
        column_descriptor_is_enum(&node->column)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        column_descriptor_is_set(&node->column)) {
        set_unsupported_error(database, "WHERE set predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_like(node->operator_kind) &&
        !column_descriptor_is_string_family(&node->column)) {
        set_unsupported_error(database, "WHERE LIKE predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (comparison_operator_is_regexp(node->operator_kind) &&
        !column_descriptor_is_string_family(&node->column)) {
        set_unsupported_error(database, "WHERE REGEXP predicates support only string columns");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(&node->column) &&
        !comparison_operator_is_string_predicate(node->operator_kind)) {
        set_unsupported_error(
            database,
            "WHERE string predicates support only =, <=>, <>, !=, <, <=, >, >=, LIKE, REGEXP, "
            "and RLIKE"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_enum(&node->column) &&
        !comparison_operator_is_enum_predicate(node->operator_kind)) {
        set_unsupported_error(database, "WHERE enum predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_set(&node->column) &&
        !comparison_operator_is_enum_predicate(node->operator_kind)) {
        set_unsupported_error(database, "WHERE set predicates support only =, <=>, <>, and !=");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_exists_predicate(
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
        .kind = PLANNED_SELECT_PREDICATE_EXISTS,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NONE,
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    struct planned_exists_subquery *subquery = NULL;
    int rc = MYLITE_OK;

    if (options == NULL || !options->allow_exists) {
        set_unsupported_error(database, "EXISTS subqueries are supported only in SELECT WHERE");
        return MYLITE_ERROR;
    }
    if (source_context == NULL || select_source_context_is_joined(source_context)) {
        set_unsupported_error(database, "EXISTS subqueries support only one outer table source");
        return MYLITE_ERROR;
    }

    subquery = calloc(1U, sizeof(*subquery));
    if (subquery == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = plan_exists_subquery(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        subquery
    );
    if (rc != MYLITE_OK) {
        planned_exists_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    node.exists_subquery = subquery;
    rc = append_planned_select_predicate_node(database, predicate, &node, out_node_index);
    if (rc != MYLITE_OK) {
        planned_exists_subquery_deinit(subquery);
        free(subquery);
        return rc;
    }

    predicate->qualify_column_references = true;
    return MYLITE_OK;
}

static int plan_exists_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_exists_subquery){.has_table_source = false};
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "EXISTS supports only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(database, "EXISTS subqueries do not support SELECT modifiers");
        return MYLITE_ERROR;
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "EXISTS subqueries support only WHERE and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        rc = plan_exists_table_source(database, from_clause, out_subquery);
    }
    if (rc == MYLITE_OK) {
        rc = validate_exists_select_list(database, select_list, out_subquery);
    }
    if (rc == MYLITE_OK && where_clause != NULL && !out_subquery->has_table_source) {
        set_unsupported_error(database, "EXISTS tableless subqueries do not support WHERE");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && out_subquery->has_table_source) {
        rc = plan_exists_inner_predicate(
            database,
            where_clause,
            outer_source_context,
            outer_columns,
            outer_column_count,
            out_subquery
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_delete_limit(database, limit_clause, &out_subquery->limit);
    }

    return rc;
}

static int plan_exists_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_exists_subquery *out_subquery
) {
    int rc = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "EXISTS subqueries support only one table source");
        return MYLITE_ERROR;
    }

    rc = plan_joined_select_source(database, from_clause, &out_subquery->source);
    if (rc == MYLITE_OK) {
        out_subquery->has_table_source = true;
    }
    return rc;
}

static int validate_exists_select_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_exists_subquery *subquery
) {
    struct select_source_context source_context = {0};

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (subquery->has_table_source) {
        source_context = (struct select_source_context){
            .source = &subquery->source.source,
            .has_alias = subquery->source.has_alias,
        };
        memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
    }

    for (const struct mylite_sql_ast_node *item = child_at(select_list, 0U); item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        struct mylite_catalog_column_descriptor column = {0};

        if (expression == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (expression->kind == MYLITE_SQL_AST_WILDCARD ||
            exists_select_item_is_supported_literal(expression)) {
            continue;
        }
        if (!subquery->has_table_source) {
            set_unsupported_error(database, "EXISTS tableless subqueries support only literals");
            return MYLITE_ERROR;
        }
        if (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
            expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
            set_unsupported_error(
                database,
                "EXISTS subqueries support only literal or descriptor-column select items"
            );
            return MYLITE_ERROR;
        }
        int rc = resolve_descriptor_column_reference(
            database,
            expression,
            &source_context,
            COLUMN_REFERENCE_FIELD,
            "EXISTS subqueries support only literal or descriptor-column select items",
            subquery->source.columns,
            subquery->source.column_count,
            &column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static bool exists_select_item_is_supported_literal(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    kind = mylite_sql_ast_node_literal_kind(expression);
    return (kind == MYLITE_SQL_AST_LITERAL_INTEGER || kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            kind == MYLITE_SQL_AST_LITERAL_FALSE || kind == MYLITE_SQL_AST_LITERAL_NULL ||
            kind == MYLITE_SQL_AST_LITERAL_STRING) != 0;
}

static int plan_exists_inner_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_exists_subquery *subquery
) {
    struct select_source_context inner_source_context = {
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };

    memcpy(inner_source_context.alias, subquery->source.alias, sizeof(inner_source_context.alias));
    int rc = MYLITE_OK;

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
        subquery->source.columns,
        subquery->source.column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }

    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(&subquery->predicate, 1U);
    }
    return rc;
}

static void offset_exists_inner_predicate_source_indexes(
    struct planned_select_predicate *predicate,
    size_t source_index_offset
) {
    if (predicate == NULL || source_index_offset == 0U) {
        return;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        struct planned_select_predicate_node *node = &predicate->nodes[node_index];

        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON && node->value_is_column_reference) {
            continue;
        }
        if (node->kind == PLANNED_SELECT_PREDICATE_COMPARISON ||
            node->kind == PLANNED_SELECT_PREDICATE_IS_NULL ||
            node->kind == PLANNED_SELECT_PREDICATE_IS_BOOLEAN ||
            node->kind == PLANNED_SELECT_PREDICATE_BETWEEN ||
            node->kind == PLANNED_SELECT_PREDICATE_IN) {
            node->column_source_index += source_index_offset;
        }
    }
}

static int plan_column_comparison_predicate(
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
        .kind = PLANNED_SELECT_PREDICATE_COMPARISON,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
        .value_is_column_reference = true,
    };
    int rc = MYLITE_OK;

    if (node.operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL &&
        node.operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        set_unsupported_error(database, "EXISTS correlated predicates support only = and <=>");
        return MYLITE_ERROR;
    }

    rc = resolve_exists_column_reference(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        options->outer_source_context,
        options->outer_columns,
        options->outer_column_count,
        &node.column,
        &node.column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = resolve_exists_column_reference(
            database,
            child_at(predicate_node, 1U),
            source_context,
            table_columns,
            table_column_count,
            options->outer_source_context,
            options->outer_columns,
            options->outer_column_count,
            &node.value_column,
            &node.value_column_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!exists_correlated_column_comparison_is_supported(&node)) {
        set_unsupported_error(
            database,
            "EXISTS correlated predicates support one inner and one outer integer column"
        );
        return MYLITE_ERROR;
    }

    predicate->qualify_column_references = true;
    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int resolve_exists_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *inner_source_context,
    const struct mylite_catalog_column_descriptor *inner_columns,
    size_t inner_column_count,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    bool resolved = false;
    int rc = MYLITE_OK;

    *out_column = (struct mylite_catalog_column_descriptor){0};
    *out_source_index = 0U;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "EXISTS correlated predicates require descriptor columns");
        return MYLITE_ERROR;
    }

    rc = collect_column_reference_parts(database, column_node, parts, &part_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        format_column_reference_name(database, parts, part_count, column_name, sizeof(column_name));
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = resolve_exists_column_reference_in_source(
        database,
        parts,
        part_count,
        column_name,
        inner_source_context,
        inner_columns,
        inner_column_count,
        out_column,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        *out_source_index = 1U;
        return rc;
    }

    rc = resolve_exists_column_reference_in_source(
        database,
        parts,
        part_count,
        column_name,
        outer_source_context,
        outer_columns,
        outer_column_count,
        out_column,
        &resolved
    );
    if (rc != MYLITE_OK || resolved) {
        *out_source_index = 0U;
        return rc;
    }

    set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
    return MYLITE_ERROR;
}

static int resolve_exists_column_reference_in_source(
    struct mylite_db *database,
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const char *column_name,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_resolved
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_resolved = false;
    if (!column_reference_qualifier_matches_source(parts, part_count, source_context)) {
        return MYLITE_OK;
    }

    rc = find_column_index(columns, column_count, parts[part_count - 1U], &column_index);
    if (rc != MYLITE_OK) {
        if (part_count > 1U) {
            set_unknown_column_reference_error(database, COLUMN_REFERENCE_WHERE, column_name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    *out_column = columns[column_index];
    *out_resolved = true;
    return MYLITE_OK;
}

static bool exists_correlated_column_comparison_is_supported(
    const struct planned_select_predicate_node *node
) {
    bool left_is_outer = node->column_source_index == 0U;
    bool right_is_outer = node->value_column_source_index == 0U;

    if (left_is_outer == right_is_outer) {
        return false;
    }
    if (!join_condition_column_is_integer_family(&node->column)) {
        return false;
    }
    return join_condition_column_is_integer_family(&node->value_column);
}

static bool comparison_operator_is_string_predicate(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_REGEXP:
    case MYLITE_SQL_AST_OPERATOR_RLIKE:
        return true;
    default:
        return false;
    }
}

static bool comparison_operator_is_enum_predicate(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return true;
    default:
        return false;
    }
}

static bool comparison_operator_is_like(enum mylite_sql_ast_operator operator_kind) {
    return operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE;
}

static bool comparison_operator_is_regexp(enum mylite_sql_ast_operator operator_kind) {
    return (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) != 0;
}

static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_NULL,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };

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

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
}

static int plan_is_boolean_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *predicate,
    size_t *out_node_index
) {
    struct planned_select_predicate_node node = {
        .kind = PLANNED_SELECT_PREDICATE_IS_BOOLEAN,
        .operator_kind = mylite_sql_ast_node_operator(predicate_node),
        .left_index = SIZE_MAX,
        .right_index = SIZE_MAX,
    };
    if (predicate_node_is_scalar_literal_expression(child_at(predicate_node, 0U))) {
        return plan_scalar_literal_is_predicate(
            database,
            predicate_node,
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
            rc = convert_predicate_value(
                database,
                child_at(predicate_node, 1U),
                &node.column,
                &node.value
            );
            if (rc == MYLITE_OK) {
                rc = convert_predicate_value(
                    database,
                    child_at(predicate_node, 2U),
                    &node.column,
                    &node.upper_value
                );
            }
        }
    }
    if (rc != MYLITE_OK) {
        planned_value_deinit(&node.value);
        planned_value_deinit(&node.upper_value);
        return rc;
    }

    return append_planned_select_predicate_node(database, predicate, &node, out_node_index);
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

    if (predicate_node == NULL || predicate_node->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "WHERE supports only descriptor column IN predicates");
        return MYLITE_ERROR;
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
        if (source_context == NULL || select_source_context_is_joined(source_context)) {
            set_unsupported_error(database, "IN subqueries support only one outer table source");
            return MYLITE_ERROR;
        }
        rc = plan_in_subquery_predicate(
            database,
            right,
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
        planned_in_subquery_deinit(node.in_subquery);
        free(node.in_subquery);
        return rc;
    }
    predicate->qualify_column_references = true;

    return rc;
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
        for (size_t value_index = 0U; value_index < node.value_count; ++value_index) {
            planned_value_deinit(&node.values[value_index]);
        }
        free(node.values);
        return rc;
    }

    return rc;
}

static int plan_in_subquery_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
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

static int plan_in_subquery(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_source_context *outer_source_context,
    const struct mylite_catalog_column_descriptor *outer_columns,
    size_t outer_column_count,
    struct planned_in_subquery *out_subquery
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    int rc = MYLITE_OK;

    *out_subquery = (struct planned_in_subquery){0};
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "IN subqueries support only SELECT subqueries");
        return MYLITE_ERROR;
    }
    select_list = child_at(statement, 0U);
    from_clause = child_at(statement, 1U);
    optional_clause = child_at(statement, 2U);
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
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

static int plan_in_subquery_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_in_subquery *out_subquery
) {
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "IN subqueries support one descriptor table source");
        return MYLITE_ERROR;
    }

    return plan_joined_select_source(database, from_clause, &out_subquery->source);
}

static int plan_in_subquery_select_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct planned_in_subquery *subquery,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    struct select_source_context source_context = {0};

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

    source_context = (struct select_source_context){
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    memcpy(source_context.alias, subquery->source.alias, sizeof(source_context.alias));
    return resolve_descriptor_column_reference(
        database,
        expression,
        &source_context,
        COLUMN_REFERENCE_FIELD,
        "IN subqueries support one explicit descriptor select column",
        subquery->source.columns,
        subquery->source.column_count,
        out_column
    );
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
    struct select_source_context inner_source_context = {
        .source = &subquery->source.source,
        .has_alias = subquery->source.has_alias,
    };
    const struct select_predicate_plan_options options = {
        .allow_column_reference_rhs = true,
        .outer_source_context = outer_source_context,
        .outer_columns = outer_columns,
        .outer_column_count = outer_column_count,
    };
    int rc = MYLITE_OK;

    memcpy(inner_source_context.alias, subquery->source.alias, sizeof(inner_source_context.alias));
    subquery->predicate = (struct planned_select_predicate){0};
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
        subquery->source.columns,
        subquery->source.column_count,
        &options,
        &subquery->predicate
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&subquery->predicate);
    }
    if (rc == MYLITE_OK && subquery->predicate.qualify_column_references) {
        offset_exists_inner_predicate_source_indexes(&subquery->predicate, 1U);
    }
    return rc;
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

static int append_planned_select_predicate_node(
    struct mylite_db *database,
    struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *out_node_index
) {
    struct planned_select_predicate_node *nodes = NULL;
    size_t required_count = predicate->node_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*nodes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    nodes = realloc(predicate->nodes, required_count * sizeof(*nodes));
    if (nodes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    predicate->nodes = nodes;
    predicate->nodes[predicate->node_count] = *node;
    if (out_node_index != NULL) {
        *out_node_index = predicate->node_count;
    }
    predicate->node_count = required_count;

    return MYLITE_OK;
}

static int append_deprecated_logical_and_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_deprecated_logical_and,
        "HY000",
        "'&&' is deprecated and will be removed in a future release. Please use AND instead"
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_deprecated_logical_or_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_deprecated_logical_or,
        "HY000",
        "'|| as a synonym for OR' is deprecated and will be removed in a future release. Please "
        "use OR instead"
    );

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static bool planned_select_predicate_has_expression(
    const struct planned_select_predicate *predicate
) {
    if (predicate == NULL) {
        return false;
    }

    return predicate->has_root;
}

static int append_predicate_work_node(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_NODE, .node = node}
    );
}

static int append_predicate_work_deprecated_and_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_AND_WARNING}
    );
}

static int append_predicate_work_deprecated_or_warning(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_DEPRECATED_OR_WARNING}
    );
}

static int append_predicate_work_finish_logical(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){
            .kind = PREDICATE_WORK_FINISH_LOGICAL,
            .operator_kind = operator_kind,
        }
    );
}

static int append_predicate_work_finish_not(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count
) {
    return append_predicate_work_item(
        database,
        items,
        item_count,
        (struct predicate_work_item){.kind = PREDICATE_WORK_FINISH_NOT}
    );
}

static int append_predicate_work_item(
    struct mylite_db *database,
    struct predicate_work_item **items,
    size_t *item_count,
    struct predicate_work_item item
) {
    struct predicate_work_item *grown_items = NULL;
    size_t required_count = *item_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(*items, required_count * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *items = grown_items;
    (*items)[*item_count] = item;
    *item_count = required_count;

    return MYLITE_OK;
}

static int append_predicate_result_index(
    struct mylite_db *database,
    size_t **indexes,
    size_t *index_count,
    size_t index
) {
    size_t *grown_indexes = NULL;
    size_t required_count = *index_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_indexes = realloc(*indexes, required_count * sizeof(*grown_indexes));
    if (grown_indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *indexes = grown_indexes;
    (*indexes)[*index_count] = index;
    *index_count = required_count;

    return MYLITE_OK;
}

static int pop_predicate_result_index(
    const size_t *indexes,
    size_t *index_count,
    size_t *out_index
) {
    if (indexes == NULL || index_count == NULL || *index_count == 0U || out_index == NULL) {
        return MYLITE_ERROR;
    }

    --(*index_count);
    *out_index = indexes[*index_count];
    return MYLITE_OK;
}

static int resolve_predicate_column_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_WHERE,
        "WHERE supports only unqualified predicate columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

static int convert_predicate_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct planned_value){.is_null = false, .integer = 0};
    if (value_node == NULL || column == NULL) {
        set_unsupported_error(database, "WHERE supports only integer predicate literals");
        return MYLITE_ERROR;
    }

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "WHERE supports only integer predicate literals");
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "WHERE supports only integer or boolean predicate literals"
            );
            return MYLITE_ERROR;
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
    }

    rc = convert_integer_for_predicate(
        database,
        magnitude,
        is_negative,
        column,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = false;

    return MYLITE_OK;
}

static int convert_predicate_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_string_family(column)) {
        return convert_predicate_string_literal(database, value_node, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return convert_predicate_enum_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return convert_predicate_set_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_bit(column)) {
        return convert_bit_literal(database, value_node, column, 1U, false, out_value);
    }
    if (column_descriptor_is_year(column)) {
        return convert_predicate_year_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_date(column)) {
        return convert_predicate_date_literal(database, value_node, column, false, out_value, NULL);
    }
    if (column_descriptor_is_time(column)) {
        return convert_predicate_time_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_predicate_datetime_literal(database, value_node, column, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_predicate_timestamp_literal(database, value_node, column, out_value);
    }
    return convert_predicate_integer_literal(database, value_node, column, out_value);
}

static int convert_predicate_value_with_date_comparison_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_compare_date_as_datetime
) {
    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = false;
    }
    if (column_descriptor_is_date(column)) {
        return convert_predicate_date_literal(
            database,
            value_node,
            column,
            false,
            out_value,
            out_compare_date_as_datetime
        );
    }
    return convert_predicate_value(database, value_node, column, out_value);
}

static int convert_predicate_date_between_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *lower_node,
    const struct mylite_sql_ast_node *upper_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_lower_value,
    struct planned_value *out_upper_value,
    bool *out_compare_date_as_datetime
) {
    bool lower_is_datetime = false;
    bool upper_is_datetime = false;
    int rc = MYLITE_OK;

    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = false;
    }
    rc = convert_predicate_date_literal(
        database,
        lower_node,
        column,
        false,
        out_lower_value,
        &lower_is_datetime
    );
    if (rc == MYLITE_OK) {
        rc = convert_predicate_date_literal(
            database,
            upper_node,
            column,
            false,
            out_upper_value,
            &upper_is_datetime
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!lower_is_datetime && !upper_is_datetime) {
        return MYLITE_OK;
    }
    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = true;
    }
    if (!lower_is_datetime) {
        planned_value_deinit(out_lower_value);
        rc = convert_predicate_date_literal(
            database,
            lower_node,
            column,
            true,
            out_lower_value,
            NULL
        );
    }
    if (rc == MYLITE_OK && !upper_is_datetime) {
        planned_value_deinit(out_upper_value);
        rc = convert_predicate_date_literal(
            database,
            upper_node,
            column,
            true,
            out_upper_value,
            NULL
        );
    }

    return rc;
}

static int convert_predicate_date_in_value_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value **out_values,
    size_t *out_value_count,
    bool *out_compare_date_as_datetime
) {
    struct planned_value *values = NULL;
    size_t value_count = 0U;
    bool uses_datetime = false;
    int rc = MYLITE_OK;

    *out_values = NULL;
    *out_value_count = 0U;
    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = false;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST ||
        mylite_sql_ast_node_child_count(value_list) == 0U) {
        set_unsupported_error(database, "WHERE supports only nonempty IN predicate lists");
        return MYLITE_ERROR;
    }
    value_count = mylite_sql_ast_node_child_count(value_list);
    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = convert_predicate_date_in_values(
        database,
        value_list,
        column,
        values,
        value_count,
        &uses_datetime
    );
    if (rc == MYLITE_OK && uses_datetime) {
        rc = convert_predicate_date_in_values_to_datetime(
            database,
            value_list,
            column,
            values,
            value_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_value_array_deinit(values, value_count);
        free(values);
        return rc;
    }

    if (out_compare_date_as_datetime != NULL) {
        *out_compare_date_as_datetime = uses_datetime;
    }
    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static int convert_predicate_date_in_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count,
    bool *out_uses_datetime
) {
    *out_uses_datetime = false;
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, value_index);
        bool value_is_datetime = false;
        int rc = MYLITE_OK;

        if (predicate_comparison_value_is_null(value_node)) {
            values[value_index] = (struct planned_value){.is_null = true, .integer = 0};
            continue;
        }
        rc = convert_predicate_date_literal(
            database,
            value_node,
            column,
            false,
            &values[value_index],
            &value_is_datetime
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (value_is_datetime) {
            *out_uses_datetime = true;
        }
    }
    return MYLITE_OK;
}

static int convert_predicate_date_in_values_to_datetime(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *values,
    size_t value_count
) {
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, value_index);

        if (date_in_value_already_compares_as_datetime(value_node, &values[value_index])) {
            continue;
        }
        planned_value_deinit(&values[value_index]);
        int rc = convert_predicate_date_literal(
            database,
            value_node,
            column,
            true,
            &values[value_index],
            NULL
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static bool date_in_value_already_compares_as_datetime(
    const struct mylite_sql_ast_node *value_node,
    const struct planned_value *value
) {
    if (predicate_comparison_value_is_null(value_node)) {
        return true;
    }
    if (!value->is_text) {
        return false;
    }
    if (value->text_length != datetime_text_length) {
        return false;
    }
    return true;
}

static void planned_value_array_deinit(struct planned_value *values, size_t value_count) {
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        planned_value_deinit(&values[value_index]);
    }
}

static int convert_predicate_enum_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_enum_string_literal(
            database,
            value_node,
            column,
            1U,
            false,
            false,
            ENUM_STRING_PRESERVE_TRAILING_SPACES,
            out_value
        );
    }
    return convert_enum_integer_literal(database, value_node, column, 1U, false, out_value);
}

static int convert_predicate_set_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return MYLITE_OK;
    }
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_set_string_literal(
            database,
            literal,
            column,
            1U,
            false,
            false,
            false,
            out_value
        );
    }
    return convert_set_integer_literal(database, value_node, column, 1U, false, out_value);
}

static int convert_predicate_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    return convert_year_literal(database, value_node, column, 1U, false, out_value);
}

static int convert_predicate_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE string predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE string predicates support only string literals",
        "WHERE string predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(text, text_length)) {
        free(text);
        set_unsupported_error(database, "WHERE string predicate literals support only ASCII text");
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_string_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "WHERE LIKE predicates support only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE LIKE predicates support only string pattern literals",
        "WHERE LIKE pattern literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(text, text_length)) {
        free(text);
        set_unsupported_error(database, "WHERE LIKE pattern literals support only ASCII text");
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_regexp_pattern_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    struct mylite_regexp_program *program = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "WHERE REGEXP predicates support only string pattern literals"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE REGEXP predicates support only string pattern literals",
        "WHERE REGEXP pattern literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_value_is_supported_string_key(text, text_length)) {
        free(text);
        set_unsupported_error(database, "WHERE REGEXP pattern literals support only ASCII text");
        return MYLITE_ERROR;
    }

    enum mylite_regexp_compile_status status =
        mylite_regexp_compile_ascii_ci(text, text_length, &program);
    mylite_regexp_program_free(program);
    if (status != MYLITE_REGEXP_COMPILE_OK) {
        free(text);
        if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        if (status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
            set_regexp_error(
                database,
                "The regular expression contains an unclosed bracket expression."
            );
            return MYLITE_ERROR;
        }
        if (status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
            set_regexp_character_range_error(
                database,
                "The regular expression contains an invalid character range."
            );
            return MYLITE_ERROR;
        }
        set_unsupported_error(
            database,
            "WHERE REGEXP patterns support only MyLite's baseline ASCII regular expression subset"
        );
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool force_datetime_comparison,
    struct planned_value *out_value,
    bool *out_compare_as_datetime
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    bool append_warning = false;
    int rc = MYLITE_OK;

    if (out_compare_as_datetime != NULL) {
        *out_compare_as_datetime = false;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE DATE predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE DATE predicates support only string literals",
        "WHERE DATE predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate_date_text_admitted(database, text, text_length)) {
        if (force_datetime_comparison) {
            rc = copy_date_midnight_predicate_value(database, text, out_value);
            free(text);
            return rc;
        }
        return assign_text_value(database, text, text_length, out_value);
    }

    if (normalize_date_datetime_predicate_text(text, text_length, normalized, &append_warning)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_date_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        if (append_warning) {
            rc = append_incorrect_date_predicate_warning(database, text, column->name);
            if (rc != MYLITE_OK) {
                free(text);
                return rc;
            }
        }
        if (out_compare_as_datetime != NULL) {
            *out_compare_as_datetime = true;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }

    set_incorrect_date_literal_error(database, text);
    free(text);
    return MYLITE_ERROR;
}

static bool normalize_date_datetime_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text,
    bool *out_append_warning
) {
    int ignored_offset_minutes = 0;

    if (text == NULL || out_text == NULL || out_append_warning == NULL) {
        return false;
    }
    *out_append_warning = false;
    if (text_length == datetime_text_length && (text[datetime_date_time_separator_offset] == ' ' ||
                                                text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }
    if (text_length == datetime_text_length + temporal_predicate_offset_text_length &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        if (!datetime_text_has_canonical_shape(out_text, datetime_text_length)) {
            return false;
        }
        return parse_temporal_predicate_offset(
            text + datetime_text_length,
            &ignored_offset_minutes
        );
    }
    if (text_length == datetime_text_length + 1U &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T') &&
        (text[datetime_text_length] == 'Z' || text[datetime_text_length] == 'z')) {
        memcpy(out_text, text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        *out_append_warning = true;
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }

    return false;
}

static int copy_date_midnight_predicate_value(
    struct mylite_db *database,
    const char *date_text,
    struct planned_value *out_value
) {
    char text[datetime_text_length + 1U];

    if (date_text == NULL) {
        return MYLITE_MISUSE;
    }
    memcpy(text, date_text, date_text_length);
    memcpy(text + date_text_length, " 00:00:00", datetime_text_length - date_text_length);
    text[datetime_text_length] = '\0';
    return copy_text_value(database, text, out_value);
}

static int convert_predicate_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int64_t seconds = 0;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE TIME predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE TIME predicates support only string literals",
        "WHERE TIME predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!time_text_to_seconds(text, text_length, &seconds)) {
        set_incorrect_time_value_error(database, text, column->name, 1U);
        free(text);
        return MYLITE_ERROR;
    }
    free(text);

    *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = seconds};
    return MYLITE_OK;
}

static int convert_predicate_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE DATETIME predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE DATETIME predicates support only string literals",
        "WHERE DATETIME predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_datetime_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!predicate_datetime_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_datetime_literal_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        rc = append_incorrect_datetime_predicate_warning(database, text, column->name);
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (!predicate_datetime_text_admitted(database, text, text_length)) {
        set_incorrect_datetime_literal_error(database, text);
        free(text);
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static int convert_predicate_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    char normalized[datetime_text_length + 1U];
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "WHERE TIMESTAMP predicates support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "WHERE TIMESTAMP predicates support only string literals",
        "WHERE TIMESTAMP predicate literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = 0;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        if (!predicate_timestamp_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_timestamp_value_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!predicate_timestamp_text_admitted(database, normalized, datetime_text_length)) {
            set_incorrect_timestamp_value_error(database, text);
            free(text);
            return MYLITE_ERROR;
        }
        rc = append_incorrect_datetime_predicate_warning(database, text, column->name);
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        free(text);
        return copy_text_value(database, normalized, out_value);
    }
    if (!predicate_timestamp_text_admitted(database, text, text_length)) {
        set_incorrect_timestamp_value_error(database, text);
        free(text);
        return MYLITE_ERROR;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static bool normalize_iso_temporal_predicate_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text
) {
    struct mylite_temporal_datetime_parts parts = {0};
    int source_offset_minutes = 0;

    if (input == NULL || input->text == NULL || out_text == NULL) {
        return false;
    }
    if (input->text_length == datetime_text_length &&
        input->text[datetime_date_time_separator_offset] == 'T') {
        memcpy(out_text, input->text, datetime_text_length);
        out_text[datetime_date_time_separator_offset] = ' ';
        out_text[datetime_text_length] = '\0';
        return datetime_text_has_canonical_shape(out_text, datetime_text_length);
    }
    if (input->text_length != datetime_text_length + sizeof("+00:00") - 1U ||
        (input->text[datetime_date_time_separator_offset] != ' ' &&
         input->text[datetime_date_time_separator_offset] != 'T')) {
        return false;
    }

    memcpy(out_text, input->text, datetime_text_length);
    out_text[datetime_date_time_separator_offset] = ' ';
    out_text[datetime_text_length] = '\0';
    if (!mylite_temporal_arithmetic_parse_datetime_text(out_text, datetime_text_length, &parts)) {
        return false;
    }
    if (!parse_temporal_predicate_offset(
            input->text + datetime_text_length,
            &source_offset_minutes
        )) {
        return false;
    }

    return shift_datetime_parts_by_minutes(
        &parts,
        input->target_offset_minutes - source_offset_minutes,
        out_text
    );
}

static bool normalize_z_temporal_predicate_text(
    const char *text,
    size_t text_length,
    char *out_text
) {
    if (text == NULL || out_text == NULL) {
        return false;
    }
    if (text_length != datetime_text_length + 1U ||
        (text[datetime_date_time_separator_offset] != ' ' &&
         text[datetime_date_time_separator_offset] != 'T') ||
        (text[datetime_text_length] != 'Z' && text[datetime_text_length] != 'z')) {
        return false;
    }

    memcpy(out_text, text, datetime_text_length);
    out_text[datetime_date_time_separator_offset] = ' ';
    out_text[datetime_text_length] = '\0';
    return datetime_text_has_canonical_shape(out_text, datetime_text_length);
}

static bool parse_temporal_predicate_offset(const char *text, int *out_offset_minutes) {
    int sign = 0;
    int hours = 0;
    int minutes = 0;
    int total_minutes = 0;

    if (text == NULL || out_offset_minutes == NULL) {
        return false;
    }
    if (text[temporal_predicate_offset_sign_index] == '+') {
        sign = 1;
    } else if (text[temporal_predicate_offset_sign_index] == '-') {
        sign = -1;
    } else {
        return false;
    }
    if (text[temporal_predicate_offset_hour_tens_index] < '0' ||
        text[temporal_predicate_offset_hour_tens_index] > '9' ||
        text[temporal_predicate_offset_hour_ones_index] < '0' ||
        text[temporal_predicate_offset_hour_ones_index] > '9' ||
        text[temporal_predicate_offset_separator_index] != ':' ||
        text[temporal_predicate_offset_minute_tens_index] < '0' ||
        text[temporal_predicate_offset_minute_tens_index] > '9' ||
        text[temporal_predicate_offset_minute_ones_index] < '0' ||
        text[temporal_predicate_offset_minute_ones_index] > '9' ||
        text[temporal_predicate_offset_text_length] != '\0') {
        return false;
    }

    hours = ((text[temporal_predicate_offset_hour_tens_index] - '0') * decimal_base) +
            (text[temporal_predicate_offset_hour_ones_index] - '0');
    minutes = ((text[temporal_predicate_offset_minute_tens_index] - '0') * decimal_base) +
              (text[temporal_predicate_offset_minute_ones_index] - '0');
    if (minutes > time_zone_max_minute) {
        return false;
    }
    total_minutes = (hours * time_zone_minutes_per_hour) + minutes;
    if (total_minutes > temporal_predicate_offset_max_minutes) {
        return false;
    }
    if (sign < 0 && total_minutes == 0) {
        return false;
    }

    *out_offset_minutes = sign * total_minutes;
    return true;
}

static bool shift_datetime_parts_by_minutes(
    const struct mylite_temporal_datetime_parts *parts,
    int delta_minutes,
    char *out_text
) {
    const int64_t seconds_per_day = mylite_temporal_arithmetic_seconds_per_day();
    struct mylite_temporal_day_second shifted = {0};
    struct mylite_temporal_datetime_parts output = {0};
    int64_t days = 0;
    int64_t day_seconds = 0;
    int64_t base_seconds = 0;
    int64_t shifted_seconds = 0;
    int64_t result_day_seconds = 0;

    if (parts == NULL || out_text == NULL) {
        return false;
    }

    days = mylite_temporal_arithmetic_days_from_datetime(parts);
    day_seconds = ((int64_t)parts->hour * (int64_t)time_second_per_hour) +
                  ((int64_t)parts->minute * (int64_t)time_second_per_minute) +
                  (int64_t)parts->second;
    base_seconds = (days * seconds_per_day) + day_seconds;
    if (!mylite_temporal_arithmetic_checked_add_int64(
            base_seconds,
            (int64_t)delta_minutes * time_zone_seconds_per_minute,
            &shifted_seconds
        )) {
        return false;
    }

    shifted = mylite_temporal_arithmetic_floor_divmod_seconds(shifted_seconds);
    mylite_temporal_arithmetic_civil_from_days(shifted.days, &output);
    if (output.year < date_minimum_year || output.year > date_maximum_year) {
        return false;
    }
    result_day_seconds = shifted.day_seconds;
    output.hour = (uint32_t)(result_day_seconds / time_second_per_hour);
    result_day_seconds %= time_second_per_hour;
    output.minute = (uint32_t)(result_day_seconds / time_second_per_minute);
    output.second = (uint32_t)(result_day_seconds % time_second_per_minute);

    return format_datetime_parts_text(&output, out_text);
}

static bool format_datetime_parts_text(
    const struct mylite_temporal_datetime_parts *parts,
    char *out_text
) {
    int written = 0;

    if (parts == NULL || out_text == NULL) {
        return false;
    }

    written = snprintf(
        out_text,
        datetime_text_length + 1U,
        "%04" PRId64 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
        parts->year,
        parts->month,
        parts->day,
        parts->hour,
        parts->minute,
        parts->second
    );
    return written == datetime_text_length;
}

static bool predicate_date_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    enum temporal_text_kind kind = classify_date_text(text, text_length);

    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return true;
    case TEMPORAL_TEXT_FULL_ZERO:
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (temporal_sql_mode_has_no_zero_in_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        return temporal_sql_mode_has_allow_invalid_dates(database);
    case TEMPORAL_TEXT_INVALID:
    case TEMPORAL_TEXT_DEFERRED:
        return false;
    }
    return false;
}

static bool predicate_datetime_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    enum temporal_text_kind kind = classify_datetime_text(text, text_length);

    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return true;
    case TEMPORAL_TEXT_FULL_ZERO:
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (temporal_sql_mode_has_no_zero_in_date(database)) {
            return false;
        }
        return true;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        return temporal_sql_mode_has_allow_invalid_dates(database);
    case TEMPORAL_TEXT_INVALID:
    case TEMPORAL_TEXT_DEFERRED:
        return false;
    }
    return false;
}

static bool predicate_timestamp_text_admitted(
    const struct mylite_db *database,
    const char *text,
    size_t text_length
) {
    if (timestamp_text_is_canonical_valid(text, text_length)) {
        return true;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        if (temporal_sql_mode_has_no_zero_date(database)) {
            return false;
        }
        return true;
    }
    return false;
}

static int convert_integer_for_predicate(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "WHERE supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        if (magnitude == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return MYLITE_OK;
    }
    if (magnitude > range.positive_max) {
        set_predicate_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}
