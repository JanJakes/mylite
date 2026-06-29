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
        .allow_scalar_count_subquery_comparison = true,
        .allow_same_scope_column_reference_rhs = true,
        .allow_sql_auto_is_null_lookup = true,
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
        .allow_in_subquery = true,
        .allow_quantified_subquery = true,
        .allow_scalar_subquery_in_literal = true,
        .allow_scalar_count_subquery_comparison = true,
        .allow_same_scope_column_reference_rhs = true,
        .allow_date_format_numeric_predicate = true,
        .allow_sql_auto_is_null_lookup = true,
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

static int plan_select_having_predicate_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct select_predicate_plan_options *options,
    struct planned_select_predicate *out_predicate
) {
    int rc = MYLITE_OK;

    *out_predicate = (struct planned_select_predicate){0};
    out_predicate->qualify_column_references = select_source_context_is_joined(source_context);
    if (having_clause == NULL) {
        return MYLITE_OK;
    }
    if (having_clause->kind != MYLITE_SQL_AST_HAVING_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only descriptor column HAVING predicates");
        return MYLITE_ERROR;
    }

    rc = plan_select_predicate_node(
        database,
        child_at(having_clause, 0U),
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
        for (size_t value_index = 0U;
             value_index < predicate->nodes[node_index].value_count &&
             predicate->nodes[node_index].row_scalar_value_expressions != NULL;
             ++value_index) {
            planned_row_scalar_expression_deinit(
                &predicate->nodes[node_index].row_scalar_value_expressions[value_index]
            );
        }
        free(predicate->nodes[node_index].row_scalar_value_expressions);
        if (predicate->nodes[node_index].row_scalar_expression != NULL) {
            planned_row_scalar_expression_deinit(predicate->nodes[node_index].row_scalar_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_expression);
        if (predicate->nodes[node_index].row_scalar_value_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_value_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_value_expression);
        if (predicate->nodes[node_index].row_scalar_upper_value_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_upper_value_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_upper_value_expression);
        planned_exists_subquery_deinit(predicate->nodes[node_index].exists_subquery);
        free(predicate->nodes[node_index].exists_subquery);
        planned_in_subquery_deinit(predicate->nodes[node_index].in_subquery);
        free(predicate->nodes[node_index].in_subquery);
        planned_scalar_aggregate_subquery_deinit(
            predicate->nodes[node_index].subject_scalar_subquery
        );
        free(predicate->nodes[node_index].subject_scalar_subquery);
        planned_scalar_aggregate_subquery_deinit(predicate->nodes[node_index].value_scalar_subquery
        );
        free(predicate->nodes[node_index].value_scalar_subquery);
        planned_scalar_aggregate_subquery_deinit(predicate->nodes[node_index].upper_scalar_subquery
        );
        free(predicate->nodes[node_index].upper_scalar_subquery);
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
        for (size_t value_index = 0U;
             value_index < predicate->nodes[node_index].value_count &&
             predicate->nodes[node_index].row_scalar_value_expressions != NULL;
             ++value_index) {
            planned_row_scalar_expression_deinit(
                &predicate->nodes[node_index].row_scalar_value_expressions[value_index]
            );
        }
        free(predicate->nodes[node_index].row_scalar_value_expressions);
        if (predicate->nodes[node_index].row_scalar_expression != NULL) {
            planned_row_scalar_expression_deinit(predicate->nodes[node_index].row_scalar_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_expression);
        if (predicate->nodes[node_index].row_scalar_value_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_value_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_value_expression);
        if (predicate->nodes[node_index].row_scalar_upper_value_expression != NULL) {
            planned_row_scalar_expression_deinit(
                predicate->nodes[node_index].row_scalar_upper_value_expression
            );
        }
        free(predicate->nodes[node_index].row_scalar_upper_value_expression);
        planned_scalar_aggregate_subquery_deinit(
            predicate->nodes[node_index].subject_scalar_subquery
        );
        free(predicate->nodes[node_index].subject_scalar_subquery);
        planned_scalar_aggregate_subquery_deinit(predicate->nodes[node_index].value_scalar_subquery
        );
        free(predicate->nodes[node_index].value_scalar_subquery);
        planned_scalar_aggregate_subquery_deinit(predicate->nodes[node_index].upper_scalar_subquery
        );
        free(predicate->nodes[node_index].upper_scalar_subquery);
    }
    free(predicate->nodes);
    *predicate = (struct planned_select_predicate){0};
}

static void planned_exists_subquery_deinit(struct planned_exists_subquery *subquery) {
    if (subquery == NULL) {
        return;
    }

    planned_select_source_deinit_shallow(&subquery->source);
    for (size_t source_index = 0U; source_index < subquery->source_count; ++source_index) {
        planned_select_source_deinit_shallow(&subquery->sources[source_index]);
    }
    free(subquery->sources);
    free(subquery->join_kinds);
    if (subquery->join_conditions != NULL) {
        for (size_t join_index = 0U; join_index < subquery->join_count; ++join_index) {
            planned_select_join_condition_deinit(&subquery->join_conditions[join_index]);
        }
    }
    free(subquery->join_conditions);
    planned_select_predicate_deinit_without_exists(&subquery->predicate);
    *subquery = (struct planned_exists_subquery){.has_table_source = false};
}

static void planned_in_subquery_deinit(struct planned_in_subquery *subquery) {
    if (subquery == NULL) {
        return;
    }

    planned_select_source_deinit_shallow(&subquery->source);
    for (size_t source_index = 0U; source_index < subquery->source_count; ++source_index) {
        planned_select_source_deinit_shallow(&subquery->sources[source_index]);
    }
    free(subquery->sources);
    free(subquery->join_kinds);
    if (subquery->join_conditions != NULL) {
        for (size_t join_index = 0U; join_index < subquery->join_count; ++join_index) {
            planned_select_join_condition_deinit(&subquery->join_conditions[join_index]);
        }
    }
    free(subquery->join_conditions);
    planned_select_predicate_deinit_without_exists(&subquery->predicate);
    *subquery = (struct planned_in_subquery){0};
}

static void planned_scalar_aggregate_subquery_deinit(
    struct planned_scalar_aggregate_subquery *subquery
) {
    if (subquery == NULL) {
        return;
    }

    planned_exists_subquery_deinit(&subquery->subquery);
    *subquery = (struct planned_scalar_aggregate_subquery){0};
}

static int merge_select_predicate_with_and(
    struct mylite_db *database,
    struct planned_select_predicate *target,
    struct planned_select_predicate *addition
) {
    struct planned_select_predicate_node *nodes = NULL;
    struct planned_select_predicate_node logical_node = {0};
    size_t original_count = 0U;
    size_t node_index = 0U;
    bool addition_qualifies = false;
    int rc = MYLITE_OK;

    if (target == NULL || addition == NULL || !addition->has_root) {
        return MYLITE_OK;
    }
    if (!target->has_root) {
        *target = *addition;
        *addition = (struct planned_select_predicate){0};
        return MYLITE_OK;
    }
    if (addition->node_count > (SIZE_MAX - target->node_count - 1U)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    addition_qualifies = addition->qualify_column_references;
    original_count = target->node_count;
    nodes = realloc(
        target->nodes,
        (target->node_count + addition->node_count) * sizeof(*target->nodes)
    );
    if (nodes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    target->nodes = nodes;
    memcpy(
        &target->nodes[target->node_count],
        addition->nodes,
        addition->node_count * sizeof(*addition->nodes)
    );
    target->node_count += addition->node_count;
    for (size_t index = original_count; index < target->node_count; ++index) {
        if (target->nodes[index].left_index != SIZE_MAX) {
            target->nodes[index].left_index += original_count;
        }
        if (target->nodes[index].right_index != SIZE_MAX) {
            target->nodes[index].right_index += original_count;
        }
    }

    logical_node = (struct planned_select_predicate_node){
        .kind = PLANNED_SELECT_PREDICATE_AND,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LOGICAL_AND,
        .left_index = target->root_index,
        .right_index = addition->root_index + original_count,
    };
    free(addition->nodes);
    *addition = (struct planned_select_predicate){0};

    rc = append_planned_select_predicate_node(database, target, &logical_node, &node_index);
    if (rc == MYLITE_OK) {
        target->root_index = node_index;
        target->has_root = true;
        target->qualify_column_references = target->qualify_column_references || addition_qualifies;
    }
    return rc;
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
                            predicate_node_is_sounds_like_expression(current) ||
                            predicate_node_is_supported_row_scalar_expression(current) ||
                            predicate_node_is_scalar_literal_expression(current) ||
                            current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IN_PREDICATE ||
                            predicate_node_is_quantified_subquery(current) ||
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
                            predicate_node_is_sounds_like_expression(current) ||
                            predicate_node_is_supported_row_scalar_expression(current) ||
                            predicate_node_is_scalar_literal_expression(current) ||
                            current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_BETWEEN_PREDICATE ||
                            current->kind == MYLITE_SQL_AST_IN_PREDICATE ||
                            predicate_node_is_quantified_subquery(current))) {
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
