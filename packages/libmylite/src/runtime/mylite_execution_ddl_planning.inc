static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table *out_plan
) {
    const struct mylite_sql_ast_node *item_list = child_at(statement, 1U);
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_table){0};
    out_plan->auto_increment_next = 1;
    out_plan->pack_keys = -1;
    out_plan->stats_persistent = -1;
    out_plan->stats_auto_recalc = -1;
    memcpy(
        out_plan->default_charset,
        MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_CHARSET)
    );
    memcpy(
        out_plan->default_collation,
        MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_COLLATION)
    );
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    memcpy(
        out_plan->default_charset,
        out_plan->target.schema.default_charset,
        strlen(out_plan->target.schema.default_charset) + 1U
    );
    memcpy(
        out_plan->default_collation,
        out_plan->target.schema.default_collation,
        strlen(out_plan->target.schema.default_collation) + 1U
    );
    if (mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        return MYLITE_ERROR;
    }
    rc = validate_create_table_options(
        database,
        create_table_options_node(statement),
        out_plan->target.table_name
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = apply_create_table_charset_collation_options(
        database,
        create_table_options_node(statement),
        out_plan
    );
    if (rc == MYLITE_OK) {
        rc = apply_create_table_comment_option(
            database,
            create_table_options_node(statement),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_storage_statistics_options(
            database,
            create_table_options_node(statement),
            out_plan
        );
    }
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }

    rc = plan_create_table_items(database, item_list, out_plan);
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }
    rc = apply_create_table_auto_increment_option(
        database,
        create_table_options_node(statement),
        out_plan
    );
    if (rc == MYLITE_OK) {
        rc = validate_create_table_auto_increment_columns(database, out_plan, true);
    }
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_create_table_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *primary_key = NULL;
    size_t column_index = 0U;
    int rc = count_create_table_column_items(database, item_list, &out_plan->column_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_plan->column_count > SIZE_MAX / sizeof(*out_plan->columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->columns = calloc(out_plan->column_count, sizeof(*out_plan->columns));
    if (out_plan->columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    item = child_at(item_list, 0U);
    while (rc == MYLITE_OK && item != NULL) {
        rc = plan_create_table_item(database, item, out_plan, &column_index, &primary_key);
        item = item->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_generated_columns(database, out_plan);
    }
    if (rc == MYLITE_OK && primary_key != NULL) {
        rc = apply_create_table_primary_key_definition(database, out_plan, primary_key);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_inline_unique_indexes(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_secondary_index_definitions(database, item_list, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_foreign_key_definitions(database, item_list, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_check_constraint_definitions(database, item_list, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
    }

    return rc;
}

static int plan_create_table_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    struct planned_create_table *out_plan,
    size_t *column_index,
    const struct mylite_sql_ast_node **primary_key
) {
    int rc = MYLITE_OK;

    if (item->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
        size_t current_column_index = *column_index;

        if (current_column_index >= out_plan->column_count) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = plan_column(database, item, &out_plan->columns[current_column_index]);
        if (rc == MYLITE_OK) {
            rc = normalize_text_length_argument_for_default_charset(
                database,
                &out_plan->columns[current_column_index],
                out_plan->default_charset
            );
        }
        if (rc == MYLITE_OK) {
            rc = apply_create_table_default_binary_charset_to_column(
                database,
                out_plan,
                item,
                &out_plan->columns[current_column_index]
            );
        }
        if (rc == MYLITE_OK) {
            rc = validate_column_default(
                database,
                out_plan->columns[current_column_index].default_node,
                &out_plan->columns[current_column_index]
            );
        }
        if (rc == MYLITE_OK && child_with_kind(item, MYLITE_SQL_AST_INLINE_PRIMARY_KEY) != NULL) {
            rc = apply_create_table_inline_primary_key(
                database,
                out_plan,
                current_column_index,
                item
            );
        }
        if (rc == MYLITE_OK && child_with_kind(item, MYLITE_SQL_AST_INLINE_UNIQUE_KEY) != NULL) {
            out_plan->columns[current_column_index].is_unique_key = true;
        }
        *column_index = current_column_index + 1U;
        return rc;
    }

    if (item->kind == MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION) {
        if (*primary_key != NULL) {
            set_multiple_primary_key_error(database);
            return MYLITE_ERROR;
        }
        *primary_key = item;
        return MYLITE_OK;
    }

    if (item->kind == MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION ||
        item->kind == MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_create_table_default_binary_charset_to_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
) {
    if (!planned_create_table_default_charset_is_binary(plan)) {
        return MYLITE_OK;
    }
    if (create_table_column_has_explicit_charset_or_collation(column_node)) {
        return MYLITE_OK;
    }
    if (planned_column_is_national_char_or_varchar(column)) {
        return MYLITE_OK;
    }
    if (!planned_column_allows_charset_collation_attributes(column)) {
        return MYLITE_OK;
    }

    return apply_default_binary_charset_to_column(database, plan->default_charset, column);
}

static int normalize_text_length_argument_for_default_charset(
    struct mylite_db *database,
    struct planned_column *column,
    const char *default_charset
) {
    const char *effective_charset = default_charset;
    const char *logical_type = NULL;

    if (column == NULL || !column->has_text_length_argument) {
        return MYLITE_OK;
    }
    if (!planned_column_is_text_family(column)) {
        return MYLITE_OK;
    }
    if (column->character_set_name[0] != '\0') {
        effective_charset = column->character_set_name;
    }
    if (effective_charset == NULL || effective_charset[0] == '\0') {
        effective_charset = MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
    }

    logical_type = text_family_logical_type_for_length(
        column->text_length_argument,
        max_bytes_per_character_for_charset(effective_charset)
    );
    return assign_text_family_descriptor_type(database, column, logical_type);
}

static int apply_default_binary_charset_to_column(
    struct mylite_db *database,
    const char *default_charset,
    struct planned_column *column
) {
    int rc = MYLITE_OK;

    if (!charset_name_is_binary(default_charset)) {
        return MYLITE_OK;
    }
    if (column->character_set_name[0] != '\0' || column->collation_name[0] != '\0') {
        return MYLITE_OK;
    }
    if (planned_column_is_national_char_or_varchar(column)) {
        return MYLITE_OK;
    }
    if (!planned_column_allows_charset_collation_attributes(column)) {
        return MYLITE_OK;
    }

    rc = normalize_text_length_argument_for_default_charset(database, column, default_charset);
    if (rc == MYLITE_OK) {
        rc = normalize_column_to_binary_string_descriptor(database, column);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    column->character_set_name[0] = '\0';
    column->collation_name[0] = '\0';
    return validate_column_default(database, column->default_node, column);
}

static bool create_table_column_has_explicit_charset_or_collation(
    const struct mylite_sql_ast_node *column_node
) {
    return (child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE) != NULL ||
            child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE) != NULL) != 0;
}

static bool planned_create_table_default_charset_is_binary(
    const struct planned_create_table *plan
) {
    if (plan == NULL) {
        return false;
    }
    if (!charset_name_is_binary(plan->default_charset)) {
        return false;
    }
    return collation_name_is_binary(plan->default_collation);
}

static int count_create_table_column_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    size_t *out_column_count
) {
    const struct mylite_sql_ast_node *item = NULL;
    int rc = validate_create_table_item_list(database, item_list);

    *out_column_count = 0U;
    if (rc != MYLITE_OK) {
        return rc;
    }

    item = child_at(item_list, 0U);
    while (item != NULL) {
        if (item->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
            ++(*out_column_count);
        }
        item = item->next_sibling;
    }
    if (*out_column_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (item_list == NULL || item_list->kind != MYLITE_SQL_AST_COLUMN_DEFINITION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item = child_at(item_list, 0U);
    while (item != NULL) {
        if (item->kind != MYLITE_SQL_AST_COLUMN_DEFINITION &&
            item->kind != MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION &&
            item->kind != MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION &&
            item->kind != MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION &&
            item->kind != MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

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
        } else if (
            child->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION &&
            out_nodes->options.index_type_node == NULL
        ) {
            out_nodes->options.index_type_node = child;
        } else if (
            child->kind == MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST && out_nodes->part_list == NULL
        ) {
            out_nodes->part_list = child;
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
        out_nodes->part_list->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int apply_create_table_foreign_key_definitions(
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
        if (item->kind == MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION) {
            rc = apply_create_table_foreign_key_definition(database, plan, item);
        }
        item = item->next_sibling;
    }

    return rc;
}

static int apply_create_table_foreign_key_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *foreign_key
) {
    struct foreign_key_definition_nodes nodes = {0};
    struct foreign_key_column_names names = {0};
    struct planned_foreign_key planned = {0};
    int rc = extract_foreign_key_definition_nodes(database, foreign_key, &nodes);

    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_part_names(
            database,
            nodes.child_parts_node,
            &names.child_parts,
            &names.child_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_part_names(
            database,
            nodes.parent_parts_node,
            &names.parent_parts,
            &names.parent_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_foreign_key_part_counts(
            database,
            names.child_part_count,
            names.parent_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_foreign_key_child_parts(database, &names);
    }
    if (rc == MYLITE_OK) {
        rc = plan_create_table_foreign_key_name(
            database,
            plan,
            nodes.constraint_name_node,
            planned.name,
            sizeof(planned.name)
        );
        planned.has_explicit_name = nodes.constraint_name_node != NULL;
    }
    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_index_name(
            database,
            nodes.index_name_node,
            planned.child_index_name,
            sizeof(planned.child_index_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = parse_foreign_key_actions(
            database,
            nodes.action_list_node,
            planned.update_rule,
            sizeof(planned.update_rule),
            planned.delete_rule,
            sizeof(planned.delete_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_create_table_foreign_key_parent(
            database,
            plan,
            nodes.parent_table_node,
            &names,
            &planned
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_planned_foreign_key_set_null_columns(database, plan, &planned);
    }
    if (rc == MYLITE_OK) {
        rc = choose_create_table_foreign_key_child_index(database, plan, &planned);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_foreign_keys(database, plan, plan->foreign_key_count + 1U);
    }
    if (rc == MYLITE_OK) {
        plan->foreign_keys[plan->foreign_key_count] = planned;
        planned = (struct planned_foreign_key){0};
        ++plan->foreign_key_count;
    }

    foreign_key_column_names_deinit(&names);
    planned_foreign_key_deinit(&planned);
    return rc;
}

static int apply_create_table_check_constraint_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *item = NULL;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (item_list == NULL || plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item = child_at(item_list, 0U);
    while (rc == MYLITE_OK && item != NULL) {
        if (item->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
            if (column_index >= plan->column_count) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            rc = apply_create_table_column_check_constraint_definitions(
                database,
                plan,
                item,
                column_index
            );
            ++column_index;
        } else if (item->kind == MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
            const struct create_table_check_constraint_definition definition = {
                .check_constraint = item,
                .inline_enforcement = NULL,
                .inline_column = NULL,
                .inline_column_index = 0U,
            };

            rc = apply_create_table_check_constraint_definition(database, plan, &definition);
        }
        item = item->next_sibling;
    }

    return rc;
}

static int apply_create_table_column_check_constraint_definitions(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_definition,
    size_t column_index
) {
    const struct mylite_sql_ast_node *attribute = child_at(column_definition, 2U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && attribute != NULL) {
        if (attribute->kind == MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
            const struct mylite_sql_ast_node *next_attribute = attribute->next_sibling;
            const bool next_attribute_is_enforcement =
                check_constraint_node_is_enforcement(next_attribute);
            const struct mylite_sql_ast_node *inline_enforcement = NULL;
            struct create_table_check_constraint_definition definition = {
                .check_constraint = attribute,
                .inline_enforcement = NULL,
                .inline_column = &plan->columns[column_index],
                .inline_column_index = column_index,
            };

            if (next_attribute_is_enforcement) {
                inline_enforcement = next_attribute;
                definition.inline_enforcement = inline_enforcement;
            }

            rc = apply_create_table_check_constraint_definition(database, plan, &definition);
            if (inline_enforcement != NULL) {
                attribute = inline_enforcement;
            }
        } else if (check_constraint_node_is_enforcement(attribute)) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        attribute = attribute->next_sibling;
    }

    return rc;
}

static int apply_create_table_check_constraint_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct create_table_check_constraint_definition *definition
) {
    const struct mylite_sql_ast_node *check_constraint = NULL;
    const struct mylite_sql_ast_node *expression = child_at(check_constraint, 0U);
    const struct mylite_sql_ast_node *child = child_at(check_constraint, 1U);
    const struct mylite_sql_ast_node *name_node = NULL;
    int64_t generated_ordinal = next_planned_check_constraint_generated_ordinal(plan);
    struct planned_check_constraint planned = {
        .generated_ordinal = generated_ordinal,
        .ordinal_position = (int64_t)plan->check_constraint_count + 1,
        .is_enforced = true,
        .name_is_generated = false,
    };
    int rc = MYLITE_OK;

    if (definition != NULL) {
        check_constraint = definition->check_constraint;
        expression = child_at(check_constraint, 0U);
        child = child_at(check_constraint, 1U);
    }

    if (definition == NULL || check_constraint == NULL ||
        check_constraint->kind != MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION ||
        expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            name_node = child;
        } else if (check_constraint_node_is_not_enforced(child)) {
            planned.is_enforced = false;
        }
        child = child->next_sibling;
    }
    if (check_constraint_node_is_not_enforced(definition->inline_enforcement)) {
        planned.is_enforced = false;
    }

    rc = plan_create_table_check_constraint_name(database, plan, name_node, &planned);
    if (rc == MYLITE_OK) {
        rc = render_check_constraint_expression(
            database,
            plan,
            definition->inline_column,
            definition->inline_column_index,
            expression,
            planned.check_clause,
            sizeof(planned.check_clause),
            planned.sqlite_expression,
            sizeof(planned.sqlite_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_check_constraints(database, plan, plan->check_constraint_count + 1U);
    }
    if (rc == MYLITE_OK) {
        plan->check_constraints[plan->check_constraint_count] = planned;
        ++plan->check_constraint_count;
    }

    return rc;
}

static int64_t next_planned_check_constraint_generated_ordinal(
    const struct planned_create_table *plan
) {
    int64_t ordinal = 1;

    if (plan == NULL) {
        return ordinal;
    }
    for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
        if (plan->check_constraints[index].name_is_generated &&
            plan->check_constraints[index].generated_ordinal >= ordinal) {
            ordinal = plan->check_constraints[index].generated_ordinal + 1;
        }
    }
    return ordinal;
}

static int plan_create_table_check_constraint_name(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *name_node,
    struct planned_check_constraint *check_constraint
) {
    int rc = MYLITE_OK;

    check_constraint->name_is_generated = name_node == NULL;
    if (name_node == NULL) {
        rc = generate_create_table_check_constraint_name(
            database,
            plan,
            check_constraint->generated_ordinal,
            check_constraint->name,
            sizeof(check_constraint->name)
        );
    } else {
        rc = copy_identifier_text(
            name_node,
            check_constraint->name,
            sizeof(check_constraint->name),
            database
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (planned_check_constraint_name_is_used(plan, check_constraint->name)) {
        set_duplicate_check_constraint_error(database, check_constraint->name);
        return MYLITE_ERROR;
    }

    return reject_duplicate_schema_check_constraint_name(database, plan, check_constraint->name);
}

static int generate_create_table_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t generated_ordinal,
    char *destination,
    size_t destination_size
) {
    int written = snprintf(
        destination,
        destination_size,
        "%s_chk_%" PRId64,
        plan->target.table_name,
        generated_ordinal
    );

    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "generated CHECK constraint name is too long");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool planned_check_constraint_name_is_used(
    const struct planned_create_table *plan,
    const char *check_constraint_name
) {
    for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                plan->check_constraints[index].name,
                check_constraint_name
            )) {
            return true;
        }
    }
    return false;
}

static int reject_duplicate_schema_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *check_constraint_name
) {
    struct find_check_constraint_name_context context = {
        .name = check_constraint_name,
        .found = false,
    };
    int rc = mylite_catalog_for_each_check_constraint_in_schema(
        database,
        plan->target.schema.schema_id,
        reject_duplicate_schema_check_constraint_name_callback,
        &context
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read CHECK constraint descriptors");
        return rc;
    }
    if (context.found) {
        set_duplicate_check_constraint_error(database, check_constraint_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_duplicate_schema_check_constraint_name_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
) {
    struct find_check_constraint_name_context *context = user_data;

    if (check_constraint == NULL || context == NULL || context->name == NULL) {
        return MYLITE_MISUSE;
    }
    if (text_equals_ascii_case_insensitive(check_constraint->name, context->name)) {
        context->found = true;
    }
    return MYLITE_OK;
}

static int finalize_planned_generated_columns(
    struct mylite_db *database,
    struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = finalize_planned_generated_column(database, plan, column_index);
    }

    return rc;
}

static int finalize_planned_generated_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index
) {
    struct planned_column *column = &plan->columns[column_index];

    if (!column->is_generated) {
        return MYLITE_OK;
    }
    if (column->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL &&
        column->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_STORED) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column->physical_type == NULL || strcmp(column->physical_type, "INTEGER") != 0 ||
        planned_column_is_bit(column) || planned_column_is_year(column)) {
        set_unsupported_error(
            database,
            "generated columns support only baseline integer descriptor target columns"
        );
        return MYLITE_ERROR;
    }
    {
        struct integer_column_range range = {0};
        int rc = integer_range_for_logical_type(
            database,
            (struct integer_logical_type_range_request){
                .logical_type = column->logical_type,
                .unsupported_message =
                    "generated columns support only baseline integer descriptor target columns",
            },
            &range
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return render_generated_column_expression(
        database,
        plan,
        column_index,
        column->generation_expression_node,
        column->generation_expression,
        sizeof(column->generation_expression),
        column->sqlite_generation_expression,
        sizeof(column->sqlite_generation_expression)
    );
}

static int render_generated_column_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t generated_column_index,
    const struct mylite_sql_ast_node *expression,
    char *generation_expression,
    size_t generation_expression_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
) {
    struct mylite_dynamic_string generation_string;
    struct mylite_dynamic_string sqlite_string;
    struct generated_expression_render_context context = {
        .database = database,
        .plan = plan,
        .generated_column_index = generated_column_index,
        .generation_expression = &generation_string,
        .sqlite_expression = &sqlite_string,
    };
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&generation_string);
    mylite_dynamic_string_init(&sqlite_string);
    rc = mylite_dynamic_string_reserve(&generation_string, 1U);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_reserve(&sqlite_string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = render_generated_expression_node(&context, expression);
    }
    if (rc == MYLITE_OK && (generation_string.length >= generation_expression_size ||
                            sqlite_string.length >= sqlite_expression_size)) {
        set_unsupported_error(database, "generated column expression is too long");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(generation_expression, generation_string.text, generation_string.length + 1U);
        memcpy(sqlite_expression, sqlite_string.text, sqlite_string.length + 1U);
    }

    mylite_dynamic_string_deinit(&generation_string);
    mylite_dynamic_string_deinit(&sqlite_string);
    return rc;
}

static int render_generated_expression_node(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    struct generated_expression_render_work_item *items = NULL;
    size_t item_count = 0U;
    int rc = append_generated_expression_render_node(context->database, &items, &item_count, node);

    while (rc == MYLITE_OK && item_count != 0U) {
        struct generated_expression_render_work_item item = items[--item_count];

        rc = render_generated_expression_work_item(context, item, &items, &item_count);
    }
    free(items);
    return rc;
}

static int render_generated_expression_work_item(
    struct generated_expression_render_context *context,
    struct generated_expression_render_work_item item,
    struct generated_expression_render_work_item **items,
    size_t *item_count
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_expression(item.node);
    int rc = MYLITE_OK;

    switch (item.kind) {
    case GENERATED_EXPRESSION_RENDER_TEXT:
        rc = mylite_dynamic_string_append(context->generation_expression, item.text);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, item.text);
        }
        return rc;
    case GENERATED_EXPRESSION_RENDER_CHAR:
        rc = mylite_dynamic_string_append_char(context->generation_expression, item.character);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(context->sqlite_expression, item.character);
        }
        return rc;
    case GENERATED_EXPRESSION_RENDER_NODE:
        break;
    }

    if (node == NULL) {
        set_parse_error(context->database, NULL);
        return MYLITE_ERROR;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_IDENTIFIER:
        return render_generated_expression_column(context, node);
    case MYLITE_SQL_AST_LITERAL:
        return render_generated_expression_literal(context, node);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return render_generated_expression_unary(context, node, items, item_count);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return render_generated_expression_binary(context, node, items, item_count);
    default:
        break;
    }

    set_unsupported_error(
        context->database,
        "generated columns support only unqualified integer columns, integer literals, NULL, "
        "TRUE, FALSE, unary signs, and +, -, * expressions"
    );
    return MYLITE_ERROR;
}

static int render_generated_expression_column(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    const struct planned_column *column = NULL;
    int rc = copy_identifier_text(node, column_name, sizeof(column_name), context->database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_planned_column_index(
        context->plan->columns,
        context->plan->column_count,
        column_name,
        &column_index
    );
    if (rc != MYLITE_OK) {
        set_unknown_column_error(context->database, column_name);
        return MYLITE_ERROR;
    }
    if (column_index >= context->generated_column_index) {
        set_unsupported_error(
            context->database,
            "generated columns support only references to previously declared base columns"
        );
        return MYLITE_ERROR;
    }

    column = &context->plan->columns[column_index];
    if (column->is_generated || column->physical_type == NULL ||
        strcmp(column->physical_type, "INTEGER") != 0 || planned_column_is_bit(column) ||
        planned_column_is_year(column)) {
        set_unsupported_error(
            context->database,
            "generated column expressions support only baseline integer descriptor columns"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_mysql_quoted_identifier(
        context->generation_expression,
        column->name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            context->sqlite_expression,
            column->name
        );
    }
    return rc;
}

static int render_generated_expression_literal(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(node);

    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        int rc = append_check_expression_span_text(
            context->database,
            context->generation_expression,
            node
        );

        if (rc == MYLITE_OK) {
            rc = append_check_expression_span_text(
                context->database,
                context->sqlite_expression,
                node
            );
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        int rc = mylite_dynamic_string_append(context->generation_expression, "TRUE");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, "1");
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        int rc = mylite_dynamic_string_append(context->generation_expression, "FALSE");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, "0");
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        int rc = mylite_dynamic_string_append(context->generation_expression, "NULL");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, "NULL");
        }
        return rc;
    }

    set_unsupported_error(
        context->database,
        "generated column expressions support only integer, boolean, and NULL literals"
    );
    return MYLITE_ERROR;
}

static int render_generated_expression_unary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    int rc = MYLITE_OK;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        return append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U)
        );
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        set_unsupported_error(
            context->database,
            "generated column expressions support only unary + and -"
        );
        return MYLITE_ERROR;
    }

    rc = append_generated_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_text(context->database, items, item_count, "-(");
    }
    return rc;
}

