static int plan_grouped_aggregate_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *order_items = NULL;
    int rc = MYLITE_OK;

    out_plan->order.has_order = false;
    out_plan->order_uses_aggregate = false;
    out_plan->order_aggregate_index = 0U;
    out_plan->order.direction = PLANNED_SELECT_ORDER_DEFAULT;
    out_plan->order.column_source_index = 0U;
    out_plan->order.qualify_column_reference = select_source_context_is_joined(source_context);
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "GROUP BY supports descriptor ORDER BY columns");
        return MYLITE_ERROR;
    }

    order_items = child_at(order_clause, 0U);
    if (order_items != NULL && order_items->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        const struct mylite_sql_ast_node *item = child_at(order_items, 0U);

        while (item != NULL) {
            if (item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
                set_unsupported_error(database, "GROUP BY supports descriptor ORDER BY columns");
                return MYLITE_ERROR;
            }
            rc = plan_grouped_aggregate_order_ast_item_and_append(
                database,
                (struct select_order_ast_item_nodes){
                    .order_key = child_at(item, 0U),
                    .direction = child_at(item, 1U),
                },
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            item = item->next_sibling;
        }
    } else {
        rc = plan_grouped_aggregate_order_ast_item_and_append(
            database,
            (struct select_order_ast_item_nodes){
                .order_key = child_at(order_clause, 0U),
                .direction = child_at(order_clause, 1U),
            },
            source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_plan->order.has_order = out_plan->order.item_count > 0U;
    return MYLITE_OK;
}

static int plan_grouped_aggregate_order_ast_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    struct planned_select_order_item planned_item = {
        .kind = PLANNED_SELECT_ORDER_ITEM_COLUMN,
        .direction = PLANNED_SELECT_ORDER_ASC,
        .column_source_index = 0U,
    };
    bool uses_aggregate = false;
    size_t aggregate_index = 0U;
    size_t group_order_index = 0U;
    int rc = resolve_grouped_order_key(
        database,
        item_nodes.order_key,
        source_context,
        table_columns,
        table_column_count,
        out_plan,
        &planned_item,
        &uses_aggregate,
        &aggregate_index
    );

    if (rc != MYLITE_OK) {
        mylite_execution_planned_select_order_item_deinit(&planned_item);
        return rc;
    }
    mylite_execution_apply_select_order_item_direction(item_nodes.direction, &planned_item);
    if (!uses_aggregate && planned_item.kind == PLANNED_SELECT_ORDER_ITEM_COLUMN &&
        session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY) &&
        !find_matching_grouped_key(
            out_plan,
            planned_item.column,
            planned_item.column_source_index,
            &group_order_index
        )) {
        int dependency_rc = MYLITE_OK;

        if (!grouped_projection_column_is_allowed(
                database,
                out_plan,
                table_columns,
                table_column_count,
                planned_item.column,
                planned_item.column_source_index,
                &dependency_rc
            )) {
            if (dependency_rc != MYLITE_OK) {
                mylite_execution_planned_select_order_item_deinit(&planned_item);
                return dependency_rc;
            }
            set_only_full_group_by_error(
                database,
                1U,
                "ORDER BY clause",
                planned_grouped_aggregate_source_resolution(
                    out_plan,
                    planned_item.column_source_index
                ),
                planned_item.column
            );
            mylite_execution_planned_select_order_item_deinit(&planned_item);
            return MYLITE_ERROR;
        }
    }
    if (!uses_aggregate && planned_item.kind == PLANNED_SELECT_ORDER_ITEM_ROW_SCALAR &&
        session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY)) {
        rc = validate_grouped_order_row_scalar_expression(
            database,
            out_plan,
            table_columns,
            table_column_count,
            planned_item.expression
        );
        if (rc != MYLITE_OK) {
            mylite_execution_planned_select_order_item_deinit(&planned_item);
            return rc;
        }
    }

    rc = append_planned_grouped_order_item(
        database,
        out_plan,
        planned_item,
        uses_aggregate,
        aggregate_index
    );
    if (rc != MYLITE_OK) {
        mylite_execution_planned_select_order_item_deinit(&planned_item);
    }
    return rc;
}

