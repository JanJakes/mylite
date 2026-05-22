static int plan_grouped_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
) {
    int rc = MYLITE_OK;

    *out_columns = NULL;
    *out_column_count = 0U;
    if (from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        return plan_grouped_aggregate_join_source(
            database,
            from_clause,
            out_plan,
            out_source_context
        );
    }

    rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );

    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            from_clause,
            &out_plan->source,
            out_source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, out_columns, out_column_count);
    }

    return rc;
}

static int plan_grouped_aggregate_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context
) {
    const struct mylite_sql_ast_node *join_condition = child_at(from_clause, 2U);
    int rc = MYLITE_OK;

    out_plan->join_kind = mylite_sql_ast_node_join_kind(from_clause);
    if ((out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER ||
         out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) &&
        join_condition == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        set_unsupported_error(database, "GROUP BY does not support RIGHT JOIN");
        return MYLITE_ERROR;
    }
    out_plan->sources = calloc(2U, sizeof(*out_plan->sources));
    if (out_plan->sources == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->source_count = 2U;

    rc = plan_joined_select_source(database, child_at(from_clause, 0U), &out_plan->sources[0]);
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_source(database, child_at(from_clause, 1U), &out_plan->sources[1]);
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            out_plan->sources,
            out_plan->source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            out_plan->sources,
            out_plan->source_count,
            out_source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_condition(
            database,
            join_condition,
            out_plan->join_kind,
            out_source_context,
            &out_plan->join_condition
        );
    }

    return rc;
}

static int validate_grouped_projection_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    const struct mylite_sql_ast_node *item = NULL;
    int rc = MYLITE_OK;

    if (select_list_is_wildcard(select_list)) {
        return MYLITE_OK;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        const struct mylite_sql_ast_node *column_node = NULL;
        const struct mylite_sql_ast_node *qualified_wildcard = NULL;
        struct mylite_catalog_column_descriptor column = {0};
        size_t source_index = 0U;

        if (grouped_aggregate_function_from_expression(
                unwrap_parenthesized_expression(expression)
            ) != PLANNED_GROUPED_AGGREGATE_NONE) {
            return MYLITE_OK;
        }
        if (expression != NULL && expression->kind == MYLITE_SQL_AST_WILDCARD &&
            child_at(item, 1U) == NULL) {
            item = item->next_sibling;
            continue;
        }
        if (select_item_qualified_wildcard(item, &qualified_wildcard)) {
            rc = validate_grouped_qualified_wildcard_reference(
                database,
                qualified_wildcard,
                source_context
            );
        } else {
            rc = select_item_column_reference(item, &column_node);
            if (rc != MYLITE_OK) {
                set_unsupported_error(
                    database,
                    "GROUP BY supports selected descriptor group columns followed by aggregate "
                    "results"
                );
                return MYLITE_ERROR;
            }
            rc = resolve_descriptor_column_reference_with_source_index(
                database,
                column_node,
                source_context,
                COLUMN_REFERENCE_FIELD,
                "GROUP BY supports selected descriptor group columns followed by aggregate "
                "results",
                table_columns,
                table_column_count,
                &column,
                &source_index
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int validate_grouped_qualified_wildcard_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    bool matched = false;
    int rc = collect_qualified_wildcard_parts(database, qualified_wildcard, parts, &part_count);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (select_source_context_is_joined(source_context)) {
        for (size_t index = 0U; index < source_context->source_count; ++index) {
            if (joined_source_matches_qualified_wildcard_parts(
                    &source_context->sources[index],
                    (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
                    part_count
                )) {
                matched = true;
                break;
            }
        }
    } else {
        matched = select_source_matches_qualified_wildcard_parts(
            source_context,
            (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
            part_count
        );
    }

    if (!matched) {
        return set_unknown_qualified_wildcard_table_error(database, parts, part_count);
    }
    return MYLITE_OK;
}