static int render_generated_expression_binary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    const char *operator_text = generated_expression_binary_operator_text(operator_kind);
    int rc = MYLITE_OK;

    if (operator_text == NULL) {
        set_unsupported_error(
            context->database,
            "generated column expressions support only +, -, *"
        );
        return MYLITE_ERROR;
    }

    rc = append_generated_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 1U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_text(
            context->database,
            items,
            item_count,
            operator_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_generated_expression_render_char(context->database, items, item_count, '(');
    }
    return rc;
}

static const char *generated_expression_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return " + ";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return " - ";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return " * ";
    default:
        return NULL;
    }
}

static int append_generated_expression_render_node(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
) {
    return append_generated_expression_render_work_item(
        database,
        items,
        item_count,
        (struct generated_expression_render_work_item){
            .kind = GENERATED_EXPRESSION_RENDER_NODE,
            .node = node,
        }
    );
}

static int append_generated_expression_render_text(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const char *text
) {
    return append_generated_expression_render_work_item(
        database,
        items,
        item_count,
        (struct generated_expression_render_work_item){
            .kind = GENERATED_EXPRESSION_RENDER_TEXT,
            .text = text,
        }
    );
}

static int append_generated_expression_render_char(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    char character
) {
    return append_generated_expression_render_work_item(
        database,
        items,
        item_count,
        (struct generated_expression_render_work_item){
            .kind = GENERATED_EXPRESSION_RENDER_CHAR,
            .character = character,
        }
    );
}