static int validate_grouped_order_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "GROUP BY supports scalar ORDER BY expressions only over grouped, "
            "primary-key-dependent, or literal operands"
        );
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return validate_grouped_order_row_scalar_column(
            database,
            plan,
            table_columns,
            table_column_count,
            expression
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        if (expression->argument_count != 1U || expression->arguments == NULL) {
            set_unsupported_error(
                database,
                "GROUP BY supports scalar ORDER BY expressions only over grouped, "
                "primary-key-dependent, or literal operands"
            );
            return MYLITE_ERROR;
        }
        return validate_grouped_order_row_scalar_expression(
            database,
            plan,
            table_columns,
            table_column_count,
            &expression->arguments[0]
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE:
        if (expression->argument_count != 2U || expression->arguments == NULL) {
            set_unsupported_error(
                database,
                "GROUP BY supports scalar ORDER BY expressions only over grouped, "
                "primary-key-dependent, or literal operands"
            );
            return MYLITE_ERROR;
        }
        for (size_t argument_index = 0U; argument_index < expression->argument_count;
             ++argument_index) {
            int rc = validate_grouped_order_row_scalar_expression(
                database,
                plan,
                table_columns,
                table_column_count,
                &expression->arguments[argument_index]
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        return MYLITE_OK;
    default:
        set_unsupported_error(
            database,
            "GROUP BY supports scalar ORDER BY expressions only over grouped, "
            "primary-key-dependent, or literal operands"
        );
        return MYLITE_ERROR;
    }
}

static int validate_grouped_order_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
) {
    int dependency_rc = MYLITE_OK;
    size_t source_index =
        expression->column_has_source_index ? expression->column_source_index : 0U;

    if (grouped_projection_column_is_allowed(
            database,
            plan,
            table_columns,
            table_column_count,
            expression->column,
            source_index,
            &dependency_rc
        )) {
        return MYLITE_OK;
    }
    if (dependency_rc != MYLITE_OK) {
        return dependency_rc;
    }

    set_only_full_group_by_error(
        database,
        1U,
        "ORDER BY clause",
        planned_grouped_aggregate_source_resolution(plan, source_index),
        expression->column
    );
    return MYLITE_ERROR;
}

static int resolve_grouped_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    struct planned_select_order_item *out_item,
    bool *out_uses_aggregate,
    size_t *out_aggregate_index
) {
    bool matches_alias = false;
    bool matches_unaliased_column = false;
    bool matches_aggregate_expression = false;
    const struct mylite_catalog_column_descriptor *resolved_column = NULL;
    size_t matched_index = 0U;
    size_t order_source_index = 0U;
    int rc = find_grouped_order_projection_alias(
        database,
        order_key,
        out_plan,
        &matches_alias,
        &matched_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        if (out_plan->projections[matched_index].uses_row_scalar_expression) {
            set_unsupported_error(
                database,
                "GROUP BY does not support ORDER BY on selected scalar group aliases"
            );
            return MYLITE_ERROR;
        }
        rc = mylite_execution_copy_planned_column_descriptor(
            database,
            out_plan->projections[matched_index].column,
            &out_item->column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_item->column_source_index = out_plan->projections[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = find_grouped_order_group_alias(
        database,
        order_key,
        out_plan,
        &matches_alias,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        if (out_plan->groups[matched_index].uses_row_scalar_expression) {
            set_unsupported_error(
                database,
                "GROUP BY does not support ORDER BY on selected scalar group aliases"
            );
            return MYLITE_ERROR;
        }
        rc = mylite_execution_copy_planned_column_descriptor(
            database,
            out_plan->groups[matched_index].column,
            &out_item->column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_item->column_source_index = out_plan->groups[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = order_identifier_matches_unaliased_projection_column(
        database,
        order_key,
        out_plan,
        &matches_unaliased_column,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_unaliased_column) {
        rc = mylite_execution_copy_planned_column_descriptor(
            database,
            out_plan->projections[matched_index].column,
            &out_item->column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_item->column_source_index = out_plan->projections[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = order_identifier_matches_unaliased_group_column(
        database,
        order_key,
        out_plan,
        &matches_unaliased_column,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_unaliased_column) {
        rc = mylite_execution_copy_planned_column_descriptor(
            database,
            out_plan->groups[matched_index].column,
            &out_item->column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_item->column_source_index = out_plan->groups[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = find_grouped_order_aggregate_alias(
        database,
        order_key,
        out_plan,
        &matches_alias,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        *out_uses_aggregate = true;
        *out_aggregate_index = matched_index;
        return MYLITE_OK;
    }

    rc = find_grouped_order_aggregate_expression(
        database,
        order_key,
        source_context,
        table_columns,
        table_column_count,
        out_plan,
        &matches_aggregate_expression,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_aggregate_expression) {
        *out_uses_aggregate = true;
        *out_aggregate_index = matched_index;
        return MYLITE_OK;
    }

    if (row_scalar_conversion_kind_from_expression(order_key) !=
            PLANNED_ROW_SCALAR_CONVERSION_NONE ||
        mylite_execution_select_order_key_is_like_predicate(order_key)) {
        rc = mylite_execution_plan_select_order_row_scalar_expression(
            database,
            order_key,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
        return rc;
    }

    rc = resolve_descriptor_column_reference_pointer_with_source_index(
        database,
        order_key,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "GROUP BY supports ORDER BY only on grouped, primary-key-dependent, "
        "or selected aggregate columns",
        table_columns,
        table_column_count,
        &resolved_column,
        &order_source_index
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_copy_planned_column_descriptor(
            database,
            resolved_column,
            &out_item->column
        );
    }
    if (rc == MYLITE_OK) {
        out_item->column_source_index = order_source_index;
    }

    return rc;
}

static int append_planned_grouped_order_item(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    struct planned_select_order_item item,
    bool uses_aggregate,
    size_t aggregate_index
) {
    bool *uses_aggregate_items = NULL;
    size_t *aggregate_indexes = NULL;
    struct mylite_catalog_column_descriptor *primary_column = NULL;
    size_t item_index = plan->order.item_count;
    size_t required_count = item_index + 1U;
    int rc = MYLITE_OK;

    if (item_index == 0U && !uses_aggregate && item.kind == PLANNED_SELECT_ORDER_ITEM_COLUMN) {
        rc =
            mylite_execution_copy_planned_column_descriptor(database, item.column, &primary_column);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    uses_aggregate_items = (bool *)
        realloc(plan->order_item_uses_aggregate, required_count * sizeof(*uses_aggregate_items));
    if (uses_aggregate_items == NULL) {
        free(primary_column);
        return MYLITE_NOMEM;
    }
    plan->order_item_uses_aggregate = uses_aggregate_items;

    aggregate_indexes = (size_t *)
        realloc(plan->order_item_aggregate_indexes, required_count * sizeof(*aggregate_indexes));
    if (aggregate_indexes == NULL) {
        free(primary_column);
        return MYLITE_NOMEM;
    }
    plan->order_item_aggregate_indexes = aggregate_indexes;

    rc = mylite_execution_append_planned_select_order_item(&plan->order, item);
    if (rc != MYLITE_OK) {
        free(primary_column);
        return rc;
    }
    plan->order_item_uses_aggregate[item_index] = uses_aggregate;
    plan->order_item_aggregate_indexes[item_index] = aggregate_index;
    if (item_index == 0U) {
        plan->order.direction = item.direction;
        if (!uses_aggregate && item.kind == PLANNED_SELECT_ORDER_ITEM_COLUMN) {
            plan->order.column = primary_column;
            plan->order.column_source_index = item.column_source_index;
        }
        plan->order_uses_aggregate = uses_aggregate;
        plan->order_aggregate_index = aggregate_index;
    }
    return MYLITE_OK;
}

static int find_grouped_order_group_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
) {
    bool matches_alias = false;
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_group_index = 0U;
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        rc = order_identifier_matches_alias_text(
            database,
            order_key,
            plan->groups[group_index].alias,
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "GROUP BY supports ORDER BY only on unique selected group aliases"
            );
            return MYLITE_ERROR;
        }
        matched_index = group_index;
    }
    if (matched_index == SIZE_MAX) {
        return MYLITE_OK;
    }

    *out_matched = true;
    *out_group_index = matched_index;
    return MYLITE_OK;
}

static int find_grouped_order_projection_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_projection_index
) {
    bool matches_alias = false;
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_projection_index = 0U;
    for (size_t projection_index = 0U; projection_index < plan->projection_count;
         ++projection_index) {
        rc = order_identifier_matches_alias_text(
            database,
            order_key,
            plan->projections[projection_index].has_alias
                ? plan->projections[projection_index].result_label
                : NULL,
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "GROUP BY supports ORDER BY only on unique selected group aliases"
            );
            return MYLITE_ERROR;
        }
        matched_index = projection_index;
    }
    if (matched_index == SIZE_MAX) {
        return MYLITE_OK;
    }

    *out_matched = true;
    *out_projection_index = matched_index;
    return MYLITE_OK;
}

static int find_grouped_order_aggregate_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_aggregate_index
) {
    bool matches_alias = false;
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_aggregate_index = 0U;
    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        rc = order_identifier_matches_alias_text(
            database,
            order_key,
            item->has_alias ? item->result_label : NULL,
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "GROUP BY supports ORDER BY only on unique selected aggregate aliases"
            );
            return MYLITE_ERROR;
        }
        matched_index = aggregate_index;
    }
    if (matched_index == SIZE_MAX) {
        return MYLITE_OK;
    }
    *out_matched = true;
    *out_aggregate_index = matched_index;
    return MYLITE_OK;
}

static int find_grouped_order_aggregate_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_aggregate_index
) {
    struct planned_grouped_aggregate_item order_item = {0};
    enum planned_grouped_aggregate_function function =
        grouped_aggregate_function_from_expression(unwrap_parenthesized_expression(order_key));
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_aggregate_index = 0U;
    if (function == PLANNED_GROUPED_AGGREGATE_NONE) {
        return MYLITE_OK;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT && plan->aggregate_count == 0U) {
        set_unsupported_error(
            database,
            "GROUP BY does not support hidden GROUP_CONCAT() ORDER BY keys"
        );
        return MYLITE_ERROR;
    }

    rc = plan_grouped_aggregate_expression(
        database,
        order_key,
        source_context,
        table_columns,
        table_column_count,
        plan,
        &order_item
    );
    if (rc != MYLITE_OK) {
        goto cleanup;
    }
    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        if (!grouped_order_aggregate_items_match(&order_item, &plan->aggregates[aggregate_index])) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "GROUP BY supports ORDER BY only on unique selected aggregate expressions"
            );
            rc = MYLITE_ERROR;
            goto cleanup;
        }
        matched_index = aggregate_index;
    }
    if (matched_index == SIZE_MAX) {
        rc = append_hidden_grouped_order_aggregate(database, plan, &order_item, &matched_index);
        if (rc != MYLITE_OK) {
            goto cleanup;
        }
    }

    *out_matched = true;
    *out_aggregate_index = matched_index;

cleanup:
    planned_grouped_aggregate_item_deinit(&order_item);
    return rc;
}

static int append_hidden_grouped_order_aggregate(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    struct planned_grouped_aggregate_item *item,
    size_t *out_aggregate_index
) {
    struct planned_grouped_aggregate_item *aggregates = NULL;
    size_t aggregate_index = plan->aggregate_count;
    size_t required_count = aggregate_index + 1U;

    if (item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        set_unsupported_error(
            database,
            "GROUP BY does not support hidden GROUP_CONCAT() ORDER BY keys"
        );
        return MYLITE_ERROR;
    }
    if (plan->aggregate_count >= grouped_aggregate_max_results) {
        set_unsupported_error(database, "GROUP BY supports at most sixteen aggregate results");
        return MYLITE_ERROR;
    }
    if (required_count > SIZE_MAX / sizeof(*aggregates)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    aggregates = realloc(plan->aggregates, required_count * sizeof(*aggregates));
    if (aggregates == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->aggregates = aggregates;
    item->is_hidden_order_key = true;
    plan->aggregates[aggregate_index] = *item;
    plan->aggregate_count = required_count;
    *item = (struct planned_grouped_aggregate_item){0};
    *out_aggregate_index = aggregate_index;
    return MYLITE_OK;
}

static bool grouped_order_aggregate_items_match(
    const struct planned_grouped_aggregate_item *left,
    const struct planned_grouped_aggregate_item *right
) {
    if (left->function != right->function ||
        left->aggregate_is_distinct != right->aggregate_is_distinct) {
        return false;
    }
    if (left->function == PLANNED_GROUPED_AGGREGATE_COUNT_STAR) {
        return true;
    }
    if (left->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        return grouped_order_group_concat_items_match(left, right);
    }
    if (left->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        if (left->aggregate_uses_row_scalar_expression ||
            right->aggregate_uses_row_scalar_expression) {
            return false;
        }
        return planned_grouped_aggregate_columns_match(
            left->aggregate_column,
            left->aggregate_column_source_index,
            right->aggregate_column,
            right->aggregate_column_source_index
        );
    }
    if (left->aggregate_uses_row_scalar_expression && right->aggregate_uses_row_scalar_expression) {
        return grouped_order_row_scalar_aggregate_expressions_match(
            &left->aggregate_row_scalar_expression,
            &right->aggregate_row_scalar_expression
        );
    }
    if (left->aggregate_uses_row_scalar_expression != right->aggregate_uses_row_scalar_expression) {
        return false;
    }
    return planned_grouped_aggregate_columns_match(
        left->aggregate_column,
        left->aggregate_column_source_index,
        right->aggregate_column,
        right->aggregate_column_source_index
    );
}

static bool grouped_order_group_concat_items_match(
    const struct planned_grouped_aggregate_item *left,
    const struct planned_grouped_aggregate_item *right
) {
    if (left->has_separator != right->has_separator ||
        !grouped_order_select_orders_match(&left->aggregate_order, &right->aggregate_order)) {
        return false;
    }
    if (left->has_separator &&
        !grouped_order_planned_values_match(&left->separator, &right->separator)) {
        return false;
    }
    if (left->aggregate_uses_row_scalar_expression && right->aggregate_uses_row_scalar_expression) {
        return grouped_order_row_scalar_aggregate_expressions_match(
            &left->aggregate_row_scalar_expression,
            &right->aggregate_row_scalar_expression
        );
    }
    if (left->aggregate_uses_row_scalar_expression != right->aggregate_uses_row_scalar_expression) {
        return false;
    }
    return planned_grouped_aggregate_columns_match(
        left->aggregate_column,
        left->aggregate_column_source_index,
        right->aggregate_column,
        right->aggregate_column_source_index
    );
}

static bool grouped_order_select_orders_match(
    const struct planned_select_order *left,
    const struct planned_select_order *right
) {
    if (left->has_order != right->has_order || left->direction != right->direction ||
        left->item_count != right->item_count) {
        return false;
    }
    if (!left->has_order) {
        return true;
    }
    return planned_grouped_aggregate_columns_match(
        left->column,
        left->column_source_index,
        right->column,
        right->column_source_index
    );
}

static bool grouped_order_row_scalar_aggregate_expressions_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
) {
    if (!grouped_order_row_scalar_aggregate_expression_fields_match(left, right) ||
        !grouped_order_planned_values_match(&left->value, &right->value) ||
        !grouped_order_row_scalar_conversion_steps_match(left, right)) {
        return false;
    }

    for (size_t argument_index = 0U; argument_index < left->argument_count; ++argument_index) {
        if (!grouped_order_row_scalar_aggregate_expressions_match(
                &left->arguments[argument_index],
                &right->arguments[argument_index]
            )) {
            return false;
        }
    }
    return true;
}

static bool grouped_order_row_scalar_aggregate_expression_fields_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
) {
    if (left == NULL || right == NULL || left->kind != right->kind ||
        left->field_domain != right->field_domain ||
        left->string_length_kind != right->string_length_kind ||
        left->string_case_kind != right->string_case_kind ||
        left->string_codepoint_kind != right->string_codepoint_kind ||
        left->string_trim_kind != right->string_trim_kind ||
        left->string_slice_kind != right->string_slice_kind ||
        left->string_search_kind != right->string_search_kind ||
        left->string_padding_kind != right->string_padding_kind ||
        left->string_bitmask_kind != right->string_bitmask_kind ||
        left->regexp_string_kind != right->regexp_string_kind ||
        left->conversion_kind != right->conversion_kind ||
        left->digest_kind != right->digest_kind ||
        left->numeric_function_kind != right->numeric_function_kind ||
        left->base_conversion_kind != right->base_conversion_kind ||
        left->numeric_extra_kind != right->numeric_extra_kind ||
        left->numeric_extra_has_decimal_scale != right->numeric_extra_has_decimal_scale ||
        left->numeric_extra_decimal_scale != right->numeric_extra_decimal_scale ||
        left->window_function_kind != right->window_function_kind ||
        left->temporal_extract_kind != right->temporal_extract_kind ||
        left->operator_kind != right->operator_kind ||
        left->arithmetic_operator != right->arithmetic_operator ||
        left->json_value_kind != right->json_value_kind ||
        left->json_mutation_kind != right->json_mutation_kind ||
        left->regexp_case_sensitive != right->regexp_case_sensitive ||
        left->like_uses_escape != right->like_uses_escape ||
        left->case_has_else != right->case_has_else ||
        left->has_rand_seed != right->has_rand_seed ||
        left->rand_seed_is_expression != right->rand_seed_is_expression ||
        left->rand_seed != right->rand_seed ||
        left->window_has_partition != right->window_has_partition ||
        left->window_has_order != right->window_has_order ||
        left->window_has_frame != right->window_has_frame ||
        left->conversion_ascii_only != right->conversion_ascii_only ||
        left->conversion_step_count != right->conversion_step_count ||
        left->window_order_direction != right->window_order_direction ||
        left->window_frame_unit != right->window_frame_unit ||
        !planned_row_scalar_columns_match(
            left->window_partition_column,
            right->window_partition_column
        ) ||
        !planned_row_scalar_columns_match(left->window_order_column, right->window_order_column) ||
        !planned_row_scalar_columns_match(left->column, right->column) ||
        left->column_cast_as_integer != right->column_cast_as_integer ||
        left->column_has_source_index != right->column_has_source_index ||
        left->argument_count != right->argument_count ||
        (left->argument_count != 0U && (left->arguments == NULL || right->arguments == NULL))) {
        return false;
    }

    if (left->column_has_source_index && left->column_source_index != right->column_source_index) {
        return false;
    }

    return grouped_order_optional_strings_match(
               left->conversion_charset,
               right->conversion_charset
           ) &&
           grouped_order_optional_strings_match(
               left->conversion_collation,
               right->conversion_collation
           ) &&
           grouped_order_optional_strings_match(left->collation_name, right->collation_name) &&
           strcmp(left->column_effective_charset, right->column_effective_charset) == 0 &&
           strcmp(left->column_effective_collation, right->column_effective_collation) == 0 &&
           grouped_order_window_frame_bounds_match(
               &left->window_frame_start,
               &right->window_frame_start
           ) &&
           grouped_order_window_frame_bounds_match(
               &left->window_frame_end,
               &right->window_frame_end
           );
}

static bool planned_row_scalar_columns_match(
    const struct mylite_catalog_column_descriptor *left,
    const struct mylite_catalog_column_descriptor *right
) {
    if (left == NULL || right == NULL) {
        return left == right;
    }
    return left->column_id == right->column_id;
}

static bool grouped_order_planned_values_match(
    const struct planned_value *left,
    const struct planned_value *right
) {
    if (left->is_null != right->is_null || left->is_text != right->is_text ||
        left->is_blob != right->is_blob || left->is_real != right->is_real) {
        return false;
    }
    if (left->is_null) {
        return true;
    }
    if (left->is_text || left->is_blob) {
        return left->text_length == right->text_length &&
               (left->text_length == 0U || memcmp(left->text, right->text, left->text_length) == 0);
    }
    if (left->is_real) {
        return left->real == right->real;
    }
    return left->integer == right->integer;
}

static bool grouped_order_row_scalar_conversion_steps_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
) {
    for (size_t step_index = 0U; step_index < left->conversion_step_count; ++step_index) {
        const struct planned_row_scalar_conversion_step *left_step =
            &left->conversion_steps[step_index];
        const struct planned_row_scalar_conversion_step *right_step =
            &right->conversion_steps[step_index];

        if (left_step->kind != right_step->kind ||
            left_step->ascii_only != right_step->ascii_only ||
            !grouped_order_optional_strings_match(left_step->charset, right_step->charset) ||
            !grouped_order_optional_strings_match(left_step->collation, right_step->collation)) {
            return false;
        }
    }
    return true;
}

static bool grouped_order_window_frame_bounds_match(
    const struct planned_window_frame_bound *left,
    const struct planned_window_frame_bound *right
) {
    return left->kind == right->kind && left->value == right->value;
}

static bool grouped_order_optional_strings_match(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }
    return strcmp(left, right) == 0;
}

