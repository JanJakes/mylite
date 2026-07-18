
static int apply_create_table_inline_unique_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        set_runtime_error(database, "invalid unique-index table plan");
        return MYLITE_ERROR;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        struct planned_column *column = &plan->columns[column_index];
        char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        if (!column->is_unique_key) {
            continue;
        }

        rc = validate_unique_index_column(database, column);
        if (rc == MYLITE_OK) {
            rc = plan_secondary_index_name(
                database,
                plan,
                NULL,
                column->name,
                index_name,
                sizeof(index_name)
            );
        }
        if (rc == MYLITE_OK) {
            rc =
                reserve_planned_secondary_indexes(database, plan, plan->secondary_index_count + 1U);
        }
        if (rc == MYLITE_OK) {
            struct planned_secondary_index *index =
                &plan->secondary_indexes[plan->secondary_index_count];

            *index = (struct planned_secondary_index){0};
            snprintf(index->name, sizeof(index->name), "%s", index_name);
            index->kind = MYLITE_CATALOG_INDEX_KIND_SECONDARY;
            index->is_unique = true;
            index->is_visible = true;
            rc = reserve_planned_secondary_index_parts(database, index, 1U);
            if (rc == MYLITE_OK) {
                index->parts[0] = (struct planned_secondary_index_part){
                    .column_index = column_index,
                    .has_prefix_length = false,
                    .prefix_length = 0,
                    .sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC,
                };
                index->part_count = 1U;
            }
        }
        if (rc == MYLITE_OK) {
            ++plan->secondary_index_count;
        }
    }

    return rc;
}

static int apply_create_table_secondary_index_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *item = NULL;
    int rc = MYLITE_OK;

    if (item_list == NULL || plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item = child_at(item_list, 0U);
    while (rc == MYLITE_OK && item != NULL) {
        if (item->kind == MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION) {
            rc = apply_create_table_secondary_index_definition(
                database,
                plan,
                item,
                MYLITE_CATALOG_INDEX_KIND_SECONDARY,
                false
            );
        } else if (item->kind == MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION) {
            rc = apply_create_table_secondary_index_definition(
                database,
                plan,
                item,
                MYLITE_CATALOG_INDEX_KIND_SECONDARY,
                true
            );
        } else if (item->kind == MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION) {
            rc = apply_create_table_secondary_index_definition(
                database,
                plan,
                item,
                MYLITE_CATALOG_INDEX_KIND_FULLTEXT,
                false
            );
        } else if (item->kind == MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION) {
            rc = apply_create_table_secondary_index_definition(
                database,
                plan,
                item,
                MYLITE_CATALOG_INDEX_KIND_SPATIAL,
                false
            );
        }
        item = item->next_sibling;
    }

    return rc;
}