static int append_generated_expression_render_work_item(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    struct generated_expression_render_work_item item
) {
    struct generated_expression_render_work_item *grown_items = NULL;
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

static int render_check_constraint_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_column *inline_column,
    size_t inline_column_index,
    const struct mylite_sql_ast_node *expression,
    char *check_clause,
    size_t check_clause_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
) {
    struct mylite_dynamic_string check_string;
    struct mylite_dynamic_string sqlite_string;
    struct check_expression_render_context context = {
        .database = database,
        .plan = plan,
        .inline_column = inline_column,
        .inline_column_index = inline_column_index,
        .check_clause = &check_string,
        .sqlite_expression = &sqlite_string,
    };
    bool is_boolean = false;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&check_string);
    mylite_dynamic_string_init(&sqlite_string);
    rc = mylite_dynamic_string_reserve(&check_string, 1U);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_reserve(&sqlite_string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = render_check_expression_node(&context, expression, true, &is_boolean);
    }
    if (rc == MYLITE_OK && !is_boolean) {
        set_check_constraint_non_boolean_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && (check_string.length >= check_clause_size ||
                            sqlite_string.length >= sqlite_expression_size)) {
        set_unsupported_error(database, "CHECK constraint expression is too long");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(check_clause, check_string.text, check_string.length + 1U);
        memcpy(sqlite_expression, sqlite_string.text, sqlite_string.length + 1U);
    }

    mylite_dynamic_string_deinit(&check_string);
    mylite_dynamic_string_deinit(&sqlite_string);
    return rc;
}

