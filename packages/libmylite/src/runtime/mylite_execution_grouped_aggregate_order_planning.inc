static int plan_grouped_aggregate_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *order_key = NULL;
    const struct mylite_sql_ast_node *direction = NULL;
    size_t group_order_index = 0U;
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
        set_unsupported_error(database, "GROUP BY supports only one grouped ORDER BY column");
        return MYLITE_ERROR;
    }

    order_key = child_at(order_clause, 0U);
    if (grouped_order_clause_uses_item_list(order_clause)) {
        set_unsupported_error(database, "GROUP BY supports only one grouped ORDER BY column");
        return MYLITE_ERROR;
    }
    rc = resolve_grouped_order_key(
        database,
        order_key,
        source_context,
        table_columns,
        table_column_count,
        out_plan
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!out_plan->order_uses_aggregate && !find_matching_grouped_key(
                                               out_plan,
                                               &out_plan->order.column,
                                               out_plan->order.column_source_index,
                                               &group_order_index
                                           )) {
        int dependency_rc = MYLITE_OK;

        if (!grouped_projection_column_is_allowed(
                database,
                out_plan,
                table_columns,
                table_column_count,
                &out_plan->order.column,
                out_plan->order.column_source_index,
                &dependency_rc
            )) {
            if (dependency_rc != MYLITE_OK) {
                return dependency_rc;
            }
            set_only_full_group_by_error(
                database,
                1U,
                "ORDER BY clause",
                planned_grouped_aggregate_source_resolution(
                    out_plan,
                    out_plan->order.column_source_index
                ),
                &out_plan->order.column
            );
            return MYLITE_ERROR;
        }
    }

    out_plan->order.has_order = true;
    out_plan->order.direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_plan->order.direction = PLANNED_SELECT_ORDER_DESC;
    }

    return MYLITE_OK;
}

static int resolve_grouped_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    bool matches_alias = false;
    bool matches_unaliased_column = false;
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
        out_plan->order.column = out_plan->projections[matched_index].column;
        out_plan->order.column_source_index =
            out_plan->projections[matched_index].column_source_index;
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
        out_plan->order.column = out_plan->groups[matched_index].column;
        out_plan->order.column_source_index = out_plan->groups[matched_index].column_source_index;
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
        out_plan->order.column = out_plan->projections[matched_index].column;
        out_plan->order.column_source_index =
            out_plan->projections[matched_index].column_source_index;
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
        out_plan->order.column = out_plan->groups[matched_index].column;
        out_plan->order.column_source_index = out_plan->groups[matched_index].column_source_index;
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
        out_plan->order_uses_aggregate = true;
        out_plan->order_aggregate_index = matched_index;
        return MYLITE_OK;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        order_key,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "GROUP BY supports ORDER BY only on grouped, primary-key-dependent, "
        "or selected aggregate columns",
        table_columns,
        table_column_count,
        &out_plan->order.column,
        &order_source_index
    );
    if (rc == MYLITE_OK) {
        out_plan->order.column_source_index = order_source_index;
    }

    return rc;
}

static bool grouped_order_clause_uses_item_list(const struct mylite_sql_ast_node *order_clause) {
    const struct mylite_sql_ast_node *order_key = child_at(order_clause, 0U);

    if (order_key == NULL) {
        return false;
    }
    return order_key->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST;
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
        rc = order_identifier_matches_alias(
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
        rc = order_identifier_matches_alias(
            database,
            order_key,
            plan->projections[projection_index].alias,
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

        rc = order_identifier_matches_alias(database, order_key, item->alias, &matches_alias);
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
    if (plan->aggregates[matched_index].function == PLANNED_GROUPED_AGGREGATE_AVG) {
        set_unsupported_error(
            database,
            "GROUP BY does not support ORDER BY on AVG aggregate aliases"
        );
        return MYLITE_ERROR;
    }
    if (grouped_aggregate_function_is_bitwise(plan->aggregates[matched_index].function)) {
        set_unsupported_error(
            database,
            "GROUP BY does not support ORDER BY on bitwise aggregate aliases"
        );
        return MYLITE_ERROR;
    }
    if (plan->aggregates[matched_index].function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        set_unsupported_error(
            database,
            "GROUP BY does not support ORDER BY on GROUP_CONCAT aggregate aliases"
        );
        return MYLITE_ERROR;
    }

    *out_matched = true;
    *out_aggregate_index = matched_index;
    return MYLITE_OK;
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
