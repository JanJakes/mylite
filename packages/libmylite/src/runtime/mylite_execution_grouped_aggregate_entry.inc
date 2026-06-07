static bool select_statement_has_group_by_clause(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);

    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_GROUP_BY_CLAUSE) {
            return true;
        }
        optional_clause = optional_clause->next_sibling;
    }

    return false;
}

static int plan_grouped_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_grouped_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    struct grouped_aggregate_clauses clauses = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_predicate_plan_options predicate_options = {
        .allow_exists = true,
        .allow_in_subquery = true,
        .allow_scalar_subquery_in_literal = true,
        .allow_date_format_numeric_predicate = true,
    };
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    size_t group_key_count = 0U;
    size_t first_aggregate_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_grouped_aggregate){0};
    out_plan->is_distinct =
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
    out_plan->calc_found_rows = mylite_sql_ast_node_select_calc_found_rows(statement) != 0;
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_unsupported_error(
            database,
            "GROUP BY supports selected descriptor group columns followed by aggregate results"
        );
        return MYLITE_ERROR;
    }
    if (from_clause == NULL || (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE &&
                                from_clause->kind != MYLITE_SQL_AST_FROM_JOIN &&
                                from_clause->kind != MYLITE_SQL_AST_FROM_DERIVED)) {
        set_unsupported_error(database, "GROUP BY requires descriptor-backed table sources");
        return MYLITE_ERROR;
    }

    rc = collect_grouped_aggregate_clauses(database, statement, &clauses);
    if (rc == MYLITE_OK) {
        group_key_count = grouped_aggregate_group_key_count(clauses.group_clause);
        if (group_key_count == 0U) {
            set_unsupported_error(database, "GROUP BY requires one descriptor group column");
            return MYLITE_ERROR;
        }
        if (group_key_count > grouped_aggregate_max_group_keys) {
            set_unsupported_error(
                database,
                "GROUP BY supports at most four descriptor group columns"
            );
            return MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_source(
            database,
            from_clause,
            out_plan,
            &source_context,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK && select_list_has_only_literal_projection_items(select_list)) {
        rc = plan_grouped_aggregate_group_columns(
            database,
            select_list,
            clauses.group_clause,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_grouped_projection_references(
            database,
            select_list,
            &source_context,
            table_columns,
            table_column_count
        );
    }
    if (rc == MYLITE_OK && out_plan->group_count == 0U) {
        rc = plan_grouped_aggregate_group_columns(
            database,
            select_list,
            clauses.group_clause,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_projection_columns(
            database,
            select_list,
            &source_context,
            table_columns,
            table_column_count,
            out_plan,
            &first_aggregate_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_functions(
            database,
            select_list,
            first_aggregate_index,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate_with_options(
            database,
            clauses.where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &predicate_options,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_having(
            database,
            clauses.having_clause,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_order(
            database,
            clauses.order_clause,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses.limit_clause, &out_plan->limit);
    }

    free(table_columns);
    return rc;
}

static void planned_grouped_aggregate_deinit(struct planned_grouped_aggregate *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t source_index = 0U; source_index < plan->source_count; ++source_index) {
        planned_select_source_deinit(&plan->sources[source_index]);
    }
    for (size_t join_index = 0U; join_index < plan->join_count; ++join_index) {
        planned_select_join_condition_deinit(&plan->join_conditions[join_index]);
    }
    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        planned_row_scalar_expression_deinit(
            &plan->aggregates[aggregate_index].aggregate_row_scalar_expression
        );
        planned_select_order_deinit(&plan->aggregates[aggregate_index].aggregate_order);
        planned_value_deinit(&plan->aggregates[aggregate_index].separator);
    }
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        planned_row_scalar_expression_deinit(&plan->groups[group_index].row_scalar_expression);
    }
    for (size_t projection_index = 0U; projection_index < plan->projection_count;
         ++projection_index) {
        planned_row_scalar_expression_deinit(
            &plan->projections[projection_index].row_scalar_expression
        );
    }
    free(plan->aggregates);
    free(plan->projections);
    free(plan->groups);
    free(plan->sources);
    free(plan->join_kinds);
    free(plan->join_conditions);
    free(plan->order_item_uses_aggregate);
    free(plan->order_item_aggregate_indexes);
    planned_select_predicate_deinit(&plan->predicate);
    planned_row_scalar_expression_deinit(plan->having.row_scalar_expression);
    free(plan->having.row_scalar_expression);
    planned_value_deinit(&plan->having.value);
    planned_select_order_deinit(&plan->order);
    *plan = (struct planned_grouped_aggregate){0};
}

static int collect_grouped_aggregate_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct grouped_aggregate_clauses *out_clauses
) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);

    *out_clauses = (struct grouped_aggregate_clauses){0};
    while (optional_clause != NULL) {
        switch (optional_clause->kind) {
        case MYLITE_SQL_AST_WHERE_CLAUSE:
            out_clauses->where_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
            out_clauses->group_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_HAVING_CLAUSE:
            out_clauses->having_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
            out_clauses->order_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_LIMIT_CLAUSE:
            out_clauses->limit_clause = optional_clause;
            break;
        default:
            set_unsupported_error(
                database,
                "GROUP BY supports only WHERE, GROUP BY, HAVING, ORDER BY, and LIMIT"
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (out_clauses->group_clause == NULL) {
        set_unsupported_error(database, "GROUP BY requires one descriptor group column");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