static int render_check_expression_node(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
) {
    struct check_expression_render_work_item *items = NULL;
    size_t item_count = 0U;
    bool root_seen = false;
    int rc = MYLITE_OK;

    *out_is_boolean = false;

    rc = append_check_expression_render_node(
        context->database,
        &items,
        &item_count,
        node,
        require_boolean
    );
    while (rc == MYLITE_OK && item_count != 0U) {
        bool item_is_boolean = false;
        struct check_expression_render_work_item item = items[--item_count];

        rc =
            render_check_expression_work_item(context, item, &items, &item_count, &item_is_boolean);
        if (rc == MYLITE_OK && !root_seen && item.kind == CHECK_EXPRESSION_RENDER_NODE) {
            *out_is_boolean = item_is_boolean;
            root_seen = true;
        }
    }
    free(items);
    return rc;
}

static int render_check_expression_work_item(
    struct check_expression_render_context *context,
    struct check_expression_render_work_item item,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
) {
    const struct mylite_sql_ast_node *node = unwrap_parenthesized_expression(item.node);
    int rc = MYLITE_OK;

    *out_is_boolean = false;
    switch (item.kind) {
    case CHECK_EXPRESSION_RENDER_TEXT:
        rc = mylite_dynamic_string_append(context->check_clause, item.text);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(context->sqlite_expression, item.text);
        }
        return rc;
    case CHECK_EXPRESSION_RENDER_CHAR:
        rc = mylite_dynamic_string_append_char(context->check_clause, item.character);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(context->sqlite_expression, item.character);
        }
        return rc;
    case CHECK_EXPRESSION_RENDER_OPERATOR:
        rc = mylite_dynamic_string_append(
            context->check_clause,
            check_expression_check_clause_operator_sql(item.operator_kind)
        );
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(
                context->sqlite_expression,
                check_expression_sqlite_operator_sql(item.operator_kind)
            );
        }
        return rc;
    case CHECK_EXPRESSION_RENDER_NODE:
        break;
    }

    if (node == NULL) {
        set_parse_error(context->database, NULL);
        return MYLITE_ERROR;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_IDENTIFIER:
        return render_check_expression_column(context, node);
    case MYLITE_SQL_AST_LITERAL:
        return render_check_expression_literal(context, node, item.require_boolean, out_is_boolean);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return render_check_expression_unary(context, node, items, item_count, out_is_boolean);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return render_check_expression_binary(context, node, items, item_count, out_is_boolean);
    case MYLITE_SQL_AST_SCALAR_SUBQUERY:
        set_check_constraint_subquery_error(context->database);
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        set_check_constraint_variable_error(context->database);
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
    case MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR:
        set_check_constraint_function_error(context->database);
        return MYLITE_ERROR;
    default:
        break;
    }

    if (is_scalar_function_projection_expression(node)) {
        set_check_constraint_function_error(context->database);
        return MYLITE_ERROR;
    }

    set_unsupported_error(
        context->database,
        "CHECK constraints support only integer columns, integer literals, boolean comparisons, "
        "IS NULL, IS NOT NULL, AND, OR, and NOT"
    );
    return MYLITE_ERROR;
}