static int order_identifier_matches_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct mylite_sql_ast_node *alias,
    bool *out_matches
) {
    char *order_name = NULL;
    char *alias_text = NULL;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER || alias == NULL) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc == MYLITE_OK) {
        rc = copy_select_item_alias_text(database, alias, &alias_text);
    }
    if (rc == MYLITE_OK) {
        *out_matches = text_equals_ascii_case_insensitive(order_name, alias_text);
    }

    free(order_name);
    free(alias_text);
    return rc;
}

static int order_identifier_matches_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const char *alias,
    bool *out_matches
) {
    char *order_name = NULL;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER || alias == NULL) {
        return MYLITE_OK;
    }
    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc == MYLITE_OK) {
        *out_matches = text_equals_ascii_case_insensitive(order_name, alias);
    }
    free(order_name);
    return rc;
}

static int order_identifier_matches_unaliased_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_group_index
) {
    char *order_name = NULL;
    bool ambiguous_group_key = false;
    int rc = MYLITE_OK;

    *out_matches = false;
    *out_group_index = 0U;
    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc == MYLITE_OK) {
        *out_matches = find_grouped_key_by_unqualified_name(
            plan,
            order_name,
            out_group_index,
            &ambiguous_group_key
        );
    }
    if (rc == MYLITE_OK && ambiguous_group_key) {
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_ORDER, order_name);
        rc = MYLITE_ERROR;
    }

    free(order_name);
    return rc;
}

static int order_identifier_matches_unaliased_projection_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_projection_index
) {
    char *order_name = NULL;
    bool ambiguous_projection = false;
    int rc = MYLITE_OK;

    *out_matches = false;
    *out_projection_index = 0U;
    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc == MYLITE_OK) {
        *out_matches = find_grouped_projection_by_unqualified_name(
            plan,
            order_name,
            out_projection_index,
            &ambiguous_projection
        );
    }
    if (rc == MYLITE_OK && ambiguous_projection) {
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_ORDER, order_name);
        rc = MYLITE_ERROR;
    }

    free(order_name);
    return rc;
}
