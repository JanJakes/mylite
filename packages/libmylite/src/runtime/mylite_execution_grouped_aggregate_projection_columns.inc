static int plan_grouped_aggregate_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    size_t *out_first_aggregate_index
) {
    const struct mylite_sql_ast_node *item = NULL;
    size_t item_index = 0U;
    int rc = MYLITE_OK;

    *out_first_aggregate_index = 0U;
    if (select_list_is_wildcard(select_list)) {
        rc = plan_grouped_aggregate_wildcard_projection_columns(
            database,
            1U,
            source_context,
            table_columns,
            table_column_count,
            out_plan
        );
        if (rc == MYLITE_OK) {
            *out_first_aggregate_index = 1U;
        }
        return rc;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        const struct mylite_sql_ast_node *column_node = NULL;
        const struct mylite_sql_ast_node *qualified_wildcard = NULL;
        struct mylite_catalog_column_descriptor column = {0};
        size_t source_index = 0U;
        size_t expression_index = out_plan->projection_count + 1U;

        if (grouped_aggregate_function_from_expression(
                unwrap_parenthesized_expression(expression)
            ) != PLANNED_GROUPED_AGGREGATE_NONE) {
            *out_first_aggregate_index = item_index;
            return MYLITE_OK;
        }
        if (expression != NULL && expression->kind == MYLITE_SQL_AST_WILDCARD &&
            child_at(item, 1U) == NULL) {
            rc = plan_grouped_aggregate_wildcard_projection_columns(
                database,
                expression_index,
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else if (select_item_qualified_wildcard(item, &qualified_wildcard)) {
            rc = plan_grouped_aggregate_qualified_wildcard_projection_columns(
                database,
                qualified_wildcard,
                expression_index,
                source_context,
                table_columns,
                table_column_count,
                out_plan
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
            if (rc == MYLITE_OK) {
                rc = validate_grouped_projection_column(
                    database,
                    out_plan,
                    table_columns,
                    table_column_count,
                    &column,
                    source_index,
                    expression_index
                );
            }
            if (rc == MYLITE_OK) {
                rc = append_grouped_projection_column(
                    database,
                    out_plan,
                    &column,
                    source_index,
                    expression,
                    child_at(item, 1U)
                );
            }
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        ++item_index;
        item = item->next_sibling;
    }

    *out_first_aggregate_index = item_index;
    return MYLITE_OK;
}

static int append_grouped_projection_column(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    struct planned_grouped_projection *projections = NULL;
    size_t required_count = plan->projection_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*projections)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    projections = realloc(plan->projections, required_count * sizeof(*projections));
    if (projections == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->projections = projections;
    plan->projections[plan->projection_count] = (struct planned_grouped_projection){
        .expression = expression,
        .alias = alias,
        .column = *column,
        .column_source_index = source_index,
    };
    plan->projection_count = required_count;
    return MYLITE_OK;
}

static int plan_grouped_aggregate_wildcard_projection_columns(
    struct mylite_db *database,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    (void)select_expression_index;

    if (select_source_context_is_joined(source_context)) {
        for (size_t source_index = 0U; source_index < source_context->source_count;
             ++source_index) {
            const struct planned_select_source *source = &source_context->sources[source_index];
            int rc = append_visible_grouped_projection_columns_from_source(
                database,
                source->columns,
                source->column_count,
                source_index,
                out_plan
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        return MYLITE_OK;
    }

    return append_visible_grouped_projection_columns_from_source(
        database,
        table_columns,
        table_column_count,
        0U,
        out_plan
    );
}

static int plan_grouped_aggregate_qualified_wildcard_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_catalog_column_descriptor *columns = table_columns;
    size_t column_count = table_column_count;
    size_t source_index = 0U;
    size_t part_count = 0U;
    bool matched = false;
    int rc = collect_qualified_wildcard_parts(database, qualified_wildcard, parts, &part_count);

    (void)select_expression_index;
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (select_source_context_is_joined(source_context)) {
        for (size_t index = 0U; index < source_context->source_count; ++index) {
            const struct planned_select_source *source = &source_context->sources[index];

            if (!joined_source_matches_qualified_wildcard_parts(
                    source,
                    (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
                    part_count
                )) {
                continue;
            }
            columns = source->columns;
            column_count = source->column_count;
            source_index = index;
            matched = true;
            break;
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

    return append_visible_grouped_projection_columns_from_source(
        database,
        columns,
        column_count,
        source_index,
        out_plan
    );
}

static int append_visible_grouped_projection_columns_from_source(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t source_index,
    struct planned_grouped_aggregate *out_plan
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        size_t expression_index = out_plan->projection_count + 1U;
        int rc = MYLITE_OK;

        if (!columns[column_index].is_visible) {
            continue;
        }
        rc = validate_grouped_projection_column(
            database,
            out_plan,
            columns,
            column_count,
            &columns[column_index],
            source_index,
            expression_index
        );
        if (rc == MYLITE_OK) {
            rc = append_grouped_projection_column(
                database,
                out_plan,
                &columns[column_index],
                source_index,
                NULL,
                NULL
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_grouped_projection_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t select_expression_index
) {
    int rc = MYLITE_OK;

    if (grouped_projection_column_is_allowed(
            database,
            plan,
            table_columns,
            table_column_count,
            column,
            source_index,
            &rc
        )) {
        return MYLITE_OK;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    set_only_full_group_by_error(
        database,
        select_expression_index,
        "SELECT list",
        planned_grouped_aggregate_source_resolution(plan, source_index),
        column
    );
    return MYLITE_ERROR;
}

static int grouped_source_primary_key_is_covered(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t source_index,
    bool *out_covered
) {
    const struct mylite_catalog_column_descriptor *source_columns = NULL;
    size_t source_column_count = 0U;
    int64_t table_id = 0;
    struct primary_key_info primary_key = mylite_execution_primary_key_info_init();
    int rc = MYLITE_OK;

    *out_covered = false;
    if (!grouped_source_columns(
            plan,
            source_index,
            table_columns,
            table_column_count,
            &source_columns,
            &source_column_count,
            &table_id
        )) {
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_primary_key_info(
        database,
        table_id,
        source_columns,
        source_column_count,
        &primary_key
    );
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        *out_covered = true;
        for (size_t part_index = 0U; part_index < primary_key.part_count; ++part_index) {
            size_t group_index = 0U;

            if (!find_matching_grouped_key(
                    plan,
                    &primary_key.parts[part_index].column,
                    source_index,
                    &group_index
                )) {
                *out_covered = false;
                break;
            }
        }
    }

    mylite_execution_primary_key_info_deinit(&primary_key);
    return rc;
}

static bool grouped_source_columns(
    const struct planned_grouped_aggregate *plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count,
    int64_t *out_table_id
) {
    if (plan->source_count == 0U) {
        if (source_index != 0U) {
            return false;
        }
        *out_columns = table_columns;
        *out_column_count = table_column_count;
        *out_table_id = plan->table.table_id;
        return true;
    }
    if (source_index >= plan->source_count) {
        return false;
    }

    *out_columns = plan->sources[source_index].columns;
    *out_column_count = plan->sources[source_index].column_count;
    *out_table_id = plan->sources[source_index].table.table_id;
    return true;
}

static bool grouped_projection_column_is_allowed(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    int *out_rc
) {
    size_t group_index = 0U;
    bool primary_key_covered = false;

    *out_rc = MYLITE_OK;
    if (find_matching_grouped_key(plan, column, source_index, &group_index)) {
        return true;
    }

    *out_rc = grouped_source_primary_key_is_covered(
        database,
        plan,
        table_columns,
        table_column_count,
        source_index,
        &primary_key_covered
    );
    if (*out_rc != MYLITE_OK) {
        return false;
    }
    return primary_key_covered;
}