static int render_check_expression_column(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node
) {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = copy_identifier_text(node, column_name, sizeof(column_name), context->database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_planned_column_index(
        context->plan->columns,
        context->plan->column_count,
        column_name,
        &column_index
    );
    if (rc != MYLITE_OK) {
        if (context->alter_check_constraint_name != NULL) {
            set_alter_check_constraint_unknown_column_error(
                context->database,
                column_name,
                context->alter_check_constraint_name
            );
        } else {
            set_check_constraint_unknown_column_error(context->database, column_name);
        }
        return MYLITE_ERROR;
    }
    if (context->inline_column != NULL && column_index != context->inline_column_index) {
        set_check_constraint_column_ref_error(context->database, "", column_name);
        return MYLITE_ERROR;
    }
    if (context->plan->columns[column_index].is_auto_increment) {
        set_check_constraint_auto_increment_error(context->database);
        return MYLITE_ERROR;
    }
    if (context->plan->columns[column_index].physical_type == NULL ||
        strcmp(context->plan->columns[column_index].physical_type, "INTEGER") != 0 ||
        planned_column_is_year(&context->plan->columns[column_index]) ||
        planned_column_is_bit(&context->plan->columns[column_index])) {
        set_unsupported_error(
            context->database,
            "CHECK constraints support only integer descriptor columns"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_mysql_quoted_identifier(
        context->check_clause,
        context->plan->columns[column_index].name
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            context->sqlite_expression,
            context->plan->columns[column_index].name
        );
    }
    return rc;
}

static int render_check_expression_literal(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(node);

    *out_is_boolean = false;
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_is_boolean = true;
        if (mylite_dynamic_string_append(context->check_clause, "TRUE") != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(context->sqlite_expression, "1");
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_is_boolean = true;
        if (mylite_dynamic_string_append(context->check_clause, "FALSE") != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(context->sqlite_expression, "0");
    }
    if (require_boolean) {
        set_check_constraint_non_boolean_error(context->database);
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (append_check_expression_span_text(context->database, context->check_clause, node) !=
            MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return append_check_expression_span_text(
            context->database,
            context->sqlite_expression,
            node
        );
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        if (mylite_dynamic_string_append(context->check_clause, "NULL") != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return mylite_dynamic_string_append(context->sqlite_expression, "NULL");
    }

    set_unsupported_error(
        context->database,
        "CHECK constraints support only integer and boolean literals"
    );
    return MYLITE_ERROR;
}

static int render_check_expression_unary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    const char *operator_sql = NULL;
    int rc = MYLITE_OK;

    *out_is_boolean = false;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        operator_sql = "+";
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            operator_sql = "-";
        }
        rc = append_check_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U),
            false
        );
        if (rc == MYLITE_OK) {
            rc = append_check_expression_render_text(
                context->database,
                items,
                item_count,
                operator_sql
            );
        }
        return rc;
    }
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        set_unsupported_error(
            context->database,
            "CHECK constraints support only +, -, and NOT unary operators"
        );
        return MYLITE_ERROR;
    }

    rc = append_check_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U),
            true
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_text(context->database, items, item_count, "(NOT ");
    }
    if (rc == MYLITE_OK) {
        *out_is_boolean = true;
    }
    return rc;
}

