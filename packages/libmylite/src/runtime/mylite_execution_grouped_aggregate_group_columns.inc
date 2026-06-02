static int plan_grouped_aggregate_group_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    size_t group_count = grouped_aggregate_group_key_count(group_clause);
    int rc = MYLITE_OK;

    if (group_count == 0U) {
        set_unsupported_error(database, "GROUP BY requires one descriptor group column");
        return MYLITE_ERROR;
    }

    out_plan->groups = calloc(group_count, sizeof(*out_plan->groups));
    if (out_plan->groups == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->group_count = group_count;

    for (size_t group_index = 0U; group_index < group_count; ++group_index) {
        rc = resolve_grouped_aggregate_group_column(
            database,
            select_list,
            child_at(group_clause, group_index),
            source_context,
            table_columns,
            table_column_count,
            &out_plan->groups[group_index]
        );
        if (rc != MYLITE_OK) {
            break;
        }
    }

    return rc;
}

static int resolve_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_key *out_key
) {
    size_t source_column_match_count = 0U;
    bool matched_alias = false;
    int rc = count_grouped_source_columns_by_unqualified_name(
        database,
        group_key,
        source_context,
        table_columns,
        table_column_count,
        &source_column_match_count
    );

    *out_key = (struct planned_grouped_key){0};
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (source_column_match_count == 0U) {
        const struct grouped_alias_group_resolution alias_resolution = {
            .select_list = select_list,
            .group_key = group_key,
            .source_context = source_context,
            .table_columns = table_columns,
            .table_column_count = table_column_count,
        };

        rc = resolve_grouped_select_alias_group_column(
            database,
            &alias_resolution,
            out_key,
            &matched_alias
        );
        if (rc != MYLITE_OK || matched_alias) {
            return rc;
        }
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        group_key,
        source_context,
        COLUMN_REFERENCE_GROUP,
        "GROUP BY supports only descriptor group columns",
        table_columns,
        table_column_count,
        &out_key->column,
        &out_key->column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = validate_grouped_aggregate_group_column(database, &out_key->column);
    }
    if (rc == MYLITE_OK) {
        out_key->expression = group_key;
        out_key->alias = NULL;
    }

    return rc;
}

static int resolve_grouped_select_alias_group_column(
    struct mylite_db *database,
    const struct grouped_alias_group_resolution *resolution,
    struct planned_grouped_key *out_key,
    bool *out_matched
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *matched_item = NULL;
    const struct mylite_sql_ast_node *matched_expression = NULL;
    const struct mylite_sql_ast_node *matched_alias = NULL;
    bool matches_alias = false;
    int rc = MYLITE_OK;

    *out_matched = false;
    if (resolution->group_key == NULL || resolution->group_key->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    item = child_at(resolution->select_list, 0U);
    while (item != NULL) {
        rc = order_identifier_matches_alias(
            database,
            resolution->group_key,
            child_at(item, 1U),
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (matches_alias) {
            if (matched_item != NULL) {
                char *alias_text = NULL;

                rc = copy_select_item_identifier_alias_text(
                    database,
                    resolution->group_key,
                    &alias_text
                );
                if (rc == MYLITE_OK) {
                    set_ambiguous_column_reference_error(
                        database,
                        COLUMN_REFERENCE_GROUP,
                        alias_text
                    );
                    rc = MYLITE_ERROR;
                }
                free(alias_text);
                return rc;
            }
            matched_item = item;
        }
        item = item->next_sibling;
    }
    if (matched_item == NULL) {
        return MYLITE_OK;
    }

    matched_expression = child_at(matched_item, 0U);
    matched_alias = child_at(matched_item, 1U);
    rc = select_item_column_reference(matched_item, &matched_expression);
    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "GROUP BY supports selected descriptor-column aliases only"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        matched_expression,
        resolution->source_context,
        COLUMN_REFERENCE_GROUP,
        "GROUP BY supports selected descriptor-column aliases only",
        resolution->table_columns,
        resolution->table_column_count,
        &out_key->column,
        &out_key->column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = validate_grouped_aggregate_group_column(database, &out_key->column);
    }
    if (rc == MYLITE_OK) {
        out_key->expression = matched_expression;
        out_key->alias = matched_alias;
        *out_matched = true;
    }

    return rc;
}

static int count_grouped_source_columns_by_unqualified_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t *out_match_count
) {
    char group_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_match_count = 0U;
    if (group_key == NULL || group_key->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(group_key, group_name, sizeof(group_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (select_source_context_is_joined(source_context)) {
        for (size_t source_index = 0U; source_index < source_context->source_count;
             ++source_index) {
            const struct planned_select_source *source = &source_context->sources[source_index];

            if (find_column_index(
                    source->columns,
                    source->column_count,
                    group_name,
                    &column_index
                ) == MYLITE_OK) {
                ++(*out_match_count);
            }
        }
        return MYLITE_OK;
    }
    if (find_column_index(table_columns, table_column_count, group_name, &column_index) ==
        MYLITE_OK) {
        *out_match_count = 1U;
    }

    return MYLITE_OK;
}

static size_t grouped_aggregate_group_key_count(const struct mylite_sql_ast_node *group_clause) {
    if (group_clause == NULL) {
        return 0U;
    }
    return mylite_sql_ast_node_child_count(group_clause);
}

static int validate_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }
    return integer_range_for_column(
        database,
        column,
        "GROUP BY supports only integer and nonbinary string descriptor group columns",
        &range
    );
}