static int apply_create_table_secondary_index_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *secondary_index,
    enum mylite_catalog_index_kind index_kind,
    bool is_unique
) {
    struct create_table_secondary_index_definition_nodes nodes = {0};
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct planned_secondary_index planned_index = {0};
    enum mylite_catalog_index_kind effective_index_kind = index_kind;
    struct create_table_secondary_index_first_part first_part = {0};
    int rc = MYLITE_OK;

    rc = extract_create_table_secondary_index_definition_nodes(database, secondary_index, &nodes);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = resolve_create_table_secondary_index_first_column(
        database,
        plan,
        &nodes,
        index_kind,
        is_unique,
        &first_part
    );
    if (rc == MYLITE_OK) {
        rc = validate_create_table_secondary_index_kind(
            database,
            plan,
            nodes.options,
            index_kind,
            is_unique,
            first_part,
            &effective_index_kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_secondary_index_name(
            database,
            plan,
            nodes.index_name_node,
            plan->columns[first_part.column_index].name,
            index_name,
            sizeof(index_name)
        );
    }
    if (rc == MYLITE_OK) {
        planned_index.kind = effective_index_kind;
        planned_index.is_unique = is_unique;
        planned_index.is_visible = true;
        snprintf(planned_index.name, sizeof(planned_index.name), "%s", index_name);
    }
    if (rc == MYLITE_OK &&
        (index_kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
         index_kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) &&
        index_option_nodes_have_type_option(nodes.options)) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = apply_index_options_to_secondary_plan(database, nodes.options, &planned_index);
    }
    if (rc == MYLITE_OK) {
        rc = validate_hash_index_key_part_order(
            database,
            planned_index.uses_hash_index_type,
            nodes.part_list
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < first_part.part_count; ++index) {
        rc = append_planned_secondary_index_part(
            database,
            plan,
            &planned_index,
            child_at(nodes.part_list, index)
        );
    }
    if (rc == MYLITE_OK && is_unique) {
        rc = validate_planned_unique_index_part_list(database, &planned_index);
    }
    if (rc == MYLITE_OK && !planned_secondary_index_is_fulltext(&planned_index) &&
        !planned_secondary_index_is_spatial(&planned_index)) {
        rc = validate_planned_secondary_index_key_length(database, plan, &planned_index);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_secondary_indexes(database, plan, plan->secondary_index_count + 1U);
    }
    if (rc == MYLITE_OK) {
        plan->secondary_indexes[plan->secondary_index_count] = planned_index;
        planned_index = (struct planned_secondary_index){0};
        ++plan->secondary_index_count;
    }
    free(planned_index.parts);

    return rc;
}

static int resolve_create_table_secondary_index_first_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct create_table_secondary_index_definition_nodes *nodes,
    enum mylite_catalog_index_kind index_kind,
    bool is_unique,
    struct create_table_secondary_index_first_part *out_first_part
) {
    const struct mylite_sql_ast_node *first_part = NULL;
    const struct mylite_sql_ast_node *first_column = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct planned_secondary_index message_index = {0};
    int rc = MYLITE_OK;

    *out_first_part = (struct create_table_secondary_index_first_part){0};
    out_first_part->part_count = mylite_sql_ast_node_child_count(nodes->part_list);
    if (out_first_part->part_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_part = child_at(nodes->part_list, 0U);
    first_column = secondary_index_part_column_node(first_part);
    if (first_column == NULL || first_column->kind != MYLITE_SQL_AST_IDENTIFIER) {
        message_index.kind = index_kind;
        message_index.is_unique = is_unique;
        set_unsupported_error(
            database,
            index_part_unsupported_message(
                first_part,
                planned_secondary_index_unqualified_column_message(&message_index)
            )
        );
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(first_column, column_name, sizeof(column_name), database);
    if (rc == MYLITE_OK) {
        rc = find_planned_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &out_first_part->column_index
        );
    }
    if (rc != MYLITE_OK) {
        set_key_column_missing_error(database, column_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_secondary_index_kind(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    struct index_option_nodes options,
    enum mylite_catalog_index_kind index_kind,
    bool is_unique,
    struct create_table_secondary_index_first_part first_part,
    enum mylite_catalog_index_kind *out_effective_index_kind
) {
    enum planned_index_type_option requested_type = PLANNED_INDEX_TYPE_DEFAULT;
    bool first_part_is_spatial = false;
    int rc = MYLITE_OK;

    *out_effective_index_kind = index_kind;
    first_part_is_spatial = planned_column_is_spatial(&plan->columns[first_part.column_index]);

    if (index_kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        rc = collect_index_type_option(database, options, &requested_type);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (first_part_is_spatial && requested_type == PLANNED_INDEX_TYPE_BTREE) {
            set_spatial_index_type_not_supported_error(database, "BTREE");
            return MYLITE_ERROR;
        }
        if (first_part_is_spatial && requested_type == PLANNED_INDEX_TYPE_HASH) {
            set_spatial_index_type_not_supported_error(database, "HASH");
            return MYLITE_ERROR;
        }
        if (!first_part_is_spatial && requested_type == PLANNED_INDEX_TYPE_RTREE) {
            set_spatial_index_non_geometric_error(database);
            return MYLITE_ERROR;
        }
        if (is_unique && (first_part_is_spatial || requested_type == PLANNED_INDEX_TYPE_RTREE)) {
            set_spatial_unique_error(database);
            return MYLITE_ERROR;
        }
        if (!is_unique && (first_part_is_spatial || requested_type == PLANNED_INDEX_TYPE_RTREE)) {
            *out_effective_index_kind = MYLITE_CATALOG_INDEX_KIND_SPATIAL;
        }
    }
    if (*out_effective_index_kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL &&
        first_part.part_count > 1U) {
        set_spatial_too_many_key_parts_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int extract_create_table_secondary_index_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *secondary_index,
    struct create_table_secondary_index_definition_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct create_table_secondary_index_definition_nodes){0};

    if (secondary_index == NULL ||
        (secondary_index->kind != MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION &&
         secondary_index->kind != MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION &&
         secondary_index->kind != MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION &&
         secondary_index->kind != MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    child = child_at(secondary_index, 0U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER && out_nodes->index_name_node == NULL) {
            out_nodes->index_name_node = child;
        } else if (child->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION &&
                   out_nodes->options.index_type_node == NULL) {
            out_nodes->options.index_type_node = child;
        } else if (child->kind == MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST &&
                   out_nodes->part_list == NULL) {
            out_nodes->part_list = child;
        } else if (child->kind == MYLITE_SQL_AST_INDEX_OPTION_LIST &&
                   out_nodes->options.option_list_node == NULL) {
            out_nodes->options.option_list_node = child;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        child = child->next_sibling;
    }
    if (out_nodes->part_list == NULL ||
        out_nodes->part_list->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