static int render_check_expression_binary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(node);
    bool require_child_boolean =
        check_expression_binary_operator_requires_boolean_children(operator_kind);
    int rc = MYLITE_OK;

    *out_is_boolean = false;
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR) {
        set_unsupported_error(context->database, "CHECK constraints do not yet support XOR");
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_IS_TRUE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_FALSE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN) {
        set_unsupported_error(
            context->database,
            "CHECK constraints support only IS NULL and IS NOT NULL"
        );
        return MYLITE_ERROR;
    }
    if (!check_expression_binary_operator_is_boolean(operator_kind) &&
        !check_expression_binary_operator_is_arithmetic(operator_kind)) {
        set_unsupported_error(
            context->database,
            "CHECK constraints do not support this binary operator"
        );
        return MYLITE_ERROR;
    }

    rc = append_check_expression_render_char(context->database, items, item_count, ')');
    if (rc == MYLITE_OK) {
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NULL ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            rc = append_check_expression_render_text(context->database, items, item_count, "NULL");
        } else {
            rc = append_check_expression_render_node(
                context->database,
                items,
                item_count,
                child_at(node, 1U),
                require_child_boolean
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_text(context->database, items, item_count, " ");
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_operator(
            context->database,
            items,
            item_count,
            operator_kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_text(context->database, items, item_count, " ");
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_node(
            context->database,
            items,
            item_count,
            child_at(node, 0U),
            require_child_boolean
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_check_expression_render_char(context->database, items, item_count, '(');
    }
    if (rc == MYLITE_OK && check_expression_binary_operator_is_boolean(operator_kind)) {
        *out_is_boolean = true;
    }
    return rc;
}

static bool check_expression_binary_operator_is_boolean(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return true;
    default:
        return false;
    }
}

static bool check_expression_binary_operator_is_arithmetic(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return true;
    default:
        return false;
    }
}

