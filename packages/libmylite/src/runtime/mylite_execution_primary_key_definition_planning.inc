
static int apply_create_table_inline_primary_key(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index,
    const struct mylite_sql_ast_node *column_node
) {
    if (column_node == NULL ||
        child_with_kind(column_node, MYLITE_SQL_AST_INLINE_PRIMARY_KEY) == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (plan->has_primary_key) {
        set_multiple_primary_key_error(database);
        return MYLITE_ERROR;
    }

    return mark_primary_key_column(
        database,
        plan,
        column_index,
        MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC
    );
}

static int apply_create_table_primary_key_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *primary_key
) {
    struct primary_key_definition_nodes nodes = {0};
    const struct mylite_sql_ast_node *part_list = NULL;
    const struct mylite_sql_ast_node *part = NULL;
    int rc = MYLITE_OK;

    if (plan->has_primary_key) {
        set_multiple_primary_key_error(database);
        return MYLITE_ERROR;
    }

    rc = extract_primary_key_definition_nodes(database, primary_key, &nodes);
    if (rc != MYLITE_OK) {
        return rc;
    }
    part_list = nodes.part_list;
    rc = apply_index_options_to_create_primary_key_plan(database, nodes.options, plan);
    if (rc == MYLITE_OK) {
        rc = validate_hash_index_key_part_order(
            database,
            plan->primary_key_uses_hash_index_type,
            part_list
        );
    }

    part = child_at(part_list, 0U);
    while (rc == MYLITE_OK && part != NULL) {
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        size_t column_index = 0U;

        const struct mylite_sql_ast_node *column_node = secondary_index_part_column_node(part);
        enum mylite_catalog_index_sort_direction sort_direction = index_part_sort_direction(part);

        if (secondary_index_part_prefix_node(part) != NULL) {
            set_unsupported_error(database, "PRIMARY KEY prefix key parts are not yet supported");
            return MYLITE_ERROR;
        }
        if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_unsupported_error(
                database,
                index_part_unsupported_message(
                    part,
                    "PRIMARY KEY supports only unqualified key columns"
                )
            );
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = find_planned_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &column_index
        );
        if (rc != MYLITE_OK) {
            set_key_column_missing_error(database, column_name);
            return MYLITE_ERROR;
        }
        if (planned_primary_key_contains_column(plan, column_index)) {
            set_duplicate_column_error(database, plan->columns[column_index].name);
            return MYLITE_ERROR;
        }
        if (planned_column_is_spatial(&plan->columns[column_index])) {
            set_spatial_unique_error(database);
            return MYLITE_ERROR;
        }
        if (plan->columns[column_index].nullability == MYLITE_SQL_AST_NULLABILITY_NULL ||
            (plan->columns[column_index].default_node != NULL &&
             plan->columns[column_index].default_node->kind ==
                 MYLITE_SQL_AST_COLUMN_DEFAULT_NULL)) {
            set_primary_key_part_null_error(database);
            return MYLITE_ERROR;
        }
        rc = mark_primary_key_column(database, plan, column_index, sort_direction);
        part = part->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_table_primary_key_key_length(database, plan);
    }

    return rc;
}

static int extract_primary_key_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *primary_key,
    struct primary_key_definition_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct primary_key_definition_nodes){0};

    if (primary_key == NULL || primary_key->kind != MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    child = child_at(primary_key, 0U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST && out_nodes->part_list == NULL) {
            out_nodes->part_list = child;
        } else if (
            child->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION &&
            out_nodes->options.index_type_node == NULL
        ) {
            out_nodes->options.index_type_node = child;
        } else if (
            child->kind == MYLITE_SQL_AST_INDEX_OPTION_LIST &&
            out_nodes->options.option_list_node == NULL
        ) {
            out_nodes->options.option_list_node = child;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        child = child->next_sibling;
    }
    if (out_nodes->part_list == NULL ||
        out_nodes->part_list->kind != MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