static bool check_expression_binary_operator_requires_boolean_children(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return true;
    default:
        return false;
    }
}

static const char *check_expression_check_clause_operator_sql(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "<=>";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        return "IS NOT";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        return "AND";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return "OR";
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "+";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "-";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "*";
    default:
        return "";
    }
}

static const char *check_expression_sqlite_operator_sql(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
        return "IS";
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        return "IS NOT";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_AND:
        return "AND";
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_DEPRECATED_LOGICAL_OR:
        return "OR";
    case MYLITE_SQL_AST_OPERATOR_ADD:
        return "+";
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        return "-";
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return "*";
    default:
        return "";
    }
}

static int append_check_expression_span_text(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *node
) {
    if (node == NULL || node->span.text == NULL || node->span.length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < node->span.length; ++index) {
        int rc = mylite_dynamic_string_append_char(string, node->span.text[index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_check_expression_render_node(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node,
    bool require_boolean
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_NODE,
            .node = node,
            .require_boolean = require_boolean,
        }
    );
}

static int append_check_expression_render_text(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const char *text
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_TEXT,
            .text = text,
        }
    );
}

static int append_check_expression_render_char(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    char character
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_CHAR,
            .character = character,
        }
    );
}

static int append_check_expression_render_operator(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
) {
    return append_check_expression_render_work_item(
        database,
        items,
        item_count,
        (struct check_expression_render_work_item){
            .kind = CHECK_EXPRESSION_RENDER_OPERATOR,
            .operator_kind = operator_kind,
        }
    );
}

static int append_check_expression_render_work_item(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    struct check_expression_render_work_item item
) {
    struct check_expression_render_work_item *grown_items = NULL;
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
