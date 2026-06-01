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

static bool check_constraint_node_is_not_enforced(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    return node->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED;
}

static bool check_constraint_node_is_enforcement(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED) {
        return true;
    }
    return node->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_NOT_ENFORCED;
}

static int reserve_planned_check_constraints(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t requested_capacity
) {
    enum { initial_check_constraint_capacity = 4 };

    struct planned_check_constraint *checks = NULL;
    size_t capacity = 0U;

    if (requested_capacity <= plan->check_constraint_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->check_constraint_capacity == 0U ? initial_check_constraint_capacity
                                                     : plan->check_constraint_capacity;
    while (capacity < requested_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*checks)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    checks = realloc(plan->check_constraints, capacity * sizeof(*checks));
    if (checks == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->check_constraints = checks;
    plan->check_constraint_capacity = capacity;
    return MYLITE_OK;
}

static int extract_foreign_key_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *foreign_key,
    struct foreign_key_definition_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *child = NULL;

    *out_nodes = (struct foreign_key_definition_nodes){0};

    if (foreign_key == NULL || foreign_key->kind != MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    child = child_at(foreign_key, 0U);
    while (child != NULL) {
        if (record_foreign_key_definition_node(database, child, out_nodes) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        child = child->next_sibling;
    }
    if (out_nodes->child_parts_node == NULL ||
        out_nodes->child_parts_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST ||
        out_nodes->parent_table_node == NULL || out_nodes->parent_parts_node == NULL ||
        out_nodes->parent_parts_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int record_foreign_key_definition_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    switch (node->kind) {
    case MYLITE_SQL_AST_IDENTIFIER:
        return record_foreign_key_identifier_node(database, node, out_nodes);
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return record_foreign_key_qualified_identifier_node(database, node, out_nodes);
    case MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME:
        return record_foreign_key_index_name_node(database, node, out_nodes);
    case MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST:
        return record_foreign_key_part_list_node(database, node, out_nodes);
    case MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST:
        return record_foreign_key_action_list_node(database, node, out_nodes);
    default:
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
}

static int record_foreign_key_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node == NULL && out_nodes->constraint_name_node == NULL) {
        out_nodes->constraint_name_node = node;
        return MYLITE_OK;
    }
    if (out_nodes->child_parts_node != NULL && out_nodes->parent_table_node == NULL) {
        out_nodes->parent_table_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_qualified_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node != NULL && out_nodes->parent_table_node == NULL) {
        out_nodes->parent_table_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_index_name_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node == NULL && out_nodes->index_name_node == NULL) {
        out_nodes->index_name_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_part_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->child_parts_node == NULL) {
        out_nodes->child_parts_node = node;
        return MYLITE_OK;
    }
    if (out_nodes->parent_table_node != NULL && out_nodes->parent_parts_node == NULL) {
        out_nodes->parent_parts_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int record_foreign_key_action_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
) {
    if (out_nodes->parent_parts_node != NULL && out_nodes->action_list_node == NULL) {
        out_nodes->action_list_node = node;
        return MYLITE_OK;
    }
    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int copy_foreign_key_index_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *index_name_node,
    char *destination,
    size_t destination_size
) {
    const struct mylite_sql_ast_node *identifier = NULL;

    if (destination == NULL || destination_size == 0U) {
        set_runtime_error(database, "invalid foreign-key index descriptor");
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    if (index_name_node == NULL) {
        return MYLITE_OK;
    }
    if (index_name_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    identifier = child_at(index_name_node, 0U);
    return copy_identifier_text(identifier, destination, destination_size, database);
}

static int parse_foreign_key_actions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *action_list_node,
    char *update_rule,
    size_t update_rule_size,
    char *delete_rule,
    size_t delete_rule_size
) {
    const struct mylite_sql_ast_node *action = NULL;
    bool saw_update = false;
    bool saw_delete = false;
    int rc = copy_foreign_key_rule_text(database, update_rule, update_rule_size, "NO ACTION");

    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_rule_text(database, delete_rule, delete_rule_size, "NO ACTION");
    }
    if (rc != MYLITE_OK || action_list_node == NULL) {
        return rc;
    }
    if (action_list_node->kind != MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    action = child_at(action_list_node, 0U);
    while (action != NULL) {
        bool is_update = false;
        const char *rule = NULL;

        switch (action->kind) {
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_CASCADE:
            rule = "CASCADE";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_RESTRICT:
            rule = "RESTRICT";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_NO_ACTION:
            rule = "NO ACTION";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_DELETE_SET_NULL:
            rule = "SET NULL";
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_CASCADE:
            rule = "CASCADE";
            is_update = true;
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_RESTRICT:
            rule = "RESTRICT";
            is_update = true;
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_NO_ACTION:
            rule = "NO ACTION";
            is_update = true;
            break;
        case MYLITE_SQL_AST_FOREIGN_KEY_ON_UPDATE_SET_NULL:
            rule = "SET NULL";
            is_update = true;
            break;
        default:
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        if (is_update) {
            if (saw_update) {
                set_unsupported_error(database, "FOREIGN KEY supports only one ON UPDATE action");
                return MYLITE_ERROR;
            }
            saw_update = true;
            rc = copy_foreign_key_rule_text(database, update_rule, update_rule_size, rule);
        } else {
            if (saw_delete) {
                set_unsupported_error(database, "FOREIGN KEY supports only one ON DELETE action");
                return MYLITE_ERROR;
            }
            saw_delete = true;
            rc = copy_foreign_key_rule_text(database, delete_rule, delete_rule_size, rule);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        action = action->next_sibling;
    }

    return MYLITE_OK;
}

static int copy_foreign_key_rule_text(
    struct mylite_db *database,
    char *destination,
    size_t destination_size,
    const char *rule
) {
    int written = 0;

    if (destination == NULL || rule == NULL) {
        set_runtime_error(database, "invalid foreign-key action descriptor");
        return MYLITE_ERROR;
    }

    written = snprintf(destination, destination_size, "%s", rule);
    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "foreign-key action descriptor is too long");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool foreign_key_rule_equals(const char *rule, const char *expected_rule) {
    int comparison = 0;

    if (rule == NULL || expected_rule == NULL) {
        return false;
    }

    comparison = strcmp(rule, expected_rule);
    if (comparison == 0) {
        return true;
    }
    return false;
}

static bool foreign_key_rule_is_cascade(const char *rule) {
    return foreign_key_rule_equals(rule, "CASCADE");
}

static bool foreign_key_rule_is_set_null(const char *rule) {
    return foreign_key_rule_equals(rule, "SET NULL");
}

static int validate_planned_foreign_key_set_null_columns(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
) {
    if (!foreign_key_rule_is_set_null(foreign_key->delete_rule) &&
        !foreign_key_rule_is_set_null(foreign_key->update_rule)) {
        return MYLITE_OK;
    }

    for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
        const size_t child_column_index = foreign_key->parts[part_index].child_column_index;

        if (child_column_index >= plan->column_count) {
            set_runtime_error(database, "invalid foreign-key child column descriptor");
            return MYLITE_ERROR;
        }
        if (!plan->columns[child_column_index].is_nullable) {
            set_foreign_key_set_null_not_nullable_error(
                database,
                plan->columns[child_column_index].name,
                foreign_key->name
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static void foreign_key_column_names_deinit(struct foreign_key_column_names *names) {
    if (names == NULL) {
        return;
    }
    free(names->child_parts);
    free(names->parent_parts);
    *names = (struct foreign_key_column_names){0};
}

static int copy_foreign_key_part_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *part_list,
    struct foreign_key_part_name **out_parts,
    size_t *out_part_count
) {
    struct foreign_key_part_name *parts = NULL;
    size_t part_count = 0U;

    *out_parts = NULL;
    *out_part_count = 0U;

    if (part_list == NULL || part_list->kind != MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    part_count = mylite_sql_ast_node_child_count(part_list);
    if (part_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (part_count > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    parts = calloc(part_count, sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < part_count; ++index) {
        const struct mylite_sql_ast_node *part = child_at(part_list, index);
        int rc = MYLITE_OK;

        if (part == NULL || part->kind != MYLITE_SQL_AST_IDENTIFIER) {
            free(parts);
            set_unsupported_error(database, "FOREIGN KEY supports only unqualified key columns");
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(part, parts[index].name, sizeof(parts[index].name), database);
        if (rc != MYLITE_OK) {
            free(parts);
            return rc;
        }
    }

    *out_parts = parts;
    *out_part_count = part_count;
    return MYLITE_OK;
}

static int validate_foreign_key_part_counts(
    struct mylite_db *database,
    size_t child_part_count,
    size_t parent_part_count
) {
    if (child_part_count == 0U || parent_part_count == 0U ||
        child_part_count != parent_part_count) {
        set_incorrect_foreign_key_definition_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_duplicate_foreign_key_child_parts(
    struct mylite_db *database,
    const struct foreign_key_column_names *names
) {
    for (size_t left = 0U; left < names->child_part_count; ++left) {
        for (size_t right = left + 1U; right < names->child_part_count; ++right) {
            if (text_equals_ascii_case_insensitive(
                    names->child_parts[left].name,
                    names->child_parts[right].name
                )) {
                set_duplicate_column_error(database, names->child_parts[right].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static int plan_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *constraint_name_node,
    char *destination,
    size_t destination_size
) {
    int rc = MYLITE_OK;

    destination[0] = '\0';
    if (constraint_name_node == NULL) {
        return generate_create_table_foreign_key_name(
            database,
            plan,
            destination,
            destination_size
        );
    }

    rc = copy_identifier_text(constraint_name_node, destination, destination_size, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (planned_foreign_key_name_is_used(plan, destination)) {
        set_duplicate_foreign_key_error(database, destination);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int generate_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    char *destination,
    size_t destination_size
) {
    unsigned int suffix = 1U;

    while (suffix < UINT_MAX) {
        char candidate[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int written =
            snprintf(candidate, sizeof(candidate), "%s_ibfk_%u", plan->target.table_name, suffix);

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_identifier_too_long_error(database, "constraint");
            return MYLITE_ERROR;
        }
        if (!planned_foreign_key_name_is_used(plan, candidate)) {
            int copied = snprintf(destination, destination_size, "%s", candidate);

            if (copied < 0 || (size_t)copied >= destination_size) {
                set_identifier_too_long_error(database, "constraint");
                return MYLITE_ERROR;
            }
            return MYLITE_OK;
        }
        ++suffix;
    }

    set_identifier_too_long_error(database, "constraint");
    return MYLITE_ERROR;
}

static bool planned_foreign_key_name_is_used(
    const struct planned_create_table *plan,
    const char *foreign_key_name
) {
    for (size_t index = 0U; index < plan->foreign_key_count; ++index) {
        if (text_equals_ascii_case_insensitive(plan->foreign_keys[index].name, foreign_key_name)) {
            return true;
        }
    }

    return false;
}

static int validate_foreign_key_integer_columns(
    struct mylite_db *database,
    const char *child_logical_type,
    const char *parent_logical_type
) {
    if (child_logical_type == NULL || parent_logical_type == NULL ||
        !text_equals_ascii_case_insensitive(child_logical_type, parent_logical_type)) {
        set_foreign_key_column_incompatible_error(database);
        return MYLITE_ERROR;
    }
    if (strcmp(child_logical_type, "TINYINT") == 0 ||
        strcmp(child_logical_type, "TINYINT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "SMALLINT") == 0 ||
        strcmp(child_logical_type, "SMALLINT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "MEDIUMINT") == 0 ||
        strcmp(child_logical_type, "MEDIUMINT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "INT") == 0 || strcmp(child_logical_type, "INT UNSIGNED") == 0 ||
        strcmp(child_logical_type, "INTEGER") == 0 ||
        strcmp(child_logical_type, "INTEGER UNSIGNED") == 0 ||
        strcmp(child_logical_type, "BIGINT") == 0 ||
        strcmp(child_logical_type, "BIGINT UNSIGNED") == 0) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, "FOREIGN KEY supports only baseline integer columns");
    return MYLITE_ERROR;
}

static void planned_foreign_key_deinit(struct planned_foreign_key *foreign_key) {
    if (foreign_key == NULL) {
        return;
    }
    free(foreign_key->parts);
    *foreign_key = (struct planned_foreign_key){0};
}

static int append_planned_foreign_key_part(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t child_column_index,
    const struct mylite_catalog_column_descriptor *parent_column
) {
    int rc = reserve_planned_foreign_key_parts(database, foreign_key, foreign_key->part_count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }

    foreign_key->parts[foreign_key->part_count] = (struct planned_foreign_key_part){
        .child_column_index = child_column_index,
        .parent_column = *parent_column,
    };
    ++foreign_key->part_count;
    return MYLITE_OK;
}

static int reserve_planned_foreign_key_parts(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t required_capacity
) {
    enum { initial_foreign_key_part_capacity = 4 };

    struct planned_foreign_key_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= foreign_key->part_capacity) {
        return MYLITE_OK;
    }
    capacity = foreign_key->part_capacity == 0U ? initial_foreign_key_part_capacity
                                                : foreign_key->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(foreign_key->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    foreign_key->parts = parts;
    foreign_key->part_capacity = capacity;
    return MYLITE_OK;
}

static int plan_create_table_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *parent_table_node,
    const struct foreign_key_column_names *names,
    struct planned_foreign_key *foreign_key
) {
    struct table_name_resolution parent_resolution = {0};
    struct mylite_catalog_column_descriptor *parent_columns = NULL;
    size_t parent_column_count = 0U;
    int rc = resolve_table_name(database, parent_table_node, &parent_resolution);

    if (rc == MYLITE_OK && parent_resolution.schema.schema_id != plan->target.schema.schema_id) {
        set_unsupported_error(database, "FOREIGN KEY does not yet support cross-schema references");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            parent_resolution.schema.schema_id,
            parent_resolution.table_name,
            &foreign_key->parent_table
        );
        if (rc != MYLITE_OK) {
            set_failed_to_open_referenced_table_error(database, parent_resolution.table_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && foreign_key->parent_table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "FOREIGN KEY references only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            foreign_key->parent_table.table_id,
            &parent_columns,
            &parent_column_count
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < names->child_part_count;
         ++part_index) {
        size_t child_column_index = 0U;
        size_t parent_column_index = 0U;

        rc = find_planned_column_index(
            plan->columns,
            plan->column_count,
            names->child_parts[part_index].name,
            &child_column_index
        );
        if (rc != MYLITE_OK) {
            set_key_column_missing_error(database, names->child_parts[part_index].name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = find_column_index(
                parent_columns,
                parent_column_count,
                names->parent_parts[part_index].name,
                &parent_column_index
            );
            if (rc != MYLITE_OK) {
                set_key_column_missing_error(database, names->parent_parts[part_index].name);
                rc = MYLITE_ERROR;
            }
        }
        if (rc == MYLITE_OK) {
            rc = validate_foreign_key_integer_columns(
                database,
                plan->columns[child_column_index].logical_type,
                parent_columns[parent_column_index].logical_type
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_planned_foreign_key_part(
                database,
                foreign_key,
                child_column_index,
                &parent_columns[parent_column_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = find_foreign_key_parent_index(
            database,
            &foreign_key->parent_table,
            foreign_key,
            &foreign_key->parent_index
        );
    }

    free(parent_columns);
    return rc;
}

static int find_foreign_key_parent_index(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *parent_table,
    const struct planned_foreign_key *foreign_key,
    struct mylite_catalog_index_descriptor *out_index
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = load_table_columns(database, parent_table->table_id, &columns, &column_count);

    *out_index = (struct mylite_catalog_index_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            parent_table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        const struct loaded_index_info *candidate = &indexes[index];
        bool matches = false;

        if (!candidate->index.is_unique || candidate->part_count != foreign_key->part_count) {
            continue;
        }
        matches = true;
        for (size_t part_index = 0U; matches && part_index < foreign_key->part_count;
             ++part_index) {
            const struct loaded_index_part *candidate_part = &candidate->parts[part_index];

            if (candidate_part->index_column.has_prefix_length ||
                candidate_part->index_column.column_id !=
                    foreign_key->parts[part_index].parent_column.column_id) {
                matches = false;
            }
        }
        if (matches) {
            *out_index = candidate->index;
            break;
        }
    }
    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index->index_id <= 0) {
        set_foreign_key_missing_unique_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int find_alter_table_foreign_key_parent_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    struct mylite_catalog_index_descriptor *out_index
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = load_table_columns(database, plan->parent_table.table_id, &columns, &column_count);

    *out_index = (struct mylite_catalog_index_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            plan->parent_table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        const struct loaded_index_info *candidate = &indexes[index];
        bool matches = false;

        if (!candidate->index.is_unique || candidate->part_count != plan->part_count) {
            continue;
        }
        matches = true;
        for (size_t part_index = 0U; matches && part_index < plan->part_count; ++part_index) {
            const struct loaded_index_part *candidate_part = &candidate->parts[part_index];

            if (candidate_part->index_column.has_prefix_length ||
                candidate_part->index_column.column_id !=
                    plan->parts[part_index].parent_column.column_id) {
                matches = false;
            }
        }
        if (matches) {
            *out_index = candidate->index;
            break;
        }
    }
    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index->index_id <= 0) {
        set_foreign_key_missing_unique_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int choose_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
) {
    if (planned_primary_key_starts_with_column(plan, foreign_key)) {
        foreign_key->child_index_is_primary = true;
        foreign_key->child_secondary_index_index = 0U;
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        const struct planned_secondary_index *candidate = &plan->secondary_indexes[index];
        bool matches = false;

        if (candidate->part_count < foreign_key->part_count) {
            continue;
        }
        matches = true;
        for (size_t part_index = 0U; matches && part_index < foreign_key->part_count;
             ++part_index) {
            if (candidate->parts[part_index].has_prefix_length ||
                candidate->parts[part_index].column_index !=
                    foreign_key->parts[part_index].child_column_index) {
                matches = false;
            }
        }
        if (matches) {
            foreign_key->child_index_is_primary = false;
            foreign_key->child_secondary_index_index = index;
            return MYLITE_OK;
        }
    }

    return add_create_table_foreign_key_child_index(database, plan, foreign_key);
}

static int add_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
) {
    struct planned_secondary_index index = {0};
    const struct planned_column *column = &plan->columns[foreign_key->parts[0].child_column_index];
    const char *default_name = column->name;
    int rc = MYLITE_OK;

    if (foreign_key->has_explicit_name) {
        default_name = foreign_key->name;
    } else if (foreign_key->child_index_name[0] != '\0') {
        default_name = foreign_key->child_index_name;
    }

    rc = plan_create_table_foreign_key_child_index_name(
        database,
        plan,
        foreign_key,
        default_name,
        index.name,
        sizeof(index.name)
    );

    if (rc == MYLITE_OK) {
        rc = reserve_planned_secondary_indexes(database, plan, plan->secondary_index_count + 1U);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_secondary_index_parts(database, &index, foreign_key->part_count);
    }
    if (rc == MYLITE_OK) {
        index.kind = MYLITE_CATALOG_INDEX_KIND_SECONDARY;
        index.is_unique = false;
        index.is_visible = true;
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        index.parts[part_index] = (struct planned_secondary_index_part){
            .column_index = foreign_key->parts[part_index].child_column_index,
            .has_prefix_length = false,
            .prefix_length = 0,
            .sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC,
        };
        ++index.part_count;
    }
    if (rc == MYLITE_OK) {
        foreign_key->child_index_is_primary = false;
        foreign_key->child_secondary_index_index = plan->secondary_index_count;
        plan->secondary_indexes[plan->secondary_index_count] = index;
        index = (struct planned_secondary_index){0};
        ++plan->secondary_index_count;
    }

    free(index.parts);
    return rc;
}

static int plan_create_table_foreign_key_child_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key,
    const char *default_name,
    char *destination,
    size_t destination_size
) {
    int copied = 0;

    if (!foreign_key->has_explicit_name && foreign_key->child_index_name[0] == '\0') {
        return plan_secondary_index_name(
            database,
            plan,
            NULL,
            default_name,
            destination,
            destination_size
        );
    }
    if (text_equals_ascii_case_insensitive(default_name, "PRIMARY")) {
        set_incorrect_index_name_error(database, default_name);
        return MYLITE_ERROR;
    }
    if (planned_index_name_is_used(plan, default_name)) {
        set_duplicate_key_name_error(database, default_name);
        return MYLITE_ERROR;
    }

    copied = snprintf(destination, destination_size, "%s", default_name);
    if (copied < 0 || (size_t)copied >= destination_size) {
        set_identifier_too_long_error(database, "index");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool planned_primary_key_starts_with_column(
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
) {
    if (plan == NULL || foreign_key == NULL || !plan->has_primary_key ||
        plan->primary_key_part_count < foreign_key->part_count) {
        return false;
    }

    for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
        if (plan->primary_key_parts[part_index].column_index !=
            foreign_key->parts[part_index].child_column_index) {
            return false;
        }
    }

    return true;
}

static int reserve_planned_foreign_keys(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    enum { initial_foreign_key_capacity = 4 };

    struct planned_foreign_key *foreign_keys = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->foreign_key_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->foreign_key_capacity == 0U ? initial_foreign_key_capacity
                                                : plan->foreign_key_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*foreign_keys)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    foreign_keys = realloc(plan->foreign_keys, capacity * sizeof(*foreign_keys));
    if (foreign_keys == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->foreign_keys = foreign_keys;
    plan->foreign_key_capacity = capacity;
    return MYLITE_OK;
}

static int append_planned_secondary_index_part(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_secondary_index *index,
    const struct mylite_sql_ast_node *part
) {
    const struct mylite_sql_ast_node *column_node = secondary_index_part_column_node(part);
    const struct mylite_sql_ast_node *prefix_node = secondary_index_part_prefix_node(part);
    const struct mylite_sql_ast_node *direction_node = secondary_index_part_direction_node(part);
    enum mylite_catalog_index_sort_direction sort_direction = index_part_sort_direction(part);
    bool is_fulltext = planned_secondary_index_is_fulltext(index);
    bool is_spatial = planned_secondary_index_is_spatial(index);
    bool has_prefix_length = false;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int64_t prefix_length = 0;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(
            database,
            index_part_unsupported_message(
                part,
                planned_secondary_index_unqualified_column_message(index)
            )
        );
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);
    if (rc == MYLITE_OK) {
        rc = find_planned_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &column_index
        );
    }
    if (rc != MYLITE_OK) {
        set_key_column_missing_error(database, column_name);
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column_index == column_index) {
            set_duplicate_column_error(database, column_name);
            return MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_planned_secondary_index_part(
            database,
            plan,
            index,
            column_index,
            column_name,
            (struct secondary_index_part_nodes){
                .prefix_node = prefix_node,
                .direction_node = direction_node,
            },
            &prefix_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_secondary_index_parts(database, index, index->part_count + 1U);
    }
    if (rc == MYLITE_OK) {
        enum mylite_catalog_index_sort_direction stored_sort_direction = sort_direction;
        int64_t stored_prefix_length = prefix_length;

        if (is_fulltext || is_spatial) {
            stored_sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
            stored_prefix_length = 0;
        } else if (prefix_node != NULL) {
            has_prefix_length = true;
        }
        index->parts[index->part_count] = (struct planned_secondary_index_part){
            .column_index = column_index,
            .has_prefix_length = has_prefix_length,
            .prefix_length = stored_prefix_length,
            .sort_direction = stored_sort_direction,
        };
        ++index->part_count;
    }

    return rc;
}

static const char *planned_secondary_index_unqualified_column_message(
    const struct planned_secondary_index *index
) {
    if (planned_secondary_index_is_fulltext(index)) {
        return "FULLTEXT indexes support only unqualified key columns";
    }
    if (planned_secondary_index_is_spatial(index)) {
        return "SPATIAL indexes support only unqualified key columns";
    }
    if (index != NULL && index->is_unique) {
        return "Unique indexes support only unqualified key columns";
    }

    return "Secondary indexes support only unqualified key columns";
}

static int validate_planned_secondary_index_part(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index,
    size_t column_index,
    const char *column_name,
    struct secondary_index_part_nodes nodes,
    int64_t *out_prefix_length
) {
    bool is_fulltext = planned_secondary_index_is_fulltext(index);
    bool is_spatial = planned_secondary_index_is_spatial(index);
    int rc = MYLITE_OK;

    if (plan == NULL || index == NULL || column_index >= plan->column_count ||
        out_prefix_length == NULL) {
        set_runtime_error(database, "invalid secondary-index key part");
        return MYLITE_ERROR;
    }
    if ((is_fulltext || is_spatial) && nodes.direction_node != NULL) {
        set_fulltext_explicit_order_error(database);
        return MYLITE_ERROR;
    }
    if (nodes.prefix_node != NULL) {
        uint64_t key_bytes = 0U;

        rc = parse_secondary_index_part_prefix_length(
            database,
            nodes.prefix_node,
            column_name,
            out_prefix_length
        );
        if (rc == MYLITE_OK && is_spatial) {
            set_incorrect_prefix_key_error(database);
            rc = MYLITE_ERROR;
        } else if (rc == MYLITE_OK && !is_fulltext) {
            rc = validate_secondary_index_prefix_for_planned_column(
                database,
                &plan->columns[column_index],
                *out_prefix_length,
                &key_bytes
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (is_fulltext) {
        return validate_fulltext_index_column(database, &plan->columns[column_index]);
    }
    if (is_spatial) {
        return validate_spatial_index_column(database, &plan->columns[column_index]);
    }
    if (nodes.prefix_node != NULL) {
        return MYLITE_OK;
    }
    if (index->is_unique) {
        return validate_unique_index_column(database, &plan->columns[column_index]);
    }

    return validate_secondary_index_column(database, &plan->columns[column_index]);
}

static int validate_planned_unique_index_part_list(
    struct mylite_db *database,
    const struct planned_secondary_index *index
) {
    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid unique-index key parts");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reserve_planned_secondary_index_parts(
    struct mylite_db *database,
    struct planned_secondary_index *index,
    size_t required_capacity
) {
    enum { initial_part_capacity = 4 };

    struct planned_secondary_index_part *parts = NULL;
    size_t capacity = 0U;

    if (index == NULL) {
        set_runtime_error(database, "invalid secondary-index plan");
        return MYLITE_ERROR;
    }
    if (required_capacity <= index->part_capacity) {
        return MYLITE_OK;
    }
    capacity = index->part_capacity == 0U ? initial_part_capacity : index->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(index->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    index->parts = parts;
    index->part_capacity = capacity;
    return MYLITE_OK;
}

static int reserve_planned_secondary_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    enum { initial_secondary_index_capacity = 4 };

    struct planned_secondary_index *indexes = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->secondary_index_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->secondary_index_capacity == 0U ? initial_secondary_index_capacity
                                                    : plan->secondary_index_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = realloc(plan->secondary_indexes, capacity * sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->secondary_indexes = indexes;
    plan->secondary_index_capacity = capacity;
    return MYLITE_OK;
}

static int plan_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *index_name_node,
    const char *column_name,
    char *destination,
    size_t destination_size
) {
    int rc = MYLITE_OK;

    destination[0] = '\0';
    if (index_name_node == NULL) {
        return generate_secondary_index_name(
            database,
            plan,
            column_name,
            destination,
            destination_size
        );
    }

    rc = copy_identifier_text(index_name_node, destination, destination_size, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(destination, "PRIMARY")) {
        set_incorrect_index_name_error(database, destination);
        return MYLITE_ERROR;
    }
    if (planned_index_name_is_used(plan, destination)) {
        set_duplicate_key_name_error(database, destination);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int generate_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *base_name,
    char *destination,
    size_t destination_size
) {
    char candidate[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    unsigned int suffix = 1U;

    while (suffix < UINT_MAX) {
        int written = 0;

        if (suffix == 1U) {
            written = snprintf(candidate, sizeof(candidate), "%s", base_name);
        } else {
            written = snprintf(candidate, sizeof(candidate), "%s_%u", base_name, suffix);
        }
        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_identifier_too_long_error(database, "index");
            return MYLITE_ERROR;
        }
        if (!text_equals_ascii_case_insensitive(candidate, "PRIMARY") &&
            !planned_index_name_is_used(plan, candidate)) {
            int copied = snprintf(destination, destination_size, "%s", candidate);

            if (copied < 0 || (size_t)copied >= destination_size) {
                set_identifier_too_long_error(database, "index");
                return MYLITE_ERROR;
            }
            return MYLITE_OK;
        }
        ++suffix;
    }

    set_identifier_too_long_error(database, "index");
    return MYLITE_ERROR;
}

static bool planned_index_name_is_used(
    const struct planned_create_table *plan,
    const char *index_name
) {
    if (plan->has_primary_key && text_equals_ascii_case_insensitive(index_name, "PRIMARY")) {
        return true;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (text_equals_ascii_case_insensitive(plan->secondary_indexes[index].name, index_name)) {
            return true;
        }
    }

    return false;
}

static int validate_secondary_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid secondary-index column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (planned_column_is_text_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_binary_blob_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        return validate_planned_string_key_column(database, column);
    }
    if (planned_column_is_approximate(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_fulltext_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid FULLTEXT index column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        return validate_fulltext_string_key_column(database, column);
    }
    if (planned_column_is_text_family(column)) {
        return MYLITE_OK;
    }

    set_fulltext_column_error(database, column->name);
    return MYLITE_ERROR;
}

static int validate_spatial_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid SPATIAL index column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (!planned_column_is_spatial(column)) {
        set_spatial_index_non_geometric_error(database);
        return MYLITE_ERROR;
    }
    if (column->is_nullable) {
        set_spatial_index_must_be_not_null_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_fulltext_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    size_t length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || !planned_column_is_char_or_varchar(column)) {
        set_runtime_error(database, "invalid FULLTEXT string key column");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "FULLTEXT indexes support only baseline CHAR descriptors",
            &length
        );
    } else {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "FULLTEXT indexes support only baseline VARCHAR descriptors",
            &length
        );
    }
    if (rc == MYLITE_OK && length == 0U) {
        set_storage_engine_cant_index_column_error(database, column->name);
        return MYLITE_ERROR;
    }

    return rc;
}

static bool planned_secondary_index_is_fulltext(const struct planned_secondary_index *index) {
    if (index == NULL) {
        return false;
    }
    if (index->kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return true;
    }

    return false;
}

static bool planned_secondary_index_is_spatial(const struct planned_secondary_index *index) {
    if (index == NULL) {
        return false;
    }
    if (index->kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return true;
    }

    return false;
}

static int validate_secondary_index_prefix_for_planned_column(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t prefix_length,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    uint64_t prefix = prefix_length < 0 ? 0U : (uint64_t)prefix_length;
    uint64_t max_bytes_per_character = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid secondary-index prefix column");
        return MYLITE_ERROR;
    }
    *out_key_bytes = 0U;
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "prefix keys support only baseline CHAR descriptors",
            &column_length
        );
    } else if (planned_column_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "prefix keys support only baseline VARCHAR descriptors",
            &column_length
        );
    } else if (planned_column_is_text_family(column)) {
        return text_family_prefix_key_bytes(database, column->logical_type, prefix, out_key_bytes);
    } else if (planned_column_is_binary_string_family(column)) {
        return binary_string_prefix_key_bytes(
            database,
            column->logical_type,
            prefix,
            out_key_bytes
        );
    } else if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    } else {
        set_incorrect_prefix_key_error(database);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (prefix > column_length) {
        set_incorrect_prefix_key_error(database);
        return MYLITE_ERROR;
    }
    max_bytes_per_character = logical_type_max_bytes_per_character(column->logical_type);
    if (prefix > UINT64_MAX / max_bytes_per_character) {
        set_key_too_long_error(database, innodb_max_key_length_bytes);
        return MYLITE_ERROR;
    }

    *out_key_bytes = prefix * max_bytes_per_character;
    return MYLITE_OK;
}

static int text_family_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
) {
    const struct text_family_type_info *info = text_family_type_info_for_logical_type(logical_type);
    uint64_t key_bytes = 0U;

    if (info == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid text-family prefix key part");
        return MYLITE_ERROR;
    }
    if (prefix > UINT64_MAX / utf8mb4_max_bytes_per_character) {
        set_key_too_long_error(database, innodb_max_key_length_bytes);
        return MYLITE_ERROR;
    }

    key_bytes = prefix * utf8mb4_max_bytes_per_character;
    if (key_bytes > info->maximum_length) {
        set_key_too_long_error(database, info->maximum_length);
        return MYLITE_ERROR;
    }

    *out_key_bytes = key_bytes;
    return MYLITE_OK;
}

static int binary_string_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(logical_type, &storage);

    if (info == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid binary prefix key part");
        return MYLITE_ERROR;
    }
    if (prefix > info->maximum_length) {
        if (info->blob_family) {
            set_key_too_long_error(database, info->maximum_length);
        } else {
            set_incorrect_prefix_key_error(database);
        }
        return MYLITE_ERROR;
    }

    *out_key_bytes = prefix;
    return MYLITE_OK;
}

static int binary_string_full_key_bytes(
    struct mylite_db *database,
    const struct binary_string_type_info *info,
    const char *column_name,
    uint64_t *out_key_bytes
) {
    if (info == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid binary string key part");
        return MYLITE_ERROR;
    }
    if (info->blob_family) {
        set_blob_key_without_length_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (info->maximum_length == 0U) {
        set_storage_engine_cant_index_column_error(database, column_name);
        return MYLITE_ERROR;
    }

    *out_key_bytes = info->maximum_length;
    return MYLITE_OK;
}

static int planned_index_part_key_bytes(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid secondary-index key part");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "secondary indexes support only baseline CHAR descriptors",
            &column_length
        );
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (planned_column_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "secondary indexes support only baseline VARCHAR descriptors",
            &column_length
        );
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (planned_column_is_text_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_binary_string_family(column)) {
        struct binary_string_type_info storage = {0};
        const struct binary_string_type_info *info =
            binary_string_type_info_for_logical_type(column->logical_type, &storage);

        return binary_string_full_key_bytes(database, info, column->name, out_key_bytes);
    }
    if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }

    return row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "Secondary indexes do not yet support this column type",
        out_key_bytes
    );
}

static int validate_unique_index_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid unique-index column");
        return MYLITE_ERROR;
    }
    if (planned_column_is_spatial(column)) {
        set_spatial_unique_error(database);
        return MYLITE_ERROR;
    }
    if (planned_column_is_text_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_binary_blob_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        return validate_planned_string_key_column(database, column);
    }
    if (planned_column_is_approximate(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_planned_secondary_index_key_length(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index
) {
    uint64_t total = 0U;

    if (plan == NULL || index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid secondary-index descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        const struct planned_secondary_index_part *part = &index->parts[part_index];
        const struct planned_column *column = NULL;
        uint64_t part_bytes = 0U;
        int rc = MYLITE_OK;

        if (part->column_index >= plan->column_count) {
            set_runtime_error(database, "invalid secondary-index key part");
            return MYLITE_ERROR;
        }
        column = &plan->columns[part->column_index];
        if (part->has_prefix_length) {
            rc = validate_secondary_index_prefix_for_planned_column(
                database,
                column,
                part->prefix_length,
                &part_bytes
            );
        } else {
            rc = planned_index_part_key_bytes(database, column, &part_bytes);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int parse_secondary_index_part_prefix_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *prefix_node,
    const char *column_name,
    int64_t *out_prefix_length
) {
    uint64_t prefix = 0U;
    int rc = MYLITE_OK;

    if (out_prefix_length == NULL || column_name == NULL || prefix_node == NULL ||
        prefix_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(prefix_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = parse_unsigned_integer_literal(&prefix_node->span, &prefix);
    if (rc != MYLITE_OK || prefix > INT64_MAX) {
        set_key_too_long_error(database, innodb_max_key_length_bytes);
        return MYLITE_ERROR;
    }
    if (prefix == 0U) {
        set_key_part_length_cannot_be_zero_error(database, column_name);
        return MYLITE_ERROR;
    }

    *out_prefix_length = (int64_t)prefix;
    return MYLITE_OK;
}

static const char *index_part_unsupported_message(
    const struct mylite_sql_ast_node *part,
    const char *default_message
) {
    if (secondary_index_part_is_multi_valued(part)) {
        return "Multi-valued indexes are not yet supported";
    }
    if (secondary_index_part_is_functional(part)) {
        return "Functional key parts are not yet supported";
    }
    return default_message;
}

static bool secondary_index_part_is_multi_valued(const struct mylite_sql_ast_node *part) {
    const struct mylite_sql_ast_node *first_child = NULL;

    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return false;
    }

    first_child = child_at(part, 0U);
    return first_child != NULL && first_child->kind == MYLITE_SQL_AST_MULTI_VALUED_INDEX_PART;
}

static bool secondary_index_part_is_functional(const struct mylite_sql_ast_node *part) {
    const struct mylite_sql_ast_node *first_child = NULL;

    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return false;
    }

    first_child = child_at(part, 0U);
    return first_child != NULL && first_child->kind != MYLITE_SQL_AST_IDENTIFIER;
}

static const struct mylite_sql_ast_node *secondary_index_part_column_node(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL) {
        return NULL;
    }
    if (part->kind == MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return child_at(part, 0U);
    }
    return part;
}

static const struct mylite_sql_ast_node *secondary_index_part_prefix_node(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return NULL;
    }
    for (size_t child_index = 1U;; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(part, child_index);

        if (child == NULL) {
            return NULL;
        }
        if (child->kind == MYLITE_SQL_AST_LITERAL) {
            return child;
        }
    }
}

static const struct mylite_sql_ast_node *secondary_index_part_direction_node(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return NULL;
    }
    for (size_t child_index = 1U;; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(part, child_index);

        if (child == NULL) {
            return NULL;
        }
        if (child->kind == MYLITE_SQL_AST_ORDER_DIRECTION) {
            return child;
        }
    }
}

static enum mylite_catalog_index_sort_direction index_part_sort_direction(
    const struct mylite_sql_ast_node *part
) {
    if (part == NULL || part->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART) {
        return MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
    }
    for (size_t child_index = 1U;; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(part, child_index);

        if (child == NULL) {
            return MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
        }
        if (child->kind != MYLITE_SQL_AST_ORDER_DIRECTION) {
            continue;
        }
        if (mylite_sql_ast_node_order_direction(child) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
            return MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC;
        }
        return MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
    }
}

static int mark_primary_key_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index,
    enum mylite_catalog_index_sort_direction sort_direction
) {
    int rc = MYLITE_OK;

    if (plan == NULL || column_index >= plan->column_count) {
        set_runtime_error(database, "invalid primary-key column");
        return MYLITE_ERROR;
    }
    if (plan->columns[column_index].is_primary_key) {
        set_multiple_primary_key_error(database);
        return MYLITE_ERROR;
    }

    rc = validate_primary_key_column(database, &plan->columns[column_index]);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reserve_planned_primary_key_parts(database, plan, plan->primary_key_part_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    plan->has_primary_key = true;
    plan->primary_key_parts[plan->primary_key_part_count] = (struct planned_primary_key_part){
        .column_index = column_index,
        .sort_direction = sort_direction,
    };
    ++plan->primary_key_part_count;
    plan->columns[column_index].is_primary_key = true;
    plan->columns[column_index].is_nullable = false;

    return MYLITE_OK;
}

static int reserve_planned_primary_key_parts(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
) {
    enum { initial_primary_key_part_capacity = 4 };

    struct planned_primary_key_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->primary_key_part_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->primary_key_part_capacity == 0U ? initial_primary_key_part_capacity
                                                     : plan->primary_key_part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(plan->primary_key_parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->primary_key_parts = parts;
    plan->primary_key_part_capacity = capacity;
    return MYLITE_OK;
}

static bool planned_primary_key_contains_column(
    const struct planned_create_table *plan,
    size_t column_index
) {
    for (size_t part_index = 0U; part_index < plan->primary_key_part_count; ++part_index) {
        if (plan->primary_key_parts[part_index].column_index == column_index) {
            return true;
        }
    }

    return false;
}

static int validate_primary_key_column(struct mylite_db *database, struct planned_column *column) {
    if (column == NULL) {
        set_runtime_error(database, "invalid primary-key column");
        return MYLITE_ERROR;
    }
    if (column->is_generated) {
        set_unsupported_error(database, "generated columns are not yet supported in keys");
        return MYLITE_ERROR;
    }
    if (planned_column_is_spatial(column)) {
        set_spatial_unique_error(database);
        return MYLITE_ERROR;
    }
    if (planned_column_is_char_or_varchar(column)) {
        int rc = validate_planned_string_key_column(database, column);

        if (rc != MYLITE_OK) {
            return rc;
        }
    } else if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    } else if (
        planned_column_is_string_family(column) || planned_column_is_binary_string_family(column) ||
        planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column) || planned_column_is_decimal(column) ||
        planned_column_is_approximate(column) || planned_column_is_date(column) ||
        planned_column_is_time(column) || planned_column_is_datetime(column) ||
        planned_column_is_timestamp(column) || planned_column_is_year(column)
    ) {
        if (column->is_auto_increment) {
            return MYLITE_OK;
        }
        set_unsupported_error(database, "PRIMARY KEY supports only integer columns");
        return MYLITE_ERROR;
    }
    if (column->nullability == MYLITE_SQL_AST_NULLABILITY_NULL) {
        set_primary_key_part_null_error(database);
        return MYLITE_ERROR;
    }
    if (column->default_node != NULL &&
        column->default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_auto_increment_columns(
    struct mylite_db *database,
    struct planned_create_table *plan,
    bool allow_secondary_key
) {
    size_t auto_increment_count = 0U;
    size_t auto_increment_column_index = 0U;

    if (plan == NULL) {
        set_runtime_error(database, "invalid auto-increment table plan");
        return MYLITE_ERROR;
    }

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_auto_increment) {
            ++auto_increment_count;
            auto_increment_column_index = column_index;
        }
    }
    if (auto_increment_count == 0U) {
        return MYLITE_OK;
    }
    if (auto_increment_count > 1U) {
        set_wrong_auto_key_error(database);
        return MYLITE_ERROR;
    }

    return validate_auto_increment_column(
        database,
        plan,
        auto_increment_column_index,
        allow_secondary_key
    );
}

static int validate_auto_increment_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t column_index,
    bool allow_secondary_key
) {
    const struct planned_column *column = NULL;

    if (plan == NULL || column_index >= plan->column_count) {
        set_runtime_error(database, "invalid auto-increment column");
        return MYLITE_ERROR;
    }

    column = &plan->columns[column_index];
    if (planned_column_is_string_family(column) || planned_column_is_binary_string_family(column) ||
        planned_column_is_bit(column) || planned_column_is_enum(column) ||
        planned_column_is_set(column) || planned_column_is_decimal(column) ||
        planned_column_is_approximate(column) || planned_column_is_date(column) ||
        planned_column_is_time(column) || planned_column_is_datetime(column) ||
        planned_column_is_timestamp(column)) {
        set_incorrect_column_specifier_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column->default_node != NULL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (plan->has_primary_key && plan->primary_key_part_count == 1U &&
        column_index == plan->primary_key_parts[0].column_index && column->is_primary_key) {
        return MYLITE_OK;
    }
    if (allow_secondary_key && planned_secondary_index_starts_with_column(plan, column_index)) {
        return MYLITE_OK;
    }

    set_wrong_auto_key_error(database);
    return MYLITE_ERROR;
}

static bool planned_secondary_index_starts_with_column(
    const struct planned_create_table *plan,
    size_t column_index
) {
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (plan->secondary_indexes[index].part_count > 0U &&
            plan->secondary_indexes[index].parts[0].column_index == column_index) {
            return true;
        }
    }

    return false;
}

static int find_planned_column_index(
    const struct planned_column *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
) {
    *out_index = 0U;
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int plan_create_table_like(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table_like *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_table_like){0};
    out_plan->create_table.suppress_spatial_index_warnings = true;
    out_plan->create_table.pack_keys = -1;
    out_plan->create_table.stats_persistent = -1;
    out_plan->create_table.stats_auto_recalc = -1;
    rc = reject_builtin_schema_write_target(database, child_at(statement, 0U));
    if (rc == MYLITE_OK) {
        rc = resolve_visible_table_reference(
            database,
            child_at(statement, 1U),
            &out_plan->source,
            &out_plan->source_table
        );
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->create_table.default_charset,
            out_plan->source_table.default_charset,
            strlen(out_plan->source_table.default_charset) + 1U
        );
        memcpy(
            out_plan->create_table.default_collation,
            out_plan->source_table.default_collation,
            strlen(out_plan->source_table.default_collation) + 1U
        );
        memcpy(
            out_plan->create_table.comment,
            out_plan->source_table.comment,
            strlen(out_plan->source_table.comment) + 1U
        );
        memcpy(
            out_plan->create_table.row_format_option,
            out_plan->source_table.row_format_option,
            strlen(out_plan->source_table.row_format_option) + 1U
        );
        out_plan->create_table.key_block_size = out_plan->source_table.key_block_size;
        out_plan->create_table.pack_keys = out_plan->source_table.pack_keys;
        out_plan->create_table.checksum = out_plan->source_table.checksum;
        out_plan->create_table.stats_persistent = out_plan->source_table.stats_persistent;
        out_plan->create_table.stats_auto_recalc = out_plan->source_table.stats_auto_recalc;
        out_plan->create_table.stats_sample_pages = out_plan->source_table.stats_sample_pages;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->create_table.target);
    }
    if (rc == MYLITE_OK &&
        mylite_catalog_name_is_reserved(out_plan->create_table.target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->create_table.target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = clone_create_table_like_columns(
            database,
            out_plan->source_table.table_id,
            &out_plan->create_table
        );
    }
    if (rc != MYLITE_OK) {
        planned_create_table_like_deinit(out_plan);
    }

    return rc;
}

static int clone_create_table_like_columns(
    struct mylite_db *database,
    int64_t source_table_id,
    struct planned_create_table *out_plan
) {
    struct mylite_catalog_column_descriptor *source_columns = NULL;
    struct primary_key_info primary_key = primary_key_info_init();
    size_t source_column_count = 0U;
    int rc = load_table_columns(database, source_table_id, &source_columns, &source_column_count);

    if (rc == MYLITE_OK) {
        rc = validate_create_table_like_source_columns(
            database,
            source_columns,
            source_column_count
        );
    }
    if (rc == MYLITE_OK && source_column_count > SIZE_MAX / sizeof(*out_plan->columns)) {
        set_nomem_error(database);
        rc = MYLITE_NOMEM;
    }
    if (rc == MYLITE_OK) {
        out_plan->columns = calloc(source_column_count, sizeof(*out_plan->columns));
        if (out_plan->columns == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->column_count = source_column_count;
        out_plan->auto_increment_next = 1;
        for (size_t column_index = 0U; column_index < source_column_count; ++column_index) {
            planned_column_from_catalog_descriptor(
                &source_columns[column_index],
                NULL,
                &out_plan->columns[column_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            source_table_id,
            source_columns,
            source_column_count,
            &primary_key
        );
    }
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        out_plan->has_primary_key = true;
        for (size_t part_index = 0U; rc == MYLITE_OK && part_index < primary_key.part_count;
             ++part_index) {
            size_t column_index = primary_key.parts[part_index].column_index;

            rc = reserve_planned_primary_key_parts(
                database,
                out_plan,
                out_plan->primary_key_part_count + 1U
            );
            if (rc == MYLITE_OK) {
                out_plan->primary_key_parts[out_plan->primary_key_part_count] =
                    (struct planned_primary_key_part){
                        .column_index = column_index,
                        .sort_direction = primary_key.parts[part_index].index_column.sort_direction,
                    };
                ++out_plan->primary_key_part_count;
                out_plan->columns[column_index].is_primary_key = true;
                out_plan->columns[column_index].is_nullable = false;
            }
            out_plan->primary_key_show_create_explicit_btree =
                primary_key.index.show_create_explicit_btree;
            snprintf(
                out_plan->primary_key_comment,
                sizeof(out_plan->primary_key_comment),
                "%s",
                primary_key.index.comment
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = clone_create_table_like_indexes(
            database,
            source_table_id,
            source_columns,
            source_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = clone_create_table_like_check_constraints(database, source_table_id, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_table_auto_increment_columns(database, out_plan, true);
    }

    primary_key_info_deinit(&primary_key);
    free(source_columns);
    return rc;
}

static int validate_create_table_like_source_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        struct integer_column_range range = {0};
        int rc = MYLITE_OK;

        if (column_descriptor_is_string_family(&columns[column_index]) ||
            column_descriptor_is_enum(&columns[column_index]) ||
            column_descriptor_is_set(&columns[column_index]) ||
            column_descriptor_is_binary_string_family(&columns[column_index]) ||
            column_descriptor_is_bit(&columns[column_index]) ||
            column_descriptor_is_decimal(&columns[column_index]) ||
            column_descriptor_is_approximate(&columns[column_index]) ||
            column_descriptor_is_year(&columns[column_index]) ||
            column_descriptor_is_date(&columns[column_index]) ||
            column_descriptor_is_time(&columns[column_index]) ||
            column_descriptor_is_datetime(&columns[column_index]) ||
            column_descriptor_is_timestamp(&columns[column_index]) ||
            column_descriptor_is_spatial(&columns[column_index])) {
            continue;
        }
        if (columns[column_index].physical_type[0] == '\0' ||
            strcmp(columns[column_index].physical_type, "INTEGER") != 0) {
            set_unsupported_error(
                database,
                "CREATE TABLE LIKE supports only integer, BIT, string, binary string, decimal, "
                "approximate numeric, YEAR, DATE, TIME, DATETIME, TIMESTAMP, and spatial "
                "descriptor columns"
            );
            return MYLITE_ERROR;
        }
        rc = integer_range_for_column(
            database,
            &columns[column_index],
            "CREATE TABLE LIKE supports only integer, BIT, string, binary string, decimal, "
            "approximate numeric, YEAR, DATE, TIME, DATETIME, TIMESTAMP, and spatial descriptor "
            "columns",
            &range
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int clone_create_table_like_indexes(
    struct mylite_db *database,
    int64_t source_table_id,
    const struct mylite_catalog_column_descriptor *source_columns,
    size_t source_column_count,
    struct planned_create_table *out_plan
) {
    struct loaded_index_info *indexes = NULL;
    size_t index_count = 0U;
    int rc = load_table_index_infos(
        database,
        source_table_id,
        source_columns,
        source_column_count,
        &indexes,
        &index_count
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        const struct loaded_index_info *source_index = &indexes[index];
        struct planned_secondary_index *target_index = NULL;

        if (source_index->index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
            source_index->index.kind != MYLITE_CATALOG_INDEX_KIND_FULLTEXT &&
            source_index->index.kind != MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            continue;
        }
        rc = reserve_planned_secondary_indexes(
            database,
            out_plan,
            out_plan->secondary_index_count + 1U
        );
        if (rc != MYLITE_OK) {
            break;
        }

        target_index = &out_plan->secondary_indexes[out_plan->secondary_index_count];
        *target_index = (struct planned_secondary_index){0};
        snprintf(target_index->name, sizeof(target_index->name), "%s", source_index->index.name);
        target_index->kind = source_index->index.kind;
        target_index->is_unique = source_index->index.is_unique;
        target_index->is_visible = source_index->index.is_visible;
        target_index->show_create_explicit_btree = source_index->index.show_create_explicit_btree;
        snprintf(
            target_index->comment,
            sizeof(target_index->comment),
            "%s",
            source_index->index.comment
        );
        rc =
            reserve_planned_secondary_index_parts(database, target_index, source_index->part_count);
        for (size_t part_index = 0U; rc == MYLITE_OK && part_index < source_index->part_count;
             ++part_index) {
            const struct loaded_index_part *source_part = &source_index->parts[part_index];

            target_index->parts[target_index->part_count] = (struct planned_secondary_index_part){
                .column_index = source_part->column_index,
                .has_prefix_length = source_part->index_column.has_prefix_length,
                .prefix_length = source_part->index_column.prefix_length,
                .sort_direction = source_part->index_column.sort_direction,
            };
            ++target_index->part_count;
        }
        if (rc != MYLITE_OK) {
            free(target_index->parts);
            *target_index = (struct planned_secondary_index){0};
            break;
        }
        ++out_plan->secondary_index_count;
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    return rc;
}

static int clone_create_table_like_check_constraints(
    struct mylite_db *database,
    int64_t source_table_id,
    struct planned_create_table *out_plan
) {
    struct loaded_check_constraint_info *checks = NULL;
    size_t check_count = 0U;
    int rc = load_table_check_constraint_infos(database, source_table_id, &checks, &check_count);

    for (size_t index = 0U; rc == MYLITE_OK && index < check_count; ++index) {
        const struct mylite_catalog_check_constraint_descriptor *source =
            &checks[index].check_constraint;
        struct planned_check_constraint target = {
            .generated_ordinal = (int64_t)out_plan->check_constraint_count + 1,
            .ordinal_position = (int64_t)out_plan->check_constraint_count + 1,
            .is_enforced = source->is_enforced,
            .name_is_generated = true,
        };

        rc = generate_create_table_check_constraint_name(
            database,
            out_plan,
            target.generated_ordinal,
            target.name,
            sizeof(target.name)
        );
        if (rc == MYLITE_OK) {
            rc = reject_duplicate_schema_check_constraint_name(database, out_plan, target.name);
        }
        if (rc == MYLITE_OK) {
            snprintf(target.check_clause, sizeof(target.check_clause), "%s", source->check_clause);
            snprintf(
                target.sqlite_expression,
                sizeof(target.sqlite_expression),
                "%s",
                source->sqlite_expression
            );
            rc = reserve_planned_check_constraints(
                database,
                out_plan,
                out_plan->check_constraint_count + 1U
            );
        }
        if (rc == MYLITE_OK) {
            out_plan->check_constraints[out_plan->check_constraint_count] = target;
            ++out_plan->check_constraint_count;
        }
    }

    loaded_check_constraint_infos_deinit(&checks, &check_count);
    return rc;
}

static void planned_create_table_like_deinit(struct planned_create_table_like *plan) {
    if (plan == NULL) {
        return;
    }

    planned_create_table_deinit(&plan->create_table);
    *plan = (struct planned_create_table_like){0};
}

static int plan_create_table_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table_select *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_table_select){0};
    out_plan->create_table.pack_keys = -1;
    out_plan->create_table.stats_persistent = -1;
    out_plan->create_table.stats_auto_recalc = -1;
    memcpy(
        out_plan->create_table.default_charset,
        MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_CHARSET)
    );
    memcpy(
        out_plan->create_table.default_collation,
        MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_COLLATION)
    );
    rc = reject_builtin_schema_write_target(database, child_at(statement, 0U));
    if (rc == MYLITE_OK) {
        rc = plan_select(database, child_at(statement, 1U), false, &out_plan->source);
    }
    if (rc == MYLITE_OK) {
        out_plan->source_locking_clause =
            mylite_sql_ast_node_select_locking_clause(child_at(statement, 1U));
    }
    if (rc == MYLITE_OK && out_plan->source.calc_found_rows) {
        set_unsupported_error(
            database,
            "CREATE TABLE ... SELECT does not support SQL_CALC_FOUND_ROWS"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && out_plan->source.source_count > 0U) {
        set_unsupported_error(database, "CREATE TABLE ... SELECT does not support joined SELECT");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->create_table.target);
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->create_table.default_charset,
            out_plan->create_table.target.schema.default_charset,
            strlen(out_plan->create_table.target.schema.default_charset) + 1U
        );
        memcpy(
            out_plan->create_table.default_collation,
            out_plan->create_table.target.schema.default_collation,
            strlen(out_plan->create_table.target.schema.default_collation) + 1U
        );
    }
    if (rc == MYLITE_OK &&
        mylite_catalog_name_is_reserved(out_plan->create_table.target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->create_table.target.table_name);
        rc = MYLITE_ERROR;
    }
    return rc;
}

static void planned_create_table_select_deinit(struct planned_create_table_select *plan) {
    if (plan == NULL) {
        return;
    }

    planned_create_table_deinit(&plan->create_table);
    planned_select_deinit(&plan->source);
    *plan = (struct planned_create_table_select){0};
}

static int create_table_select_from_plan(
    struct mylite_db *database,
    struct planned_create_table_select *plan,
    int64_t *out_affected_rows
) {
    struct mylite_catalog_table_descriptor existing_table = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    bool existing_table_found = false;
    int64_t table_id = 0;
    int rc = MYLITE_OK;

    *out_affected_rows = 0;
    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->create_table.target.schema.schema_id,
        plan->create_table.target.table_name,
        &existing_table,
        &existing_table_found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (existing_table_found) {
        set_table_exists_error(database, plan->create_table.target.table_name);
        return MYLITE_ERROR;
    }
    if (plan->source_locking_clause != MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        set_create_table_select_locking_clause_error(
            database,
            plan->source.source.table_name,
            plan->create_table.target.table_name
        );
        return MYLITE_ERROR;
    }

    rc = infer_create_table_select_columns(database, plan);
    if (rc == MYLITE_OK) {
        rc = check_duplicate_column_names(
            database,
            plan->create_table.columns,
            plan->create_table.column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_table_select_rows(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_allocate_table_id_in_mutation(database, &mutation, &table_id);
    }
    if (rc == MYLITE_OK) {
        rc = build_physical_table_name(table_id, physical_name, sizeof(physical_name));
    }
    if (rc == MYLITE_OK) {
        rc = insert_create_table_catalog_rows(
            database,
            &plan->create_table,
            &mutation,
            table_id,
            physical_name,
            &table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_create_table(
            database,
            &plan->create_table,
            table.physical_name,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_create_table_select_copy(
            database,
            plan,
            table.physical_name,
            out_affected_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int create_temporary_table_select_from_plan(
    struct mylite_db *database,
    struct planned_create_table_select *plan,
    int64_t *out_affected_rows
) {
    struct mylite_temporary_catalog_table temporary_table = {0};
    struct mylite_catalog_table_descriptor existing_table = {0};
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    bool existing_table_found = false;
    bool physical_table_created = false;
    int64_t table_id = 0;
    int rc = MYLITE_OK;

    *out_affected_rows = 0;
    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        plan->create_table.target.schema.name,
        plan->create_table.target.table_name,
        &existing_table,
        &existing_table_found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (existing_table_found) {
        set_table_exists_error(database, plan->create_table.target.table_name);
        return MYLITE_ERROR;
    }
    if (plan->source_locking_clause != MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        set_create_table_select_locking_clause_error(
            database,
            plan->source.source.table_name,
            plan->create_table.target.table_name
        );
        return MYLITE_ERROR;
    }

    rc = infer_create_table_select_columns(database, plan);
    if (rc == MYLITE_OK) {
        rc = check_duplicate_column_names(
            database,
            plan->create_table.columns,
            plan->create_table.column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_table_select_rows(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_temporary_catalog_allocate_table_identity(
            &database->session.temporary_catalog,
            &table_id,
            physical_name,
            sizeof(physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_temporary_table_descriptors(
            database,
            &plan->create_table,
            table_id,
            physical_name,
            &temporary_table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_create_table(database, &plan->create_table, physical_name, true);
        physical_table_created = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = execute_create_table_select_copy(database, plan, physical_name, out_affected_rows);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_temporary_catalog_append_table(
            &database->session.temporary_catalog,
            &temporary_table
        );
    }
    if (rc != MYLITE_OK) {
        if (physical_table_created) {
            (void)execute_physical_drop_table(database, physical_name);
        }
        mylite_temporary_catalog_table_deinit(&temporary_table);
        set_internal_error_if_clear(database, rc, "failed to create temporary table from SELECT");
        return rc;
    }

    ++database->session.sqlite_schema_generation;
    return MYLITE_OK;
}

static int infer_create_table_select_columns(
    struct mylite_db *database,
    struct planned_create_table_select *plan
) {
    int rc = validate_create_table_select_source_columns(
        database,
        plan->source.columns,
        plan->source.column_count
    );

    if (rc == MYLITE_OK &&
        plan->source.column_count > SIZE_MAX / sizeof(*plan->create_table.columns)) {
        set_nomem_error(database);
        rc = MYLITE_NOMEM;
    }
    if (rc == MYLITE_OK) {
        plan->create_table.columns =
            calloc(plan->source.column_count, sizeof(*plan->create_table.columns));
        if (plan->create_table.columns == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        plan->create_table.column_count = plan->source.column_count;
        plan->create_table.auto_increment_next = 1;
        for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->source.column_count;
             ++column_index) {
            struct planned_column *column = &plan->create_table.columns[column_index];

            planned_column_from_catalog_descriptor(
                &plan->source.columns[column_index],
                NULL,
                column
            );
            copy_create_table_select_column_character_metadata(
                &plan->source.table,
                &plan->source.columns[column_index],
                &plan->create_table,
                column
            );
            column->is_visible = true;
            rc =
                copy_create_table_select_column_name(database, &plan->source, column_index, column);
        }
    }

    return rc;
}

static int validate_create_table_select_source_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        struct integer_column_range range = {0};
        int rc = MYLITE_OK;

        if (column_descriptor_is_string_family(&columns[column_index]) ||
            column_descriptor_is_json(&columns[column_index]) ||
            column_descriptor_is_binary_string_family(&columns[column_index]) ||
            column_descriptor_is_bit(&columns[column_index]) ||
            column_descriptor_is_decimal(&columns[column_index]) ||
            column_descriptor_is_approximate(&columns[column_index]) ||
            column_descriptor_is_year(&columns[column_index]) ||
            column_descriptor_is_date(&columns[column_index]) ||
            column_descriptor_is_time(&columns[column_index]) ||
            column_descriptor_is_datetime(&columns[column_index]) ||
            column_descriptor_is_timestamp(&columns[column_index])) {
            continue;
        }
        if (columns[column_index].physical_type[0] == '\0' ||
            strcmp(columns[column_index].physical_type, "INTEGER") != 0) {
            set_unsupported_error(
                database,
                "CREATE TABLE SELECT supports only integer, BIT, string, JSON, binary string, "
                "decimal, "
                "approximate numeric, YEAR, DATE, TIME, DATETIME, and TIMESTAMP descriptor columns"
            );
            return MYLITE_ERROR;
        }
        rc = integer_range_for_column(
            database,
            &columns[column_index],
            "CREATE TABLE SELECT supports only integer, BIT, string, JSON, binary string, decimal, "
            "approximate numeric, YEAR, DATE, TIME, DATETIME, and TIMESTAMP descriptor columns",
            &range
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static void copy_create_table_select_column_character_metadata(
    const struct mylite_catalog_table_descriptor *source_table,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct planned_create_table *target_table,
    struct planned_column *out_column
) {
    const char *character_set_name =
        column_effective_character_set_name(source_table, source_column);
    const char *collation_name = column_effective_collation_name(source_table, source_column);

    if (source_column->character_set_name[0] != '\0' || source_column->collation_name[0] != '\0') {
        return;
    }
    if (character_set_name != NULL &&
        strcmp(character_set_name, target_table->default_charset) != 0) {
        snprintf(
            out_column->character_set_name,
            sizeof(out_column->character_set_name),
            "%s",
            character_set_name
        );
    }
    if (collation_name != NULL && strcmp(collation_name, target_table->default_collation) != 0) {
        snprintf(
            out_column->collation_name,
            sizeof(out_column->collation_name),
            "%s",
            collation_name
        );
    }
}

static int copy_create_table_select_column_name(
    struct mylite_db *database,
    const struct planned_select *source,
    size_t column_index,
    struct planned_column *out_column
) {
    const struct mylite_sql_ast_node *alias = source->column_aliases[column_index];
    char *alias_text = NULL;
    int rc = MYLITE_OK;

    if (alias == NULL) {
        return MYLITE_OK;
    }

    rc = copy_select_item_alias_text(database, alias, &alias_text);
    if (rc == MYLITE_OK && alias_text[0] == '\0') {
        set_reserved_name_error(database, "column", alias_text);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && strlen(alias_text) >= sizeof(out_column->name)) {
        set_identifier_too_long_error(database, "column");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(alias_text)) {
        set_reserved_name_error(database, "column", alias_text);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        snprintf(out_column->name, sizeof(out_column->name), "%s", alias_text);
    }
    free(alias_text);

    return rc;
}

static int execute_create_table_select_copy(
    struct mylite_db *database,
    const struct planned_create_table_select *plan,
    const char *physical_name,
    int64_t *out_affected_rows
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_create_table_select_insert_sql(plan, physical_name, &sql);

    *out_affected_rows = 0;
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, &plan->source);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            *out_affected_rows = sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_select_rows(
    struct mylite_db *database,
    const struct planned_create_table_select *plan
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = build_select_sql(&plan->source, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, &plan->source);
    }
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (row_number == SIZE_MAX) {
            set_unsupported_error(database, "CREATE TABLE ... SELECT selected too many rows");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            ++row_number;
            rc = validate_create_table_select_row(database, statement, plan, row_number);
        }
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_select_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_create_table_select *plan,
    size_t row_number
) {
    if (plan->create_table.column_count > (size_t)INT_MAX) {
        set_unsupported_error(database, "CREATE TABLE ... SELECT selected too many columns");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; column_index < plan->create_table.column_count; ++column_index) {
        struct mylite_catalog_column_descriptor column = {0};
        const struct planned_column *planned_column = &plan->create_table.columns[column_index];
        int rc = MYLITE_OK;

        snprintf(column.name, sizeof(column.name), "%s", planned_column->name);
        snprintf(
            column.logical_type,
            sizeof(column.logical_type),
            "%s",
            planned_column->logical_type
        );
        snprintf(
            column.physical_type,
            sizeof(column.physical_type),
            "%s",
            planned_column->physical_type
        );
        column.is_nullable = planned_column->is_nullable;
        rc = validate_insert_select_value(
            database,
            statement,
            (int)column_index,
            &column,
            &column,
            row_number,
            false
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_create_table_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    const char *table_name
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    bool has_charset = false;
    bool has_collation = false;
    int rc = MYLITE_OK;

    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = copy_table_charset_option_name(
                database,
                table_option,
                charset_name,
                sizeof(charset_name)
            );
            has_charset = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = copy_table_collation_option_name(
                database,
                table_option,
                collation_name,
                sizeof(collation_name)
            );
            has_collation = true;
        } else {
            rc = validate_create_table_option(database, table_option, table_name);
        }
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values_with_binary(
            database,
            has_charset,
            charset_name,
            has_collation,
            collation_name,
            true
        );
    }

    return rc;
}

static int validate_alter_table_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    bool has_charset = false;
    bool has_collation = false;
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = copy_table_charset_option_name(
                database,
                table_option,
                charset_name,
                sizeof(charset_name)
            );
            has_charset = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = copy_table_collation_option_name(
                database,
                table_option,
                collation_name,
                sizeof(collation_name)
            );
            has_collation = true;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values(
            database,
            has_charset,
            charset_name,
            has_collation,
            collation_name
        );
    }

    return rc;
}

static int validate_schema_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_options
) {
    const struct mylite_sql_ast_node *schema_option = NULL;
    struct schema_default_option_validation validation = {0};
    int rc = MYLITE_OK;

    if (schema_options == NULL) {
        return MYLITE_OK;
    }
    if (schema_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    schema_option = child_at(schema_options, 0U);
    while (rc == MYLITE_OK && schema_option != NULL) {
        if (schema_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = track_schema_default_charset_option(database, schema_option, &validation);
        } else if (schema_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = track_schema_default_collation_option(database, schema_option, &validation);
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        schema_option = schema_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values_with_binary(
            database,
            validation.has_charset,
            validation.charset_name,
            validation.has_collation,
            validation.collation_name,
            true
        );
    }
    if (rc == MYLITE_OK && validation.unsupported_charset_name[0] != '\0') {
        set_unknown_character_set_error(database, validation.unsupported_charset_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && validation.unsupported_collation_name[0] != '\0') {
        set_unknown_collation_error(database, validation.unsupported_collation_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int track_schema_default_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    struct schema_default_option_validation *validation
) {
    char normalized_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    int rc = copy_table_charset_option_name(
        database,
        schema_option,
        validation->charset_name,
        sizeof(validation->charset_name)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (charset_name_is_binary(validation->charset_name)) {
        memcpy(normalized_charset, "binary", sizeof("binary"));
    } else if (character_set_by_name(validation->charset_name) != NULL) {
        const struct mylite_execution_catalog_character_set *charset =
            character_set_by_name(validation->charset_name);

        if (strlen(charset->name) >= sizeof(normalized_charset)) {
            set_runtime_error(database, "schema charset descriptor is too small");
            return MYLITE_ERROR;
        }
        memcpy(normalized_charset, charset->name, strlen(charset->name) + 1U);
    } else if (validation->unsupported_charset_name[0] == '\0') {
        memcpy(
            validation->unsupported_charset_name,
            validation->charset_name,
            strlen(validation->charset_name) + 1U
        );
    }

    if (normalized_charset[0] != '\0' && !validation->has_charset) {
        memcpy(validation->first_charset_name, normalized_charset, strlen(normalized_charset) + 1U);
    } else if (
        normalized_charset[0] != '\0' && validation->first_charset_name[0] != '\0' &&
        strcmp(validation->first_charset_name, normalized_charset) != 0
    ) {
        set_conflicting_character_set_declarations_error(
            database,
            validation->first_charset_name,
            normalized_charset
        );
        return MYLITE_ERROR;
    }
    validation->has_charset = true;

    return MYLITE_OK;
}

static int track_schema_default_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    struct schema_default_option_validation *validation
) {
    int rc = copy_table_collation_option_name(
        database,
        schema_option,
        validation->collation_name,
        sizeof(validation->collation_name)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (collation_by_name(validation->collation_name) == NULL &&
        validation->unsupported_collation_name[0] == '\0') {
        memcpy(
            validation->unsupported_collation_name,
            validation->collation_name,
            strlen(validation->collation_name) + 1U
        );
    }
    validation->has_collation = true;

    return MYLITE_OK;
}

static int validate_create_table_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    const char *table_name
) {
    if (table_option == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (table_option->kind == MYLITE_SQL_AST_TABLE_ENGINE_OPTION) {
        return validate_create_table_engine_option(database, table_option, table_name);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
        return validate_create_table_charset_option(database, table_option, true);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        return validate_create_table_collation_option(database, table_option, true);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION) {
        return MYLITE_OK;
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_COMMENT_OPTION) {
        return validate_create_table_comment_option(database, table_option);
    }
    if (create_table_storage_statistics_option_is_valid(table_option)) {
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_create_table_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    bool has_charset_option = false;
    bool has_collation_option = false;

    if (plan == NULL) {
        set_runtime_error(database, "invalid charset/collation table plan");
        return MYLITE_ERROR;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            has_charset_option = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            has_collation_option = true;
        }
        table_option = table_option->next_sibling;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        int rc = MYLITE_OK;

        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = apply_table_charset_option(
                database,
                table_option,
                true,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = apply_table_collation_option(
                database,
                table_option,
                true,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        table_option = table_option->next_sibling;
    }
    if (has_charset_option && !has_collation_option) {
        int rc = copy_default_collation_for_charset(
            database,
            plan->default_charset,
            plan->default_collation,
            sizeof(plan->default_collation)
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (!has_charset_option && has_collation_option) {
        const struct mylite_execution_catalog_collation *collation =
            collation_by_name(plan->default_collation);

        if (collation == NULL) {
            set_unknown_collation_error(database, plan->default_collation);
            return MYLITE_ERROR;
        }
        if (strlen(collation->character_set) >= sizeof(plan->default_charset)) {
            set_runtime_error(database, "table charset descriptor is too small");
            return MYLITE_ERROR;
        }
        memcpy(
            plan->default_charset,
            collation->character_set,
            strlen(collation->character_set) + 1U
        );
    }

    return MYLITE_OK;
}

static int apply_schema_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_options,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
) {
    const struct mylite_sql_ast_node *schema_option = NULL;
    bool has_collation_option = false;
    int rc = validate_schema_default_charset_collation_options(database, schema_options);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (schema_options == NULL) {
        return MYLITE_OK;
    }
    if (schema_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    schema_option = child_at(schema_options, 0U);
    while (rc == MYLITE_OK && schema_option != NULL) {
        if (schema_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = apply_schema_default_charset_option(
                database,
                schema_option,
                has_collation_option,
                default_charset,
                default_charset_size,
                default_collation,
                default_collation_size
            );
        } else if (schema_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = apply_schema_default_collation_option(
                database,
                schema_option,
                default_charset,
                default_charset_size,
                default_collation,
                default_collation_size
            );
            has_collation_option = true;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        schema_option = schema_option->next_sibling;
    }

    return rc;
}

static int apply_schema_default_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    bool has_collation_option,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    int rc =
        copy_table_charset_option_name(database, schema_option, charset_name, sizeof(charset_name));

    if (rc == MYLITE_OK) {
        rc = copy_normalized_charset_name(
            database,
            charset_name,
            default_charset,
            default_charset_size
        );
    }
    if (rc == MYLITE_OK && !has_collation_option) {
        rc = copy_default_collation_for_charset(
            database,
            default_charset,
            default_collation,
            default_collation_size
        );
    }

    return rc;
}

static int apply_schema_default_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    const struct mylite_execution_catalog_collation *collation = NULL;
    int rc = copy_table_collation_option_name(
        database,
        schema_option,
        collation_name,
        sizeof(collation_name)
    );

    if (rc == MYLITE_OK) {
        collation = collation_by_name(collation_name);
        if (collation == NULL) {
            set_unknown_collation_error(database, collation_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = copy_normalized_charset_name(
            database,
            collation->character_set,
            default_charset,
            default_charset_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_normalized_collation_name(
            database,
            collation_name,
            default_collation,
            default_collation_size
        );
    }

    return rc;
}

static int apply_create_table_auto_increment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const uint64_t sqlite_int64_positive_max = 9223372036854775807ULL;
    const struct mylite_sql_ast_node *table_option = NULL;

    if (plan == NULL) {
        set_runtime_error(database, "invalid auto-increment table plan");
        return MYLITE_ERROR;
    }
    if (plan->auto_increment_next <= 0) {
        plan->auto_increment_next = 1;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION) {
            const struct mylite_sql_ast_node *literal = child_at(table_option, 0U);
            uint64_t magnitude = 0U;
            int rc = MYLITE_OK;

            if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
                mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
            if (rc != MYLITE_OK || magnitude > sqlite_int64_positive_max) {
                set_unsupported_error(
                    database,
                    "AUTO_INCREMENT table option supports only signed 64-bit nonnegative values"
                );
                return MYLITE_ERROR;
            }
            plan->auto_increment_next = magnitude == 0U ? 1 : (int64_t)magnitude;
        }
        table_option = table_option->next_sibling;
    }

    return MYLITE_OK;
}

static int apply_create_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;

    if (plan == NULL) {
        set_runtime_error(database, "invalid table comment plan");
        return MYLITE_ERROR;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_COMMENT_OPTION) {
            int rc = decode_table_comment_option(
                database,
                table_option,
                plan->target.table_name,
                plan->comment,
                sizeof(plan->comment)
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        table_option = table_option->next_sibling;
    }

    return MYLITE_OK;
}

static int apply_create_table_storage_statistics_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    int rc = MYLITE_OK;

    if (plan == NULL) {
        set_runtime_error(database, "invalid storage/statistics table plan");
        return MYLITE_ERROR;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        rc = apply_table_storage_statistics_option(database, table_option, plan);
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_table_storage_statistics_combination(database, plan);
    }

    return rc;
}

static int apply_table_storage_statistics_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    if (table_option == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION) {
        return apply_table_row_format_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION) {
        return apply_table_key_block_size_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION) {
        return apply_table_pack_keys_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION) {
        return apply_table_checksum_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION) {
        return apply_table_stats_persistent_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION) {
        return apply_table_stats_auto_recalc_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION) {
        return apply_table_stats_sample_pages_option(database, table_option, plan);
    }

    return MYLITE_OK;
}

static int apply_table_row_format_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    char row_format_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    int rc = copy_table_row_format_option_name(
        database,
        table_option,
        row_format_name,
        sizeof(row_format_name)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "DEFAULT")) {
        plan->row_format_option[0] = '\0';
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "FIXED")) {
        set_table_storage_engine_option_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "DYNAMIC")) {
        memcpy(plan->row_format_option, "DYNAMIC", sizeof("DYNAMIC"));
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "COMPACT")) {
        memcpy(plan->row_format_option, "COMPACT", sizeof("COMPACT"));
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "REDUNDANT")) {
        memcpy(plan->row_format_option, "REDUNDANT", sizeof("REDUNDANT"));
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "COMPRESSED")) {
        memcpy(plan->row_format_option, "COMPRESSED", sizeof("COMPRESSED"));
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_table_key_block_size_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    uint64_t value = 0U;
    int rc = parse_table_option_unsigned_integer(database, child_at(table_option, 0U), &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != 0U && value != 1U && value != 2U && value != 4U &&
        value != table_key_block_size_eight && value != table_key_block_size_sixteen) {
        set_table_storage_engine_option_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    plan->key_block_size = (int64_t)value;
    return MYLITE_OK;
}

static int apply_table_pack_keys_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    int64_t value = -1;
    int rc =
        parse_table_option_default_or_integer(database, child_at(table_option, 0U), -1, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != -1 && value != 0 && value != 1) {
        set_unsupported_error(database, "PACK_KEYS supports only DEFAULT, 0, or 1");
        return MYLITE_ERROR;
    }
    plan->pack_keys = value;
    return MYLITE_OK;
}

static int apply_table_checksum_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    uint64_t value = 0U;
    int rc = parse_table_option_unsigned_integer(database, child_at(table_option, 0U), &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    plan->checksum = value == 0U ? 0 : 1;
    return MYLITE_OK;
}

static int apply_table_stats_persistent_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    int64_t value = -1;
    int rc =
        parse_table_option_default_or_integer(database, child_at(table_option, 0U), -1, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != -1 && value != 0 && value != 1) {
        set_unsupported_error(database, "STATS_PERSISTENT supports only DEFAULT, 0, or 1");
        return MYLITE_ERROR;
    }
    plan->stats_persistent = value;
    return MYLITE_OK;
}

static int apply_table_stats_auto_recalc_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    int64_t value = -1;
    int rc =
        parse_table_option_default_or_integer(database, child_at(table_option, 0U), -1, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != -1 && value != 0 && value != 1) {
        set_unsupported_error(database, "STATS_AUTO_RECALC supports only DEFAULT, 0, or 1");
        return MYLITE_ERROR;
    }
    plan->stats_auto_recalc = value;
    return MYLITE_OK;
}

static int apply_table_stats_sample_pages_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (table_option_value_is_default(child_at(table_option, 0U))) {
        plan->stats_sample_pages = 0;
        return MYLITE_OK;
    }
    rc = parse_table_option_unsigned_integer(database, child_at(table_option, 0U), &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value == 0U || value > table_stats_sample_pages_max) {
        set_unsupported_error(
            database,
            "STATS_SAMPLE_PAGES value is outside the valid range for stats_sample_pages"
        );
        return MYLITE_ERROR;
    }
    plan->stats_sample_pages = (int64_t)value;
    return MYLITE_OK;
}

static int copy_table_row_format_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    char *row_format_name,
    size_t row_format_name_size
) {
    if (table_option == NULL || table_option->kind != MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_identifier_text(
        child_at(table_option, 0U),
        row_format_name,
        row_format_name_size,
        database
    );
}

static int parse_table_option_unsigned_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    if (out_value == NULL || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&value_node->span, out_value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "table option supports only unsigned decimal integer values"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int parse_table_option_default_or_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t default_value,
    int64_t *out_value
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (table_option_value_is_default(value_node)) {
        *out_value = default_value;
        return MYLITE_OK;
    }
    rc = parse_table_option_unsigned_integer(database, value_node, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value > (uint64_t)INT64_MAX) {
        set_unsupported_error(database, "table option integer value is out of range");
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)value;
    return MYLITE_OK;
}

static bool table_option_value_is_default(const struct mylite_sql_ast_node *value_node) {
    static const char default_text[] = "DEFAULT";

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_IDENTIFIER ||
        value_node->span.text == NULL || value_node->span.length != sizeof(default_text) - 1U) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(default_text) - 1U; ++index) {
        char byte = value_node->span.text[index];
        if (byte >= 'a' && byte <= 'z') {
            byte = (char)(byte - ('a' - 'A'));
        }
        if (byte != default_text[index]) {
            return false;
        }
    }
    return true;
}

static bool create_table_storage_statistics_option_is_valid(
    const struct mylite_sql_ast_node *table_option
) {
    if (table_option == NULL) {
        return false;
    }
    switch (table_option->kind) {
    case MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION:
    case MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION:
    case MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION:
    case MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION:
        return true;
    default:
        return false;
    }
}

static int validate_create_table_storage_statistics_combination(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (plan->key_block_size > 0 && plan->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(plan->row_format_option, "COMPRESSED")) {
        set_table_storage_engine_option_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int decode_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *comment_option,
    const char *table_name,
    char *destination,
    size_t destination_size
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    size_t decoded_character_count = 0U;
    int rc = MYLITE_OK;

    if (comment_option == NULL || comment_option->kind != MYLITE_SQL_AST_TABLE_COMMENT_OPTION ||
        destination == NULL || destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(
        database,
        child_at(comment_option, 0U),
        &decoded,
        (struct table_option_name_policy){
            .identifier_kind = "table comment",
            .nul_message = "table comments do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    decoded_length = strlen(decoded);
    if (validate_utf8_text(decoded, decoded_length, &decoded_character_count) != MYLITE_OK) {
        free(decoded);
        set_unsupported_error(database, "table comments support only valid UTF-8 text");
        return MYLITE_ERROR;
    }
    if (decoded_character_count > MYLITE_CATALOG_TABLE_COMMENT_MAX_CHARACTERS ||
        decoded_length >= destination_size) {
        free(decoded);
        set_table_comment_too_long_error(database, table_name);
        return MYLITE_ERROR;
    }

    memcpy(destination, decoded, decoded_length + 1U);
    free(decoded);
    return MYLITE_OK;
}

static int validate_create_table_engine_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *engine_option,
    const char *table_name
) {
    char engine_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (engine_option == NULL) {
        return MYLITE_OK;
    }
    if (engine_option->kind != MYLITE_SQL_AST_TABLE_ENGINE_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(engine_option, 0U),
        engine_name,
        sizeof(engine_name),
        (struct table_option_name_policy){
            .identifier_kind = "storage engine",
            .nul_message = "table engine names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(engine_name, "InnoDB")) {
        if (session_sql_mode_has(
                &database->session,
                MYLITE_SESSION_SQL_MODE_NO_ENGINE_SUBSTITUTION
            )) {
            set_unknown_storage_engine_error(database, engine_name);
            return MYLITE_ERROR;
        }

        rc = append_unknown_storage_engine_warning(database, engine_name);
        if (rc == MYLITE_OK) {
            rc = append_using_storage_engine_warning(database, table_name);
        }
        return rc;
    }

    return MYLITE_OK;
}

static int append_unknown_storage_engine_warning(
    struct mylite_db *database,
    const char *engine_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown storage engine '%s'", engine_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_storage_engine,
        "42000",
        message
    );
}

static int append_using_storage_engine_warning(struct mylite_db *database, const char *table_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Using storage engine InnoDB for table '%s'",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_using_storage_engine,
        "HY000",
        message
    );
}

static int validate_create_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    bool allow_binary
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    rc = copy_table_charset_option_name(
        database,
        charset_option,
        charset_name,
        sizeof(charset_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (allow_binary && charset_name_is_binary(charset_name)) {
        return MYLITE_OK;
    }
    if (charset_name_is_binary(charset_name)) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (character_set_by_name(charset_name) == NULL) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    bool allow_binary
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    rc = copy_table_collation_option_name(
        database,
        collation_option,
        collation_name,
        sizeof(collation_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (allow_binary && collation_name_is_binary(collation_name)) {
        return MYLITE_OK;
    }
    if (collation_name_is_binary(collation_name)) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (collation_by_name(collation_name) == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *comment_option
) {
    char *decoded = NULL;
    int rc = MYLITE_OK;

    if (comment_option == NULL || comment_option->kind != MYLITE_SQL_AST_TABLE_COMMENT_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(
        database,
        child_at(comment_option, 0U),
        &decoded,
        (struct table_option_name_policy){
            .identifier_kind = "table comment",
            .nul_message = "table comments do not support NUL bytes",
        }
    );
    free(decoded);
    return rc;
}

static int validate_table_charset_collation_option_values(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
) {
    return validate_table_charset_collation_option_values_with_binary(
        database,
        has_charset,
        charset_name,
        has_collation,
        collation_name,
        false
    );
}

static int validate_table_charset_collation_option_values_with_binary(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name,
    bool allow_binary
) {
    bool charset_is_binary = false;
    bool charset_is_supported = false;
    bool charset_is_known_unsupported = false;
    bool collation_is_binary = false;
    bool collation_is_supported = false;
    bool collation_is_known_unsupported = false;
    int rc = MYLITE_OK;

    rc = classify_table_charset_option_value(
        database,
        has_charset,
        charset_name,
        allow_binary,
        &charset_is_binary,
        &charset_is_supported,
        &charset_is_known_unsupported
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = classify_table_collation_option_value(
        database,
        has_collation,
        collation_name,
        allow_binary,
        &collation_is_binary,
        &collation_is_supported,
        &collation_is_known_unsupported
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (has_charset && has_collation) {
        rc = validate_table_charset_collation_option_pair(
            database,
            charset_name,
            charset_is_binary,
            charset_is_supported,
            charset_is_known_unsupported,
            collation_name,
            collation_is_binary,
            collation_is_supported,
            collation_is_known_unsupported
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (has_charset && !charset_is_supported && !charset_is_binary) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (has_collation && !collation_is_supported && !collation_is_binary) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int classify_table_charset_option_value(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool allow_binary,
    bool *out_is_binary,
    bool *out_is_supported,
    bool *out_is_known_unsupported
) {
    const struct mylite_execution_catalog_character_set *charset = NULL;

    *out_is_binary = false;
    *out_is_supported = false;
    *out_is_known_unsupported = false;
    if (!has_charset) {
        return MYLITE_OK;
    }

    if (allow_binary) {
        *out_is_binary = charset_name_is_binary(charset_name);
    }
    charset = character_set_by_name(charset_name);
    if (charset != NULL && (*out_is_binary || !charset_name_is_binary(charset_name))) {
        *out_is_supported = true;
    }
    *out_is_known_unsupported = charset_name_is_known_unsupported(charset_name);
    if (!*out_is_binary && !*out_is_supported && !*out_is_known_unsupported) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int classify_table_collation_option_value(
    struct mylite_db *database,
    bool has_collation,
    const char *collation_name,
    bool allow_binary,
    bool *out_is_binary,
    bool *out_is_supported,
    bool *out_is_known_unsupported
) {
    const struct mylite_execution_catalog_collation *collation = NULL;

    *out_is_binary = false;
    *out_is_supported = false;
    *out_is_known_unsupported = false;
    if (!has_collation) {
        return MYLITE_OK;
    }

    if (allow_binary) {
        *out_is_binary = collation_name_is_binary(collation_name);
    }
    collation = collation_by_name(collation_name);
    if (collation != NULL && (*out_is_binary || !collation_name_is_binary(collation_name))) {
        *out_is_supported = true;
    }
    *out_is_known_unsupported = collation_name_is_known_unsupported(collation_name);
    if (!*out_is_binary && !*out_is_supported && !*out_is_known_unsupported) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_table_charset_collation_option_pair(
    struct mylite_db *database,
    const char *charset_name,
    bool charset_is_binary,
    bool charset_is_supported,
    bool charset_is_known_unsupported,
    const char *collation_name,
    bool collation_is_binary,
    bool collation_is_supported,
    bool collation_is_known_unsupported
) {
    const struct mylite_execution_catalog_character_set *charset = NULL;
    const struct mylite_execution_catalog_collation *collation = NULL;

    if (charset_is_binary != collation_is_binary) {
        set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
        return MYLITE_ERROR;
    }
    if (charset_is_supported && collation_is_supported) {
        charset = character_set_by_name(charset_name);
        collation = collation_by_name(collation_name);
        if (charset == NULL || collation == NULL ||
            strcmp(charset->name, collation->character_set) != 0) {
            set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
            return MYLITE_ERROR;
        }
    }
    if ((charset_is_supported && collation_is_known_unsupported) ||
        (charset_is_known_unsupported && collation_is_supported)) {
        set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int copy_table_charset_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    char *charset_name,
    size_t charset_name_size
) {
    if (charset_option == NULL || charset_option->kind != MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_table_option_name_text(
        database,
        child_at(charset_option, 0U),
        charset_name,
        charset_name_size,
        (struct table_option_name_policy){
            .identifier_kind = "character set",
            .nul_message = "table character set names do not support NUL bytes",
        }
    );
}

static int copy_table_collation_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    char *collation_name,
    size_t collation_name_size
) {
    if (collation_option == NULL ||
        collation_option->kind != MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_table_option_name_text(
        database,
        child_at(collation_option, 0U),
        collation_name,
        collation_name_size,
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "table collation names do not support NUL bytes",
        }
    );
}

static int apply_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    bool allow_binary,
    char *default_charset,
    size_t default_charset_size
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = validate_create_table_charset_option(database, charset_option, allow_binary);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = copy_table_charset_option_name(
        database,
        charset_option,
        charset_name,
        sizeof(charset_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    (void)allow_binary;
    return copy_normalized_charset_name(
        database,
        charset_name,
        default_charset,
        default_charset_size
    );
}

static int apply_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    bool allow_binary,
    char *default_collation,
    size_t default_collation_size
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_collation *collation = NULL;
    int rc = MYLITE_OK;

    if (collation_option == NULL ||
        collation_option->kind != MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(collation_option, 0U),
        collation_name,
        sizeof(collation_name),
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "table collation names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    (void)allow_binary;
    collation = collation_by_name(collation_name);
    if (collation == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (strlen(collation->name) >= default_collation_size) {
        set_runtime_error(database, "table collation descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(default_collation, collation->name, strlen(collation->name) + 1U);
    return MYLITE_OK;
}

static int copy_default_collation_for_charset(
    struct mylite_db *database,
    const char *charset_name,
    char *default_collation,
    size_t default_collation_size
) {
    const struct mylite_execution_catalog_character_set *charset =
        character_set_by_name(charset_name);

    if (charset == NULL) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (strlen(charset->default_collation) >= default_collation_size) {
        set_runtime_error(database, "schema collation descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(default_collation, charset->default_collation, strlen(charset->default_collation) + 1U);
    return MYLITE_OK;
}

static int copy_normalized_charset_name(
    struct mylite_db *database,
    const char *charset_name,
    char *destination,
    size_t destination_size
) {
    const struct mylite_execution_catalog_character_set *charset =
        character_set_by_name(charset_name);

    if (charset == NULL) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (strlen(charset->name) >= destination_size) {
        set_runtime_error(database, "schema charset descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(destination, charset->name, strlen(charset->name) + 1U);
    return MYLITE_OK;
}

static int copy_normalized_collation_name(
    struct mylite_db *database,
    const char *collation_name,
    char *destination,
    size_t destination_size
) {
    const struct mylite_execution_catalog_collation *collation = collation_by_name(collation_name);

    if (collation == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (strlen(collation->name) >= destination_size) {
        set_runtime_error(database, "schema collation descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(destination, collation->name, strlen(collation->name) + 1U);
    return MYLITE_OK;
}

static int copy_table_option_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char *destination,
    size_t destination_size,
    struct table_option_name_policy policy
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (option_name_node == NULL || destination == NULL || destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (option_name_node->span.text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < option_name_node->span.length; ++index) {
        if (option_name_node->span.text[index] == '\0') {
            set_unsupported_error(database, policy.nul_message);
            return MYLITE_ERROR;
        }
    }
    if (option_name_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_identifier_text(option_name_node, destination, destination_size, database);
    }
    if (option_name_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(option_name_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(database, option_name_node, &decoded, policy);
    if (rc != MYLITE_OK) {
        return rc;
    }

    decoded_length = strlen(decoded);
    if (decoded_length >= destination_size) {
        free(decoded);
        set_identifier_too_long_error(database, policy.identifier_kind);
        return MYLITE_ERROR;
    }

    memcpy(destination, decoded, decoded_length + 1U);
    free(decoded);
    return MYLITE_OK;
}

static int decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char **out_name,
    struct table_option_name_policy policy
) {
    struct mylite_dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    bool allow_backslash = false;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        set_runtime_error(database, "invalid table option name");
        return MYLITE_ERROR;
    }
    *out_name = NULL;
    if (option_name_node == NULL || option_name_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(option_name_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    text = option_name_node->span.text;
    length = option_name_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid table option name");
        return MYLITE_ERROR;
    }

    quote = text[0];
    allow_backslash = true;
    if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        allow_backslash = false;
    }
    mylite_dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = mylite_dynamic_string_append_char(&string, quote);
            ++index;
        } else if (allow_backslash && byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_table_option_name_escape(database, &string, text[index], policy);
        } else {
            rc = mylite_dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_name = mylite_dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_table_option_name_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte,
    struct table_option_name_policy policy
) {
    switch (escaped_byte) {
    case '0':
        set_unsupported_error(database, policy.nul_message);
        return MYLITE_ERROR;
    case 'n':
        return mylite_dynamic_string_append_char(string, '\n');
    case 'r':
        return mylite_dynamic_string_append_char(string, '\r');
    case 't':
        return mylite_dynamic_string_append_char(string, '\t');
    case 'b':
        return mylite_dynamic_string_append_char(string, '\b');
    case 'Z':
        return mylite_dynamic_string_append_char(string, '\032');
    case '\\':
        return mylite_dynamic_string_append_char(string, '\\');
    case '\'':
        return mylite_dynamic_string_append_char(string, '\'');
    case '"':
        return mylite_dynamic_string_append_char(string, '"');
    case '%':
    case '_': {
        int rc = mylite_dynamic_string_append_char(string, '\\');

        if (rc != MYLITE_OK) {
            return rc;
        }
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
    default:
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
}

static const struct mylite_execution_catalog_collation *utf8mb4_collation_by_name(
    const char *name
) {
    return mylite_execution_catalog_utf8mb4_collation_by_name(name);
}

static const struct mylite_execution_catalog_character_set *character_set_by_name(
    const char *name
) {
    return mylite_execution_catalog_character_set_by_name(name);
}

static const struct mylite_execution_catalog_collation *collation_by_name(const char *name) {
    return mylite_execution_catalog_collation_by_name(name);
}

static bool charset_name_is_binary(const char *name) {
    if (name == NULL) {
        return false;
    }
    return text_equals_ascii_case_insensitive(name, "binary");
}

static bool collation_name_is_binary(const char *name) {
    if (name == NULL) {
        return false;
    }
    return text_equals_ascii_case_insensitive(name, "binary");
}

static bool charset_name_is_known_unsupported(const char *name) {
    if (name == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(name, "latin1")) {
        return true;
    }
    return false;
}

static bool collation_name_is_known_unsupported(const char *name) {
    if (name == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(name, "latin1_swedish_ci")) {
        return true;
    }
    return false;
}

static void planned_create_table_deinit(struct planned_create_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    free(plan->primary_key_parts);
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        free(plan->secondary_indexes[index].parts);
    }
    free(plan->secondary_indexes);
    for (size_t index = 0U; index < plan->foreign_key_count; ++index) {
        planned_foreign_key_deinit(&plan->foreign_keys[index]);
    }
    free(plan->foreign_keys);
    free(plan->check_constraints);
    *plan = (struct planned_create_table){0};
}

static int validate_sql_require_primary_key_for_planned_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    if (!sql_require_primary_key_session_enabled(database)) {
        return MYLITE_OK;
    }
    if (plan != NULL && plan->has_primary_key) {
        return MYLITE_OK;
    }

    set_sql_require_primary_key_error(database);
    return MYLITE_ERROR;
}

static int validate_sql_require_primary_key_for_table_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct primary_key_info primary_key = primary_key_info_init();
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (!sql_require_primary_key_session_enabled(database) || table == NULL ||
        (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
         table->kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY)) {
        return MYLITE_OK;
    }

    rc = load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(database, table->table_id, columns, column_count, &primary_key);
    }
    if (rc == MYLITE_OK && !primary_key.has_primary_key) {
        set_sql_require_primary_key_error(database);
        rc = MYLITE_ERROR;
    }

    primary_key_info_deinit(&primary_key);
    free(columns);
    return rc;
}

static bool sql_require_primary_key_session_enabled(const struct mylite_db *database) {
    if (database == NULL) {
        return false;
    }
    return database->session.sql_require_primary_key;
}

static int create_table_from_plan(struct mylite_db *database, struct planned_create_table *plan) {
    struct mylite_catalog_table_descriptor existing_table = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    bool existing_table_found = false;
    int64_t table_id = 0;
    size_t hash_warning_count = planned_create_table_hash_index_warning_count(plan);
    size_t spatial_warning_count = planned_create_table_spatial_index_warning_count(plan);
    int rc = MYLITE_OK;

    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &existing_table,
        &existing_table_found
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (existing_table_found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }

    rc = reserve_hash_index_warnings(database, hash_warning_count);
    if (rc == MYLITE_OK) {
        rc = reserve_spatial_index_warnings(database, spatial_warning_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_allocate_table_id_in_mutation(database, &mutation, &table_id);
    }
    if (rc == MYLITE_OK) {
        rc = build_physical_table_name(table_id, physical_name, sizeof(physical_name));
    }
    if (rc == MYLITE_OK) {
        rc = assign_create_table_index_ids(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = assign_create_table_foreign_key_ids(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = assign_create_table_check_constraint_ids(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = insert_create_table_catalog_rows(
            database,
            plan,
            &mutation,
            table_id,
            physical_name,
            &table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_create_table(database, plan, table.physical_name, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    rc = append_hash_index_warnings(database, hash_warning_count);
    if (rc == MYLITE_OK) {
        rc = append_spatial_index_warnings(database, plan);
    }
    return rc;
}

static int create_temporary_table_from_plan(
    struct mylite_db *database,
    struct planned_create_table *plan
) {
    struct mylite_temporary_catalog_table temporary_table = {0};
    struct mylite_catalog_table_descriptor existing_table = {0};
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    bool existing_table_found = false;
    bool physical_table_created = false;
    int64_t table_id = 0;
    size_t hash_warning_count = planned_create_table_hash_index_warning_count(plan);
    size_t spatial_warning_count = planned_create_table_spatial_index_warning_count(plan);
    int rc = MYLITE_OK;

    if (plan->foreign_key_count != 0U) {
        set_unsupported_error(database, "Temporary FOREIGN KEY tables are not yet supported");
        return MYLITE_ERROR;
    }
    if (plan->check_constraint_count != 0U) {
        set_unsupported_error(database, "Temporary CHECK constraint tables are not yet supported");
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_generated) {
            set_unsupported_error(database, "Temporary generated columns are not yet supported");
            return MYLITE_ERROR;
        }
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        plan->target.schema.name,
        plan->target.table_name,
        &existing_table,
        &existing_table_found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (existing_table_found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }

    rc = reserve_hash_index_warnings(database, hash_warning_count);
    if (rc == MYLITE_OK) {
        rc = reserve_spatial_index_warnings(database, spatial_warning_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_temporary_catalog_allocate_table_identity(
            &database->session.temporary_catalog,
            &table_id,
            physical_name,
            sizeof(physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = assign_create_temporary_table_index_ids(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = build_temporary_table_descriptors(
            database,
            plan,
            table_id,
            physical_name,
            &temporary_table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_create_table(database, plan, physical_name, true);
        physical_table_created = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_temporary_catalog_append_table(
            &database->session.temporary_catalog,
            &temporary_table
        );
    }
    if (rc != MYLITE_OK) {
        if (physical_table_created) {
            (void)execute_physical_drop_table(database, physical_name);
        }
        mylite_temporary_catalog_table_deinit(&temporary_table);
        set_internal_error_if_clear(database, rc, "failed to create temporary table descriptor");
        return rc;
    }

    ++database->session.sqlite_schema_generation;
    rc = append_hash_index_warnings(database, hash_warning_count);
    if (rc == MYLITE_OK) {
        rc = append_spatial_index_warnings(database, plan);
    }
    return rc;
}

static int assign_create_table_index_ids(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_create_table *plan
) {
    int64_t next_index_id = 0;
    int rc = MYLITE_OK;

    if (!plan->has_primary_key && plan->secondary_index_count == 0U) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_allocate_index_id_in_mutation(database, mutation, &next_index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->has_primary_key) {
        plan->primary_key_index_id = next_index_id;
        rc = build_physical_index_name(
            plan->primary_key_index_id,
            plan->primary_key_physical_name,
            sizeof(plan->primary_key_physical_name)
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (next_index_id == INT64_MAX) {
            set_runtime_error(database, "index identifier space is exhausted");
            return MYLITE_ERROR;
        }
        ++next_index_id;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        plan->secondary_indexes[index].index_id = next_index_id;
        rc = build_physical_index_name(
            plan->secondary_indexes[index].index_id,
            plan->secondary_indexes[index].physical_name,
            sizeof(plan->secondary_indexes[index].physical_name)
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (index + 1U < plan->secondary_index_count) {
            if (next_index_id == INT64_MAX) {
                set_runtime_error(database, "index identifier space is exhausted");
                return MYLITE_ERROR;
            }
            ++next_index_id;
        }
    }

    return MYLITE_OK;
}

static int assign_create_table_foreign_key_ids(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_create_table *plan
) {
    int64_t next_foreign_key_id = 0;
    int rc = MYLITE_OK;

    if (plan->foreign_key_count == 0U) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_allocate_foreign_key_id_in_mutation(
        database,
        mutation,
        &next_foreign_key_id
    );
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->foreign_key_count; ++index) {
        struct planned_foreign_key *foreign_key = &plan->foreign_keys[index];

        foreign_key->foreign_key_id = next_foreign_key_id;
        if (foreign_key->child_index_is_primary) {
            foreign_key->child_index_id = plan->primary_key_index_id;
        } else if (foreign_key->child_secondary_index_index < plan->secondary_index_count) {
            foreign_key->child_index_id =
                plan->secondary_indexes[foreign_key->child_secondary_index_index].index_id;
        }
        if (foreign_key->child_index_id <= 0) {
            set_runtime_error(database, "invalid foreign-key child index descriptor");
            return MYLITE_ERROR;
        }
        if (index + 1U < plan->foreign_key_count) {
            if (next_foreign_key_id == INT64_MAX) {
                set_runtime_error(database, "foreign-key identifier space is exhausted");
                return MYLITE_ERROR;
            }
            ++next_foreign_key_id;
        }
    }

    return rc;
}

static int assign_create_table_check_constraint_ids(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_create_table *plan
) {
    int64_t next_check_constraint_id = 0;
    int rc = MYLITE_OK;

    if (plan->check_constraint_count == 0U) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_allocate_check_constraint_id_in_mutation(
        database,
        mutation,
        &next_check_constraint_id
    );
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->check_constraint_count; ++index) {
        struct planned_check_constraint *check_constraint = &plan->check_constraints[index];

        check_constraint->check_constraint_id = next_check_constraint_id;
        rc = build_physical_check_constraint_name(
            check_constraint->check_constraint_id,
            check_constraint->physical_name,
            sizeof(check_constraint->physical_name)
        );
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to build CHECK constraint physical name");
            return MYLITE_ERROR;
        }
        if (index + 1U < plan->check_constraint_count) {
            if (next_check_constraint_id == INT64_MAX) {
                set_runtime_error(database, "CHECK constraint identifier space is exhausted");
                return MYLITE_ERROR;
            }
            ++next_check_constraint_id;
        }
    }

    return rc;
}

static int assign_create_temporary_table_index_ids(
    struct mylite_db *database,
    struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    if (!plan->has_primary_key && plan->secondary_index_count == 0U) {
        return MYLITE_OK;
    }
    if (plan->has_primary_key) {
        rc = mylite_temporary_catalog_allocate_index_identity(
            &database->session.temporary_catalog,
            &plan->primary_key_index_id,
            plan->primary_key_physical_name,
            sizeof(plan->primary_key_physical_name)
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->secondary_index_count; ++index) {
        rc = mylite_temporary_catalog_allocate_index_identity(
            &database->session.temporary_catalog,
            &plan->secondary_indexes[index].index_id,
            plan->secondary_indexes[index].physical_name,
            sizeof(plan->secondary_indexes[index].physical_name)
        );
    }

    return rc;
}

static int insert_create_table_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_table_descriptor *out_table
) {
    int64_t *column_ids = NULL;
    int64_t created_time_utc_epoch = current_timestamp_epoch(database);
    int rc = mylite_catalog_insert_table_in_mutation(
        database,
        mutation,
        table_id,
        plan->target.schema.schema_id,
        plan->target.table_name,
        physical_name,
        MYLITE_CATALOG_TABLE_KIND_BASE,
        plan->auto_increment_next > 0 ? plan->auto_increment_next : 1,
        plan->default_charset,
        plan->default_collation,
        plan->comment,
        plan->row_format_option,
        plan->key_block_size,
        plan->pack_keys,
        plan->checksum,
        plan->stats_persistent,
        plan->stats_auto_recalc,
        plan->stats_sample_pages,
        created_time_utc_epoch,
        created_time_utc_epoch,
        out_table
    );

    if (rc == MYLITE_OK && plan->column_count > SIZE_MAX / sizeof(*column_ids)) {
        set_nomem_error(database);
        rc = MYLITE_NOMEM;
    }
    if (rc == MYLITE_OK) {
        column_ids = calloc(plan->column_count, sizeof(*column_ids));
        if (column_ids == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];
        struct mylite_catalog_column_descriptor inserted_column = {0};

        rc = mylite_catalog_insert_column_in_mutation(
            database,
            mutation,
            table_id,
            (int64_t)column_index + 1,
            column->name,
            column->logical_type,
            column->physical_type,
            column->is_nullable,
            column->is_visible,
            column->is_auto_increment,
            column->default_kind,
            column->default_integer,
            column->default_text,
            column->on_update_current_timestamp,
            column->character_set_name,
            column->collation_name,
            column->comment,
            column->is_generated,
            column->generated_kind,
            column->generation_expression,
            column->sqlite_generation_expression,
            &inserted_column
        );
        if (rc == MYLITE_OK) {
            column_ids[column_index] = inserted_column.column_id;
        }
    }
    if (rc == MYLITE_OK) {
        rc = insert_create_table_index_catalog_rows(database, plan, mutation, table_id, column_ids);
    }
    if (rc == MYLITE_OK) {
        rc = insert_create_table_foreign_key_catalog_rows(
            database,
            plan,
            mutation,
            table_id,
            column_ids
        );
    }
    if (rc == MYLITE_OK) {
        rc = insert_create_table_check_constraint_catalog_rows(database, plan, mutation, table_id);
    }

    free(column_ids);
    return rc;
}

static int insert_create_table_index_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const int64_t *column_ids
) {
    int rc = MYLITE_OK;

    if (plan->has_primary_key) {
        if (plan->primary_key_part_count == 0U || plan->primary_key_index_id <= 0 ||
            plan->primary_key_physical_name[0] == '\0') {
            set_runtime_error(database, "invalid primary-key descriptor");
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_insert_index_in_mutation(
            database,
            mutation,
            plan->primary_key_index_id,
            table_id,
            "PRIMARY",
            plan->primary_key_physical_name,
            MYLITE_CATALOG_INDEX_KIND_PRIMARY,
            true,
            true,
            plan->primary_key_comment,
            plan->primary_key_show_create_explicit_btree,
            NULL
        );
    }
    for (size_t part_index = 0U;
         rc == MYLITE_OK && plan->has_primary_key && part_index < plan->primary_key_part_count;
         ++part_index) {
        size_t column_index = plan->primary_key_parts[part_index].column_index;

        if (column_index >= plan->column_count || column_ids[column_index] <= 0) {
            set_runtime_error(database, "invalid primary-key descriptor");
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_insert_index_column_in_mutation(
            database,
            mutation,
            plan->primary_key_index_id,
            table_id,
            column_ids[column_index],
            (int64_t)part_index + 1,
            NULL,
            plan->primary_key_parts[part_index].sort_direction,
            NULL
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->secondary_index_count; ++index) {
        const struct planned_secondary_index *secondary = &plan->secondary_indexes[index];

        if (secondary->part_count == 0U || secondary->index_id <= 0 ||
            secondary->physical_name[0] == '\0') {
            set_runtime_error(database, "invalid secondary-index descriptor");
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_insert_index_in_mutation(
            database,
            mutation,
            secondary->index_id,
            table_id,
            secondary->name,
            secondary->physical_name,
            secondary->kind,
            secondary->is_unique,
            secondary->is_visible,
            secondary->comment,
            secondary->show_create_explicit_btree,
            NULL
        );
        for (size_t part_index = 0U; rc == MYLITE_OK && part_index < secondary->part_count;
             ++part_index) {
            const struct planned_secondary_index_part *part = &secondary->parts[part_index];
            const int64_t *prefix_length = NULL;

            if (part->has_prefix_length) {
                prefix_length = &part->prefix_length;
            }
            if (part->column_index >= plan->column_count || column_ids[part->column_index] <= 0) {
                set_runtime_error(database, "invalid secondary-index descriptor");
                return MYLITE_ERROR;
            }
            rc = mylite_catalog_insert_index_column_in_mutation(
                database,
                mutation,
                secondary->index_id,
                table_id,
                column_ids[part->column_index],
                (int64_t)part_index + 1,
                prefix_length,
                part->sort_direction,
                NULL
            );
        }
    }

    return rc;
}

static int insert_create_table_foreign_key_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const int64_t *column_ids
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->foreign_key_count; ++index) {
        const struct planned_foreign_key *foreign_key = &plan->foreign_keys[index];

        if (foreign_key->part_count == 0U || foreign_key->foreign_key_id <= 0 ||
            foreign_key->parent_index.index_id <= 0 || foreign_key->child_index_id <= 0) {
            set_runtime_error(database, "invalid foreign-key descriptor");
            return MYLITE_ERROR;
        }

        rc = mylite_catalog_insert_foreign_key_in_mutation(
            database,
            mutation,
            foreign_key->foreign_key_id,
            table_id,
            foreign_key->parent_table.table_id,
            foreign_key->name,
            foreign_key->parent_index.index_id,
            foreign_key->child_index_id,
            foreign_key->update_rule,
            foreign_key->delete_rule,
            "NONE",
            NULL
        );
        for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
             ++part_index) {
            const struct planned_foreign_key_part *part = &foreign_key->parts[part_index];
            int64_t child_column_id = 0;

            if (part->child_column_index >= plan->column_count ||
                column_ids[part->child_column_index] <= 0 || part->parent_column.column_id <= 0) {
                set_runtime_error(database, "invalid foreign-key descriptor");
                return MYLITE_ERROR;
            }
            child_column_id = column_ids[part->child_column_index];
            rc = mylite_catalog_insert_foreign_key_column_in_mutation(
                database,
                mutation,
                foreign_key->foreign_key_id,
                table_id,
                foreign_key->parent_table.table_id,
                child_column_id,
                part->parent_column.column_id,
                (int64_t)part_index + 1,
                (int64_t)part_index + 1,
                NULL
            );
        }
    }

    return rc;
}

static int insert_create_table_check_constraint_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->check_constraint_count; ++index) {
        const struct planned_check_constraint *check_constraint = &plan->check_constraints[index];

        if (check_constraint->check_constraint_id <= 0 ||
            check_constraint->physical_name[0] == '\0' || check_constraint->name[0] == '\0' ||
            check_constraint->check_clause[0] == '\0' ||
            check_constraint->sqlite_expression[0] == '\0') {
            set_runtime_error(database, "invalid CHECK constraint descriptor");
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_insert_check_constraint_in_mutation(
            database,
            mutation,
            check_constraint->check_constraint_id,
            table_id,
            check_constraint->name,
            check_constraint->physical_name,
            check_constraint->check_clause,
            check_constraint->sqlite_expression,
            check_constraint->is_enforced,
            check_constraint->name_is_generated,
            check_constraint->generated_ordinal,
            check_constraint->ordinal_position,
            NULL
        );
    }

    return rc;
}

static int build_temporary_table_descriptors(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    const char *physical_name,
    struct mylite_temporary_catalog_table *out_table
) {
    int64_t *column_ids = NULL;
    int rc = MYLITE_OK;

    if (out_table == NULL || physical_name == NULL) {
        set_runtime_error(database, "invalid temporary table descriptor");
        return MYLITE_ERROR;
    }
    *out_table = (struct mylite_temporary_catalog_table){0};
    snprintf(
        out_table->schema_name,
        sizeof(out_table->schema_name),
        "%s",
        plan->target.schema.name
    );
    out_table->table = (struct mylite_catalog_table_descriptor){
        .table_id = table_id,
        .schema_id = plan->target.schema.schema_id,
        .kind = MYLITE_CATALOG_TABLE_KIND_TEMPORARY,
        .auto_increment_next = plan->auto_increment_next > 0 ? plan->auto_increment_next : 1,
        .key_block_size = plan->key_block_size,
        .pack_keys = plan->pack_keys,
        .checksum = plan->checksum,
        .stats_persistent = plan->stats_persistent,
        .stats_auto_recalc = plan->stats_auto_recalc,
        .stats_sample_pages = plan->stats_sample_pages,
    };
    snprintf(out_table->table.name, sizeof(out_table->table.name), "%s", plan->target.table_name);
    snprintf(
        out_table->table.physical_name,
        sizeof(out_table->table.physical_name),
        "%s",
        physical_name
    );
    snprintf(
        out_table->table.default_charset,
        sizeof(out_table->table.default_charset),
        "%s",
        plan->default_charset
    );
    snprintf(
        out_table->table.default_collation,
        sizeof(out_table->table.default_collation),
        "%s",
        plan->default_collation
    );
    snprintf(out_table->table.comment, sizeof(out_table->table.comment), "%s", plan->comment);
    snprintf(
        out_table->table.row_format_option,
        sizeof(out_table->table.row_format_option),
        "%s",
        plan->row_format_option
    );

    rc = build_temporary_table_column_descriptors(database, plan, table_id, out_table, &column_ids);
    if (rc == MYLITE_OK) {
        rc = build_temporary_table_index_descriptors(
            database,
            plan,
            table_id,
            column_ids,
            out_table
        );
    }

    free(column_ids);
    if (rc != MYLITE_OK) {
        mylite_temporary_catalog_table_deinit(out_table);
    }
    return rc;
}

static int build_temporary_table_column_descriptors(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    struct mylite_temporary_catalog_table *out_table,
    int64_t **out_column_ids
) {
    int64_t *column_ids = NULL;
    int rc = MYLITE_OK;

    *out_column_ids = NULL;
    if (plan->column_count > SIZE_MAX / sizeof(*out_table->columns) ||
        plan->column_count > SIZE_MAX / sizeof(*column_ids)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_table->columns = calloc(plan->column_count, sizeof(*out_table->columns));
    column_ids = calloc(plan->column_count, sizeof(*column_ids));
    if (out_table->columns == NULL || column_ids == NULL) {
        free(column_ids);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_table->column_count = plan->column_count;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];
        struct mylite_catalog_column_descriptor *descriptor = &out_table->columns[column_index];

        rc = mylite_temporary_catalog_allocate_column_id(
            &database->session.temporary_catalog,
            &column_ids[column_index]
        );
        if (rc != MYLITE_OK) {
            break;
        }
        *descriptor = (struct mylite_catalog_column_descriptor){
            .column_id = column_ids[column_index],
            .table_id = table_id,
            .ordinal_position = (int64_t)column_index + 1,
            .is_nullable = column->is_nullable,
            .is_visible = column->is_visible,
            .is_auto_increment = column->is_auto_increment,
            .default_kind = column->default_kind,
            .default_integer = column->default_integer,
            .on_update_current_timestamp = column->on_update_current_timestamp,
        };
        snprintf(descriptor->name, sizeof(descriptor->name), "%s", column->name);
        snprintf(
            descriptor->logical_type,
            sizeof(descriptor->logical_type),
            "%s",
            column->logical_type
        );
        snprintf(
            descriptor->physical_type,
            sizeof(descriptor->physical_type),
            "%s",
            column->physical_type
        );
        snprintf(
            descriptor->default_text,
            sizeof(descriptor->default_text),
            "%s",
            column->default_text
        );
        snprintf(
            descriptor->character_set_name,
            sizeof(descriptor->character_set_name),
            "%s",
            column->character_set_name
        );
        snprintf(
            descriptor->collation_name,
            sizeof(descriptor->collation_name),
            "%s",
            column->collation_name
        );
        snprintf(descriptor->comment, sizeof(descriptor->comment), "%s", column->comment);
    }
    if (rc != MYLITE_OK) {
        free(column_ids);
        return rc;
    }

    *out_column_ids = column_ids;
    return MYLITE_OK;
}

static int build_temporary_table_index_descriptors(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    const int64_t *column_ids,
    struct mylite_temporary_catalog_table *out_table
) {
    size_t primary_index_count = 0U;
    size_t index_count = 0U;
    size_t index_column_count = 0U;
    struct temporary_index_descriptor_positions positions = {0};
    int rc = MYLITE_OK;

    if (plan->has_primary_key) {
        primary_index_count = 1U;
        index_column_count = plan->primary_key_part_count;
    }
    index_count = plan->secondary_index_count + primary_index_count;
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        index_column_count += plan->secondary_indexes[index].part_count;
    }
    if (index_count == 0U) {
        return MYLITE_OK;
    }
    if (index_column_count == 0U) {
        return MYLITE_ERROR;
    }
    if (index_count > SIZE_MAX / sizeof(*out_table->indexes) ||
        index_column_count > SIZE_MAX / sizeof(*out_table->index_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_table->indexes = calloc(index_count, sizeof(*out_table->indexes));
    out_table->index_columns = calloc(index_column_count, sizeof(*out_table->index_columns));
    if (out_table->indexes == NULL || out_table->index_columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_table->index_count = index_count;
    out_table->index_column_count = index_column_count;

    if (plan->has_primary_key) {
        rc = append_temporary_primary_index_descriptor(
            database,
            plan,
            table_id,
            column_ids,
            out_table,
            &positions
        );
    }
    for (size_t index_index = 0U; rc == MYLITE_OK && index_index < plan->secondary_index_count;
         ++index_index) {
        rc = append_temporary_secondary_index_descriptor(
            database,
            &plan->secondary_indexes[index_index],
            table_id,
            column_ids,
            out_table,
            &positions
        );
    }

    return rc;
}

static int append_temporary_primary_index_descriptor(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t table_id,
    const int64_t *column_ids,
    struct mylite_temporary_catalog_table *out_table,
    struct temporary_index_descriptor_positions *positions
) {
    struct mylite_catalog_index_descriptor *index = NULL;

    if (positions->index >= out_table->index_count) {
        return MYLITE_ERROR;
    }
    index = &out_table->indexes[positions->index];
    ++positions->index;

    *index = (struct mylite_catalog_index_descriptor){
        .index_id = plan->primary_key_index_id,
        .table_id = table_id,
        .kind = MYLITE_CATALOG_INDEX_KIND_PRIMARY,
        .is_unique = true,
        .is_visible = true,
        .show_create_explicit_btree = plan->primary_key_show_create_explicit_btree,
    };
    snprintf(index->name, sizeof(index->name), "%s", "PRIMARY");
    snprintf(
        index->physical_name,
        sizeof(index->physical_name),
        "%s",
        plan->primary_key_physical_name
    );
    snprintf(index->comment, sizeof(index->comment), "%s", plan->primary_key_comment);

    for (size_t part_index = 0U; part_index < plan->primary_key_part_count; ++part_index) {
        size_t column_index = plan->primary_key_parts[part_index].column_index;
        struct mylite_catalog_index_column_descriptor *part = NULL;
        int64_t index_column_id = 0;
        int rc = MYLITE_OK;

        if (positions->index_column >= out_table->index_column_count) {
            return MYLITE_ERROR;
        }
        part = &out_table->index_columns[positions->index_column];
        ++positions->index_column;

        rc = mylite_temporary_catalog_allocate_index_column_id(
            &database->session.temporary_catalog,
            &index_column_id
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        *part = (struct mylite_catalog_index_column_descriptor){
            .index_column_id = index_column_id,
            .index_id = plan->primary_key_index_id,
            .table_id = table_id,
            .column_id = column_ids[column_index],
            .ordinal_position = (int64_t)part_index + 1,
            .sort_direction = plan->primary_key_parts[part_index].sort_direction,
        };
    }

    return MYLITE_OK;
}

static int append_temporary_secondary_index_descriptor(
    struct mylite_db *database,
    const struct planned_secondary_index *planned,
    int64_t table_id,
    const int64_t *column_ids,
    struct mylite_temporary_catalog_table *out_table,
    struct temporary_index_descriptor_positions *positions
) {
    struct mylite_catalog_index_descriptor *index = NULL;

    if (positions->index >= out_table->index_count) {
        return MYLITE_ERROR;
    }
    index = &out_table->indexes[positions->index];
    ++positions->index;

    *index = (struct mylite_catalog_index_descriptor){
        .index_id = planned->index_id,
        .table_id = table_id,
        .kind = planned->kind,
        .is_unique = planned->is_unique,
        .is_visible = planned->is_visible,
        .show_create_explicit_btree = planned->show_create_explicit_btree,
    };
    snprintf(index->name, sizeof(index->name), "%s", planned->name);
    snprintf(index->physical_name, sizeof(index->physical_name), "%s", planned->physical_name);
    snprintf(index->comment, sizeof(index->comment), "%s", planned->comment);

    for (size_t part_index = 0U; part_index < planned->part_count; ++part_index) {
        const struct planned_secondary_index_part *planned_part = &planned->parts[part_index];
        struct mylite_catalog_index_column_descriptor *part = NULL;
        int64_t index_column_id = 0;
        int rc = MYLITE_OK;

        if (positions->index_column >= out_table->index_column_count) {
            return MYLITE_ERROR;
        }
        part = &out_table->index_columns[positions->index_column];
        ++positions->index_column;

        rc = mylite_temporary_catalog_allocate_index_column_id(
            &database->session.temporary_catalog,
            &index_column_id
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        *part = (struct mylite_catalog_index_column_descriptor){
            .index_column_id = index_column_id,
            .index_id = planned->index_id,
            .table_id = table_id,
            .column_id = column_ids[planned_part->column_index],
            .ordinal_position = (int64_t)part_index + 1,
            .has_prefix_length = planned_part->has_prefix_length,
            .prefix_length = planned_part->prefix_length,
            .sort_direction = planned_part->sort_direction,
        };
    }

    return MYLITE_OK;
}

static bool planned_create_table_has_fulltext_index(const struct planned_create_table *plan) {
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (plan->secondary_indexes[index].kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
            return true;
        }
    }
    return false;
}

static bool planned_create_table_has_spatial_index(const struct planned_create_table *plan) {
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (plan->secondary_indexes[index].kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            return true;
        }
    }
    return false;
}

static bool planned_create_table_has_spatial_column(const struct planned_create_table *plan) {
    if (plan == NULL) {
        return false;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (planned_column_is_spatial(&plan->columns[column_index])) {
            return true;
        }
    }
    return false;
}

static int execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary
) {
    char *sql = NULL;
    int rc = build_create_table_sql(plan, physical_name, temporary, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int execute_physical_drop_table(struct mylite_db *database, const char *physical_name) {
    char *sql = NULL;
    int rc = build_drop_table_sql(physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int create_schema_from_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor existing_schema = {0};
    bool existing_schema_found = false;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    memcpy(
        default_charset,
        MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_CHARSET)
    );
    memcpy(
        default_collation,
        MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_COLLATION)
    );

    if (rc == MYLITE_OK) {
        rc = reject_builtin_schema_schema_write_name(database, schema_name);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(
            database,
            schema_name,
            &existing_schema,
            &existing_schema_found
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        } else if (existing_schema_found) {
            set_database_exists_error(database, schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = apply_schema_default_charset_collation_options(
            database,
            child_with_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST),
            default_charset,
            sizeof(default_charset),
            default_collation,
            sizeof(default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_create_schema_with_defaults(
            database,
            schema_name,
            default_charset,
            default_collation,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        mylite_result_set_affected_rows(result, 1);
    }

    return rc;
}

static int plan_drop_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_catalog_mutation *mutation,
    struct planned_drop_schema *out_plan
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct collect_drop_schema_tables_context context = {0};
    bool schema_found = false;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    *out_plan = (struct planned_drop_schema){0};
    if (rc == MYLITE_OK) {
        rc = reject_builtin_schema_schema_write_name(database, schema_name);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(
            database,
            schema_name,
            &out_plan->schema,
            &schema_found
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        } else if (!schema_found) {
            set_cant_drop_database_error(database, schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.plan = out_plan;
        rc = mylite_catalog_for_each_table_in_schema(
            database,
            out_plan->schema.schema_id,
            collect_drop_schema_table,
            &context
        );
        if (rc != MYLITE_OK) {
            planned_drop_schema_deinit(out_plan);
            if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == 0) {
                set_runtime_error(database, "failed to plan DROP DATABASE");
            }
        }
    }

    return rc;
}

static void planned_drop_schema_deinit(struct planned_drop_schema *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->tables);
    *plan = (struct planned_drop_schema){0};
}

static int collect_drop_schema_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct collect_drop_schema_tables_context *context = user_data;
    struct planned_drop_schema *plan = NULL;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        table->kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
        set_unsupported_error(
            context->database,
            "DROP DATABASE supports only persistent base tables and views"
        );
        return MYLITE_ERROR;
    }

    plan = context->plan;
    ++plan->object_count;
    if (table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        return MYLITE_OK;
    }

    rc = reserve_drop_schema_tables(plan, plan->table_count + 1U);
    if (rc != MYLITE_OK) {
        set_nomem_error(context->database);
        return rc;
    }

    memcpy(
        plan->tables[plan->table_count].physical_name,
        table->physical_name,
        sizeof(plan->tables[plan->table_count].physical_name)
    );
    ++plan->table_count;

    return MYLITE_OK;
}

static int reserve_drop_schema_tables(struct planned_drop_schema *plan, size_t required_capacity) {
    enum { initial_table_capacity = 4 };

    struct planned_drop_schema_table *tables = NULL;
    size_t capacity = 0U;

    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (required_capacity <= plan->table_capacity) {
        return MYLITE_OK;
    }

    capacity = plan->table_capacity == 0U ? initial_table_capacity : plan->table_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*tables)) {
        return MYLITE_NOMEM;
    }

    tables = (struct planned_drop_schema_table *)realloc(plan->tables, capacity * sizeof(*tables));
    if (tables == NULL) {
        return MYLITE_NOMEM;
    }

    plan->tables = tables;
    plan->table_capacity = capacity;

    return MYLITE_OK;
}

static int drop_schema_from_plan(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation,
    const struct planned_drop_schema *plan,
    mylite_result *result
) {
    int rc = mylite_catalog_delete_schema_in_mutation(database, mutation, plan->schema.schema_id);

    for (size_t table_index = 0U; rc == MYLITE_OK && table_index < plan->table_count;
         ++table_index) {
        rc = execute_physical_drop_table(database, plan->tables[table_index].physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, mutation);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (plan->table_count > 0U) {
        ++database->session.sqlite_schema_generation;
    }
    if (database->session.has_selected_schema &&
        strcmp(database->session.selected_schema, plan->schema.name) == 0) {
        database->session.has_selected_schema = false;
        database->session.selected_schema[0] = '\0';
    }
    mylite_result_set_affected_rows(result, (int64_t)plan->object_count);

    return MYLITE_OK;
}

static int plan_truncate_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_truncate_table *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_truncate_table){0};
    rc = resolve_writable_truncate_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "TRUNCATE TABLE supports only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_table_with_foreign_keys(database, out_plan->table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = reject_table_referenced_by_foreign_key(database, out_plan->table.table_id);
    }

    return rc;
}

static int execute_truncate_from_plan(
    struct mylite_db *database,
    const struct planned_truncate_table *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    bool transaction_started = false;
    int sqlite_rc = SQLITE_OK;
    int rc = build_truncate_table_sql(plan, &sql);

    (void)result;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK && plan->table.auto_increment_next != 1) {
        rc = mylite_catalog_update_table_auto_increment_next(database, plan->table.table_id, 1);
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(database, &plan->table, 0, true);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table_statement *out_plan
) {
    const struct mylite_sql_ast_node *pair_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *pair_node = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_rename_table_statement){0};
    if (pair_list == NULL || pair_list->kind != MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->pair_count = mylite_sql_ast_node_child_count(pair_list);
    if (out_plan->pair_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->pair_count > SIZE_MAX / sizeof(*out_plan->pairs)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->pairs = calloc(out_plan->pair_count, sizeof(*out_plan->pairs));
    if (out_plan->pairs == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    pair_node = child_at(pair_list, 0U);
    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < out_plan->pair_count;
         ++pair_index) {
        rc = plan_rename_table_pair(database, pair_node, &out_plan->pairs[pair_index]);
        if (pair_node != NULL) {
            pair_node = pair_node->next_sibling;
        }
    }
    if (rc != MYLITE_OK) {
        planned_rename_table_statement_deinit(out_plan);
    }

    return rc;
}

static int plan_rename_table_pair(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pair_node,
    struct planned_rename_table *out_pair
) {
    int rc = MYLITE_OK;

    *out_pair = (struct planned_rename_table){0};
    if (pair_node == NULL || pair_node->kind != MYLITE_SQL_AST_RENAME_TABLE_PAIR) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = resolve_writable_table_name(database, child_at(pair_node, 0U), &out_pair->source);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_pair->source.table_name)) {
        set_reserved_name_error(database, "table", out_pair->source.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_writable_table_name(database, child_at(pair_node, 1U), &out_pair->target);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_pair->target.table_name)) {
        set_reserved_name_error(database, "table", out_pair->target.table_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static void planned_rename_table_statement_deinit(struct planned_rename_table_statement *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->pairs);
    *plan = (struct planned_rename_table_statement){0};
}

static int rename_table_statement_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table_statement *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < plan->pair_count; ++pair_index) {
        rc = rename_table_pair_in_mutation(
            database,
            &mutation,
            &plan->pairs[pair_index],
            false,
            "RENAME TABLE supports only persistent base tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table descriptor");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_rename(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_rename_table){0};
    rc = require_selected_schema_for_unqualified_table_name(database, child_at(statement, 1U));
    if (rc == MYLITE_OK) {
        rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->source);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->source.table_name)) {
        set_reserved_name_error(database, "table", out_plan->source.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_writable_table_name(database, child_at(statement, 1U), &out_plan->target);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int alter_table_rename_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table *plan
) {
    return rename_table_from_plan_with_policy(
        database,
        plan,
        true,
        "ALTER TABLE RENAME supports only persistent base tables"
    );
}

static int plan_alter_table_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t duplicate_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_column){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 1U), &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_alter_add_column_planned_column(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = reject_inline_primary_key_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE ADD COLUMN does not support PRIMARY KEY"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_auto_increment_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE ADD COLUMN does not support AUTO_INCREMENT"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD COLUMN supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = normalize_text_length_argument_for_default_charset(
            database,
            &out_plan->column,
            out_plan->table.default_charset
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_default_binary_charset_to_column(
            database,
            out_plan->table.default_charset,
            &out_plan->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_column_default(database, out_plan->column.default_node, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = reject_nonempty_temporal_add_column_without_default(
            database,
            &out_plan->table,
            &out_plan->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK &&
        find_column_index(columns, column_count, out_plan->column.name, &duplicate_index) ==
            MYLITE_OK) {
        set_duplicate_column_error(database, out_plan->column.name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_added_column_row_size(database, columns, column_count, &out_plan->column);
    }
    if (rc == MYLITE_OK && column_count >= (size_t)INT64_MAX) {
        set_runtime_error(database, "too many columns in table descriptor");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = finish_alter_table_add_column_plan_columns(
            database,
            child_at(statement, 2U),
            out_plan,
            &columns,
            column_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_column_deinit(out_plan);
    }

    free(columns);

    return rc;
}

static int reject_unsupported_alter_add_column_planned_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column->is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD COLUMN does not support generated columns"
        );
        return MYLITE_ERROR;
    }
    if (planned_column_is_spatial(column)) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD COLUMN does not yet support spatial columns"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int finish_alter_table_add_column_plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_add_column *out_plan,
    struct mylite_catalog_column_descriptor **columns,
    size_t column_count
) {
    int rc = collect_no_zero_date_default_warnings(
        database,
        *columns,
        column_count,
        (struct zero_temporal_default_warning_filter){.skipped_column_id = -1},
        &out_plan->zero_temporal_default_warnings
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    out_plan->columns = *columns;
    out_plan->column_count = column_count;
    *columns = NULL;
    return plan_alter_table_add_column_position(database, position, out_plan);
}

static int plan_alter_table_add_column_position(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_add_column *out_plan
) {
    size_t after_column_index = 0U;
    int rc = MYLITE_OK;

    out_plan->target_column_index = out_plan->column_count;
    if (position == NULL) {
        return MYLITE_OK;
    }

    if (position->kind == MYLITE_SQL_AST_COLUMN_POSITION_FIRST) {
        out_plan->target_column_index = 0U;
        out_plan->changes_position = out_plan->target_column_index != out_plan->column_count;
        return MYLITE_OK;
    }
    if (position->kind != MYLITE_SQL_AST_COLUMN_POSITION_AFTER) {
        set_runtime_error(database, "invalid ALTER TABLE ADD COLUMN position");
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(
        child_at(position, 0U),
        out_plan->after_column_name,
        sizeof(out_plan->after_column_name),
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(out_plan->after_column_name)) {
        set_reserved_name_error(database, "column", out_plan->after_column_name);
        return MYLITE_ERROR;
    }

    rc = find_column_index(
        out_plan->columns,
        out_plan->column_count,
        out_plan->after_column_name,
        &after_column_index
    );
    if (rc != MYLITE_OK) {
        set_unknown_column_in_table_error(
            database,
            out_plan->after_column_name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }

    out_plan->target_column_index = after_column_index + 1U;
    out_plan->changes_position = out_plan->target_column_index != out_plan->column_count;
    return MYLITE_OK;
}

static int reject_nonempty_temporal_add_column_without_default(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct planned_column *column
) {
    int64_t row_count = 0;
    int rc = MYLITE_OK;

    if ((!planned_column_is_date(column) && !planned_column_is_datetime(column)) ||
        column->is_nullable || column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_NONE) {
        return MYLITE_OK;
    }

    rc = read_show_table_status_row_count(database, table, &row_count);
    if (rc != MYLITE_OK || row_count <= 0) {
        return rc;
    }

    if (planned_column_is_date(column)) {
        set_incorrect_date_value_error(database, "0000-00-00", column->name, 1U);
    } else {
        set_incorrect_datetime_value_error(database, "0000-00-00 00:00:00", column->name, 1U);
    }

    return MYLITE_ERROR;
}

static int collect_no_zero_date_default_warnings(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct zero_temporal_default_warning_filter filter,
    struct zero_temporal_default_warnings *out_warnings
) {
    int rc = MYLITE_OK;

    if (!temporal_sql_mode_has_no_zero_date(database) || temporal_sql_mode_is_strict(database)) {
        return MYLITE_OK;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &columns[column_index];

        if (column->column_id == filter.skipped_column_id ||
            !column_descriptor_has_full_zero_temporal_default(column)) {
            continue;
        }
        rc = append_zero_temporal_default_warning_name(database, out_warnings, column->name);
    }

    return rc;
}

static int reserve_zero_temporal_default_warnings(
    struct mylite_db *database,
    const struct zero_temporal_default_warnings *warnings
) {
    if (warnings == NULL || warnings->count == 0U) {
        return MYLITE_OK;
    }
    return reserve_additional_warning_capacity(database, warnings->count);
}

static int append_zero_temporal_default_warnings(
    struct mylite_db *database,
    const struct zero_temporal_default_warnings *warnings
) {
    int rc = MYLITE_OK;

    if (warnings == NULL) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < warnings->count; ++index) {
        rc = append_zero_temporal_default_warning(database, warnings->items[index].column_name);
    }

    return rc;
}

static int append_zero_temporal_default_warning(
    struct mylite_db *database,
    const char *column_name
) {
    return append_out_of_range_warning(database, column_name, 1U);
}

static bool column_descriptor_has_full_zero_temporal_default(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return false;
    }
    if (column_descriptor_is_date(column)) {
        return strcmp(column->default_text, "0000-00-00") == 0;
    }
    if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
        return strcmp(column->default_text, "0000-00-00 00:00:00") == 0;
    }
    return false;
}

static int append_zero_temporal_default_warning_name(
    struct mylite_db *database,
    struct zero_temporal_default_warnings *warnings,
    const char *column_name
) {
    struct zero_temporal_default_warning *items = NULL;

    if (warnings == NULL || column_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (warnings->count == warnings->capacity) {
        size_t new_capacity = warnings->capacity == 0U ? 4U : warnings->capacity * 2U;

        if (new_capacity < warnings->capacity ||
            new_capacity > SIZE_MAX / sizeof(warnings->items[0])) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = realloc(warnings->items, new_capacity * sizeof(warnings->items[0]));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        warnings->items = items;
        warnings->capacity = new_capacity;
    }

    snprintf(
        warnings->items[warnings->count].column_name,
        sizeof(warnings->items[warnings->count].column_name),
        "%s",
        column_name
    );
    ++warnings->count;
    return MYLITE_OK;
}

static void zero_temporal_default_warnings_deinit(struct zero_temporal_default_warnings *warnings) {
    if (warnings == NULL) {
        return;
    }

    free(warnings->items);
    *warnings = (struct zero_temporal_default_warnings){0};
}

static void planned_alter_table_add_column_deinit(struct planned_alter_table_add_column *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    zero_temporal_default_warnings_deinit(&plan->zero_temporal_default_warnings);
    *plan = (struct planned_alter_table_add_column){0};
}

static int alter_table_add_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = alter_table_add_column_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to add table column");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int alter_table_add_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor inserted_column = {0};
    int rc = MYLITE_OK;

    if (mutation == NULL || plan == NULL) {
        set_runtime_error(database, "invalid add-column mutation plan");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_insert_column_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        (int64_t)plan->column_count + 1,
        plan->column.name,
        plan->column.logical_type,
        plan->column.physical_type,
        plan->column.is_nullable,
        true,
        false,
        plan->column.default_kind,
        plan->column.default_integer,
        plan->column.default_text,
        plan->column.on_update_current_timestamp,
        plan->column.character_set_name,
        plan->column.collation_name,
        plan->column.comment,
        plan->column.is_generated,
        plan->column.generated_kind,
        plan->column.generation_expression,
        plan->column.sqlite_generation_expression,
        &inserted_column
    );
    if (rc == MYLITE_OK) {
        rc = reorder_catalog_columns_after_add(database, mutation, plan, &inserted_column);
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_add_column(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }

    return rc;
}

static int reorder_catalog_columns_after_add(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_column *plan,
    const struct mylite_catalog_column_descriptor *inserted_column
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t new_column_count = 0U;
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    if (!plan->changes_position) {
        return MYLITE_OK;
    }
    if (plan->column_count == SIZE_MAX) {
        set_runtime_error(database, "failed to plan add-column reorder");
        return MYLITE_ERROR;
    }

    new_column_count = plan->column_count + 1U;
    if (inserted_column == NULL || inserted_column->column_id <= 0 ||
        plan->target_column_index >= new_column_count ||
        new_column_count > SIZE_MAX / sizeof(*columns)) {
        set_runtime_error(database, "failed to plan add-column reorder");
        return MYLITE_ERROR;
    }

    columns = calloc(new_column_count, sizeof(*columns));
    if (columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < new_column_count; ++index) {
        if (index == plan->target_column_index) {
            columns[index] = *inserted_column;
            continue;
        }
        if (source_index >= plan->column_count) {
            free(columns);
            set_runtime_error(database, "failed to build add-column reorder");
            return MYLITE_ERROR;
        }
        columns[index] = plan->columns[source_index];
        ++source_index;
    }

    const struct mylite_catalog_column_reorder reorder = {
        .table_id = plan->table.table_id,
        .columns = columns,
        .column_count = new_column_count,
        .metadata_replaced_column_id = inserted_column->column_id,
    };

    rc = mylite_catalog_reorder_columns_in_mutation(database, mutation, &reorder);
    free(columns);
    return rc;
}

static int execute_physical_alter_table_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    char *sql = NULL;
    int rc = build_alter_table_add_column_sql(database, plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_add_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_primary_key *out_plan
) {
    const struct mylite_sql_ast_node *primary_key = child_at(statement, 1U);
    struct primary_key_definition_nodes nodes = {0};
    const struct mylite_sql_ast_node *part_list = NULL;
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct primary_key_info existing_primary_key = primary_key_info_init();
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_primary_key){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD PRIMARY KEY supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &existing_primary_key
        );
    }
    if (rc == MYLITE_OK && existing_primary_key.has_primary_key) {
        set_multiple_primary_key_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = extract_primary_key_definition_nodes(database, primary_key, &nodes);
    }
    if (rc == MYLITE_OK) {
        part_list = nodes.part_list;
        rc = apply_index_options_to_alter_primary_key_plan(database, nodes.options, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc =
            validate_hash_index_key_part_order(database, out_plan->uses_hash_index_type, part_list);
    }
    if (rc == MYLITE_OK) {
        rc = choose_sqlite_rowid_alias(
            database,
            columns,
            column_count,
            "ALTER TABLE ADD PRIMARY KEY requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_primary_key_parts_from_ast(
            database,
            out_plan,
            part_list,
            columns,
            column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_primary_key_key_length(database, out_plan);
    }

    primary_key_info_deinit(&existing_primary_key);
    free(columns);
    return rc;
}

static int append_alter_table_primary_key_parts_from_ast(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    const struct mylite_sql_ast_node *part_list,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    const struct mylite_sql_ast_node *part = child_at(part_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && part != NULL) {
        const struct mylite_sql_ast_node *column_node = secondary_index_part_column_node(part);
        enum mylite_catalog_index_sort_direction sort_direction = index_part_sort_direction(part);
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_unsupported_error(
                database,
                index_part_unsupported_message(
                    part,
                    "PRIMARY KEY supports only unqualified key columns"
                )
            );
            rc = MYLITE_ERROR;
            break;
        }
        rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
            set_reserved_name_error(database, "column", column_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = append_alter_table_primary_key_part(
                database,
                plan,
                columns,
                column_count,
                column_name,
                sort_direction
            );
        }
        part = part->next_sibling;
    }

    return rc;
}

static void planned_alter_table_add_primary_key_deinit(
    struct planned_alter_table_add_primary_key *plan
) {
    if (plan == NULL) {
        return;
    }
    free(plan->parts);
    *plan = (struct planned_alter_table_add_primary_key){0};
}

static int validate_alter_table_primary_key_key_length(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    uint64_t total = 0U;

    if (plan == NULL || plan->parts == NULL || plan->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key key parts");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];
        uint64_t part_bytes = 0U;
        int rc = column_descriptor_primary_key_part_key_bytes(database, &part->column, &part_bytes);

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int append_alter_table_primary_key_part(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *column_name,
    enum mylite_catalog_index_sort_direction sort_direction
) {
    size_t column_index = 0U;
    int rc = find_column_index(columns, column_count, column_name, &column_index);

    if (rc != MYLITE_OK) {
        set_key_column_missing_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (alter_table_primary_key_contains_column(plan, column_index)) {
        set_duplicate_column_error(database, columns[column_index].name);
        return MYLITE_ERROR;
    }

    if (column_descriptor_is_char_or_varchar(&columns[column_index])) {
        rc = validate_descriptor_string_key_column(database, &columns[column_index]);
    } else if (column_descriptor_is_json(&columns[column_index])) {
        set_json_key_error(database, columns[column_index].name);
        rc = MYLITE_ERROR;
    } else {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &columns[column_index],
            "PRIMARY KEY supports only integer columns",
            &range
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reserve_alter_table_primary_key_parts(database, plan, plan->part_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    plan->parts[plan->part_count] = (struct loaded_index_part){
        .index_column =
            {
                .column_id = columns[column_index].column_id,
                .ordinal_position = (int64_t)plan->part_count + 1,
                .sort_direction = sort_direction,
            },
        .column = columns[column_index],
        .column_index = column_index,
    };
    ++plan->part_count;

    return MYLITE_OK;
}

static int reserve_alter_table_primary_key_parts(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    size_t required_capacity
) {
    enum { initial_primary_key_part_capacity = 4 };

    struct loaded_index_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->part_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->part_capacity == 0U ? initial_primary_key_part_capacity : plan->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(plan->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->parts = parts;
    plan->part_capacity = capacity;
    return MYLITE_OK;
}

static bool alter_table_primary_key_contains_column(
    const struct planned_alter_table_add_primary_key *plan,
    size_t column_index
) {
    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (plan->parts[part_index].column_index == column_index) {
            return true;
        }
    }

    return false;
}

static int alter_table_add_primary_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key plan");
        return MYLITE_ERROR;
    }
    rc = validate_alter_table_add_primary_key_existing_rows(database, plan);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = reserve_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_add_primary_key_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to add primary key");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return append_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
}

static int alter_table_add_primary_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_primary_key *plan
) {
    char primary_key_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    int64_t primary_key_index_id = 0;
    int rc = MYLITE_OK;

    if (mutation == NULL || plan == NULL || plan->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key mutation plan");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_allocate_index_id_in_mutation(database, mutation, &primary_key_index_id);
    if (rc == MYLITE_OK) {
        rc = build_physical_index_name(
            primary_key_index_id,
            primary_key_physical_name,
            sizeof(primary_key_physical_name)
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->parts[part_index].column;
        enum mylite_catalog_column_default_kind default_kind =
            alter_table_primary_key_default_kind(column);
        int64_t default_integer = 0;
        const char *default_text = NULL;

        if (column_default_kind_has_integer_value(default_kind)) {
            default_integer = column->default_integer;
        }
        if (column_default_kind_has_text_value(default_kind)) {
            default_text = column->default_text;
        }

        rc = mylite_catalog_replace_column_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            column->column_id,
            column->name,
            column->logical_type,
            column->physical_type,
            false,
            column->is_visible,
            column->is_auto_increment,
            default_kind,
            default_integer,
            default_text,
            column->on_update_current_timestamp,
            column->character_set_name,
            column->collation_name,
            column->comment,
            column->is_generated,
            column->generated_kind,
            column->generation_expression,
            column->sqlite_generation_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_index_in_mutation(
            database,
            mutation,
            primary_key_index_id,
            plan->table.table_id,
            "PRIMARY",
            primary_key_physical_name,
            MYLITE_CATALOG_INDEX_KIND_PRIMARY,
            true,
            true,
            plan->comment,
            plan->show_create_explicit_btree,
            NULL
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        rc = mylite_catalog_insert_index_column_in_mutation(
            database,
            mutation,
            primary_key_index_id,
            plan->table.table_id,
            plan->parts[part_index].column.column_id,
            (int64_t)part_index + 1,
            NULL,
            plan->parts[part_index].index_column.sort_direction,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            execute_physical_alter_table_add_primary_key(database, plan, primary_key_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }

    return rc;
}

static int validate_alter_table_add_primary_key_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    int rc = validate_alter_table_add_primary_key_nulls(database, plan);

    if (rc == MYLITE_OK) {
        rc = validate_alter_table_add_primary_key_string_values(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_add_primary_key_duplicates(database, plan);
    }
    return rc;
}

static int validate_alter_table_add_primary_key_nulls(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_add_primary_key_null_validation_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            set_invalid_use_of_null_error(database);
            rc = MYLITE_ERROR;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_alter_table_add_primary_key_string_values(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    size_t string_part_count = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (column_descriptor_is_char_or_varchar(&plan->parts[part_index].column)) {
            ++string_part_count;
        }
    }
    if (string_part_count == 0U) {
        return MYLITE_OK;
    }

    rc = build_alter_table_add_primary_key_string_validation_sql(plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        for (size_t column_index = 0U; column_index < string_part_count; ++column_index) {
            const unsigned char *text = NULL;
            int text_length = 0;

            if (column_index > (size_t)INT_MAX) {
                set_runtime_error(database, "invalid string key validation column");
                rc = MYLITE_ERROR;
                break;
            }
            if (sqlite3_column_type(statement, (int)column_index) == SQLITE_NULL) {
                continue;
            }

            text = sqlite3_column_text(statement, (int)column_index);
            text_length = sqlite3_column_bytes(statement, (int)column_index);
            if (text_length < 0 ||
                !text_value_is_supported_string_key((const char *)text, (size_t)text_length)) {
                set_non_ascii_string_key_error(database);
                rc = MYLITE_ERROR;
                break;
            }
        }
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_alter_table_add_primary_key_duplicates(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    sqlite3_stmt *statement = NULL;
    char duplicate_value[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_add_primary_key_duplicate_validation_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = format_sqlite_key_tuple(
                statement,
                NULL,
                plan->part_count,
                duplicate_value,
                sizeof(duplicate_value)
            );
            if (rc == MYLITE_OK) {
                set_duplicate_key_error(database, plan->table.name, "PRIMARY", duplicate_value);
                rc = MYLITE_ERROR;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static enum mylite_catalog_column_default_kind alter_table_primary_key_default_kind(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_BINARY;
    }

    return MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT;
}

static int execute_physical_alter_table_add_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan,
    const char *index_physical_name
) {
    char *sql = NULL;
    int rc = build_alter_table_add_primary_key_index_sql(plan, index_physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_add_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_index *out_plan
) {
    struct alter_table_add_index_nodes nodes = {0};
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_index){0};
    out_plan->is_visible = true;
    rc = extract_alter_table_add_index_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        out_plan->kind = nodes.kind;
        out_plan->is_unique = nodes.is_unique;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_add_index_target(
            database,
            child_at(statement, 0U),
            "ALTER TABLE ADD INDEX supports only persistent base tables",
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        part_count = mylite_sql_ast_node_child_count(nodes.part_list);
        if (part_count == 0U) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_index_name_from_parts(
            database,
            &nodes,
            indexes,
            index_count,
            out_plan->index_name,
            sizeof(out_plan->index_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_loaded_add_index_spatial_kind_from_parts(
            database,
            nodes.part_list,
            nodes.options,
            columns,
            column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_temporary_spatial_add_index(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_loaded_add_index_kind_options(
            database,
            out_plan,
            nodes.kind,
            nodes.options,
            part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_index_options_to_alter_plan(database, nodes.options, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_hash_index_key_part_order(
            database,
            out_plan->uses_hash_index_type,
            nodes.part_list
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        rc = append_loaded_add_index_part(
            database,
            out_plan,
            child_at(nodes.part_list, part_index),
            columns,
            column_count
        );
    }
    if (rc == MYLITE_OK && !planned_add_index_is_fulltext(out_plan) &&
        !planned_add_index_is_spatial(out_plan)) {
        rc = validate_loaded_index_part_list(
            database,
            out_plan->parts,
            out_plan->part_count,
            out_plan->is_unique
        );
    }
    if (rc == MYLITE_OK && out_plan->is_unique) {
        rc = choose_sqlite_rowid_alias(
            database,
            columns,
            column_count,
            "ALTER TABLE ADD UNIQUE requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }
    if (rc == MYLITE_OK && out_plan->is_unique) {
        rc = validate_create_unique_index_existing_rows(database, out_plan);
    }
    if (rc == MYLITE_OK && out_plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        rc = read_show_table_status_row_count(database, &out_plan->table, &out_plan->affected_rows);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int plan_alter_table_add_index_name_from_parts(
    struct mylite_db *database,
    const struct alter_table_add_index_nodes *nodes,
    const struct loaded_index_info *indexes,
    size_t index_count,
    char *destination,
    size_t destination_size
) {
    const struct mylite_sql_ast_node *first_part = child_at(nodes->part_list, 0U);
    const struct mylite_sql_ast_node *first_column = secondary_index_part_column_node(first_part);
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (first_column == NULL || first_column->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(
            database,
            index_part_unsupported_message(
                first_part,
                add_index_unqualified_column_error(nodes->kind)
            )
        );
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(first_column, column_name, sizeof(column_name), database);
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_index_name(
            database,
            indexes,
            index_count,
            nodes->index_name_node,
            nodes->index_name_node == NULL ? column_name : "",
            destination,
            destination_size
        );
    }

    return rc;
}

static int plan_create_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_index *out_plan
) {
    struct create_index_nodes nodes = {0};
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_index){0};
    out_plan->is_visible = true;
    if (statement != NULL && statement->kind == MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT) {
        out_plan->is_unique = true;
    }
    rc = extract_create_index_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        out_plan->kind = nodes.kind;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_add_index_target(
            database,
            nodes.table_name_node,
            "CREATE INDEX supports only persistent base tables",
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        part_count = mylite_sql_ast_node_child_count(nodes.part_list_node);
        if (part_count == 0U) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_index_name(
            database,
            indexes,
            index_count,
            nodes.index_name_node,
            "",
            out_plan->index_name,
            sizeof(out_plan->index_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_loaded_add_index_spatial_kind_from_parts(
            database,
            nodes.part_list_node,
            nodes.options,
            columns,
            column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_temporary_spatial_add_index(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_loaded_add_index_kind_options(
            database,
            out_plan,
            nodes.kind,
            nodes.options,
            part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_index_options_to_alter_plan(database, nodes.options, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_hash_index_key_part_order(
            database,
            out_plan->uses_hash_index_type,
            nodes.part_list_node
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        rc = append_loaded_add_index_part(
            database,
            out_plan,
            child_at(nodes.part_list_node, part_index),
            columns,
            column_count
        );
    }
    if (rc == MYLITE_OK && !planned_add_index_is_fulltext(out_plan) &&
        !planned_add_index_is_spatial(out_plan)) {
        rc = validate_loaded_index_part_list(
            database,
            out_plan->parts,
            out_plan->part_count,
            out_plan->is_unique
        );
    }
    if (rc == MYLITE_OK && out_plan->is_unique) {
        rc = choose_sqlite_rowid_alias(
            database,
            columns,
            column_count,
            "CREATE UNIQUE INDEX requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }
    if (rc == MYLITE_OK && out_plan->is_unique) {
        rc = validate_create_unique_index_existing_rows(database, out_plan);
    }
    if (rc == MYLITE_OK && out_plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        rc = read_show_table_status_row_count(database, &out_plan->table, &out_plan->affected_rows);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int apply_loaded_add_index_spatial_kind_from_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *part_list,
    struct index_option_nodes options,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_add_index *plan
) {
    const struct mylite_sql_ast_node *first_part = child_at(part_list, 0U);
    const struct mylite_sql_ast_node *first_column = secondary_index_part_column_node(first_part);
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    enum planned_index_type_option requested_type = PLANNED_INDEX_TYPE_DEFAULT;
    size_t first_column_index = 0U;
    bool first_part_is_spatial = false;
    int rc = MYLITE_OK;

    if (first_column == NULL || first_column->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = resolve_loaded_add_index_part_column(
        database,
        first_column,
        columns,
        column_count,
        column_name,
        sizeof(column_name),
        &first_column_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    first_part_is_spatial = column_descriptor_is_spatial(&columns[first_column_index]);
    if (plan->kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
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
        if (plan->is_unique &&
            (first_part_is_spatial || requested_type == PLANNED_INDEX_TYPE_RTREE)) {
            set_spatial_unique_error(database);
            return MYLITE_ERROR;
        }
        if (!plan->is_unique &&
            (first_part_is_spatial || requested_type == PLANNED_INDEX_TYPE_RTREE)) {
            plan->kind = MYLITE_CATALOG_INDEX_KIND_SPATIAL;
        }
    }

    return MYLITE_OK;
}

static int validate_loaded_add_index_kind_options(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    enum mylite_catalog_index_kind original_kind,
    struct index_option_nodes options,
    size_t part_count
) {
    if (planned_add_index_is_spatial(plan) && part_count > 1U) {
        set_spatial_too_many_key_parts_error(database);
        return MYLITE_ERROR;
    }
    if ((original_kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
         original_kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) &&
        index_option_nodes_have_type_option(options)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int apply_index_options_to_alter_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_alter_table_add_index *plan
) {
    struct planned_index_options options = {
        .type = PLANNED_INDEX_TYPE_DEFAULT,
        .is_visible = true,
    };
    int rc = collect_index_options(database, plan == NULL ? "" : plan->index_name, nodes, &options);

    if (rc == MYLITE_OK && plan == NULL) {
        set_runtime_error(database, "invalid index plan");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        plan->is_visible = options.is_visible;
        memcpy(plan->comment, options.comment, strlen(options.comment) + 1U);
        plan->show_create_explicit_btree = options.type == PLANNED_INDEX_TYPE_BTREE;
        plan->uses_hash_index_type = options.type == PLANNED_INDEX_TYPE_HASH;
    }

    return rc;
}

static int apply_index_options_to_secondary_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_secondary_index *index
) {
    struct planned_index_options options = {
        .type = PLANNED_INDEX_TYPE_DEFAULT,
        .is_visible = true,
    };
    int rc = collect_index_options(database, index == NULL ? "" : index->name, nodes, &options);

    if (rc == MYLITE_OK && index == NULL) {
        set_runtime_error(database, "invalid index plan");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        index->is_visible = options.is_visible;
        memcpy(index->comment, options.comment, strlen(options.comment) + 1U);
        index->show_create_explicit_btree = options.type == PLANNED_INDEX_TYPE_BTREE;
        index->uses_hash_index_type = options.type == PLANNED_INDEX_TYPE_HASH;
    }

    return rc;
}

static int apply_index_options_to_create_primary_key_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_create_table *plan
) {
    if (plan == NULL) {
        set_runtime_error(database, "invalid primary-key plan");
        return MYLITE_ERROR;
    }

    return collect_primary_key_index_options(
        database,
        nodes,
        plan->primary_key_comment,
        sizeof(plan->primary_key_comment),
        &plan->primary_key_show_create_explicit_btree,
        &plan->primary_key_uses_hash_index_type
    );
}

static int apply_index_options_to_alter_primary_key_plan(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    struct planned_alter_table_add_primary_key *plan
) {
    if (plan == NULL) {
        set_runtime_error(database, "invalid primary-key plan");
        return MYLITE_ERROR;
    }

    return collect_primary_key_index_options(
        database,
        nodes,
        plan->comment,
        sizeof(plan->comment),
        &plan->show_create_explicit_btree,
        &plan->uses_hash_index_type
    );
}

static int collect_primary_key_index_options(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    char *comment,
    size_t comment_size,
    bool *out_show_create_explicit_btree,
    bool *out_uses_hash_index_type
) {
    struct planned_index_options options = {
        .type = PLANNED_INDEX_TYPE_DEFAULT,
        .is_visible = true,
    };
    int rc = collect_index_options(database, "PRIMARY", nodes, &options);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!options.is_visible) {
        set_primary_key_index_invisible_error(database);
        return MYLITE_ERROR;
    }
    if (options.type == PLANNED_INDEX_TYPE_RTREE) {
        set_spatial_index_non_geometric_error(database);
        return MYLITE_ERROR;
    }
    if (comment == NULL || out_show_create_explicit_btree == NULL ||
        out_uses_hash_index_type == NULL) {
        return MYLITE_MISUSE;
    }
    if (strlen(options.comment) >= comment_size) {
        set_runtime_error(database, "invalid primary-key comment buffer");
        return MYLITE_ERROR;
    }

    memcpy(comment, options.comment, strlen(options.comment) + 1U);
    *out_show_create_explicit_btree = options.type == PLANNED_INDEX_TYPE_BTREE;
    *out_uses_hash_index_type = options.type == PLANNED_INDEX_TYPE_HASH;
    return MYLITE_OK;
}

static int collect_index_options(
    struct mylite_db *database,
    const char *index_name,
    struct index_option_nodes nodes,
    struct planned_index_options *out_options
) {
    const struct mylite_sql_ast_node *option = NULL;
    int rc = MYLITE_OK;

    if (out_options == NULL) {
        return MYLITE_MISUSE;
    }
    out_options->type = PLANNED_INDEX_TYPE_DEFAULT;
    out_options->is_visible = true;
    out_options->comment[0] = '\0';

    if (nodes.index_type_node != NULL) {
        rc = apply_single_index_option(database, index_name, nodes.index_type_node, out_options);
    }
    if (rc != MYLITE_OK || nodes.option_list_node == NULL) {
        return rc;
    }
    if (nodes.option_list_node->kind != MYLITE_SQL_AST_INDEX_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    option = child_at(nodes.option_list_node, 0U);
    while (rc == MYLITE_OK && option != NULL) {
        rc = apply_single_index_option(database, index_name, option, out_options);
        option = option->next_sibling;
    }

    return rc;
}

static int collect_index_type_option(
    struct mylite_db *database,
    struct index_option_nodes nodes,
    enum planned_index_type_option *out_type
) {
    const struct mylite_sql_ast_node *option = NULL;
    struct planned_index_options options = {
        .type = PLANNED_INDEX_TYPE_DEFAULT,
        .is_visible = true,
    };
    int rc = MYLITE_OK;

    if (out_type == NULL) {
        return MYLITE_MISUSE;
    }
    if (nodes.index_type_node != NULL) {
        rc = apply_index_type_option(database, nodes.index_type_node, &options);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (nodes.option_list_node != NULL) {
        if (nodes.option_list_node->kind != MYLITE_SQL_AST_INDEX_OPTION_LIST) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        option = child_at(nodes.option_list_node, 0U);
        while (rc == MYLITE_OK && option != NULL) {
            if (option->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION) {
                rc = apply_index_type_option(database, option, &options);
            }
            option = option->next_sibling;
        }
    }
    if (rc == MYLITE_OK) {
        *out_type = options.type;
    }

    return rc;
}

static int apply_single_index_option(
    struct mylite_db *database,
    const char *index_name,
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
) {
    if (option == NULL || options == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (option->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION) {
        return apply_index_type_option(database, option, options);
    }
    if (option->kind == MYLITE_SQL_AST_INDEX_COMMENT_OPTION) {
        return apply_index_comment_option(database, index_name, option, options);
    }
    if (option->kind == MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION) {
        return apply_index_visibility_option(option, options);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_index_type_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
) {
    const struct mylite_sql_ast_node *type_node = child_at(option, 0U);
    char type_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (type_node == NULL || type_node->kind != MYLITE_SQL_AST_IDENTIFIER || options == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(type_node, type_name, sizeof(type_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(type_name, "BTREE")) {
        options->type = PLANNED_INDEX_TYPE_BTREE;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(type_name, "HASH")) {
        options->type = PLANNED_INDEX_TYPE_HASH;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(type_name, "RTREE")) {
        options->type = PLANNED_INDEX_TYPE_RTREE;
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_index_comment_option(
    struct mylite_db *database,
    const char *index_name,
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
) {
    if (option == NULL || options == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return decode_index_comment_literal(
        database,
        index_name,
        child_at(option, 0U),
        options->comment,
        sizeof(options->comment)
    );
}

static int apply_index_visibility_option(
    const struct mylite_sql_ast_node *option,
    struct planned_index_options *options
) {
    enum mylite_sql_ast_column_visibility visibility = MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;

    if (option == NULL || options == NULL) {
        return MYLITE_MISUSE;
    }

    visibility = mylite_sql_ast_node_column_visibility(option);
    options->is_visible = visibility != MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE;
    return MYLITE_OK;
}

static int decode_index_comment_literal(
    struct mylite_db *database,
    const char *index_name,
    const struct mylite_sql_ast_node *literal,
    char *destination,
    size_t destination_size
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    size_t decoded_character_count = 0U;
    int rc = decode_table_option_string_literal(
        database,
        literal,
        &decoded,
        (struct table_option_name_policy){
            .identifier_kind = "index comment",
            .nul_message = "index comments do not support NUL bytes",
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    decoded_length = strlen(decoded);
    if (validate_utf8_text(decoded, decoded_length, &decoded_character_count) != MYLITE_OK) {
        free(decoded);
        set_unsupported_error(database, "index comments support only valid UTF-8 text");
        return MYLITE_ERROR;
    }
    if (decoded_character_count > MYLITE_CATALOG_INDEX_COMMENT_MAX_CHARACTERS ||
        decoded_length >= destination_size) {
        free(decoded);
        set_index_comment_too_long_error(database, index_name);
        return MYLITE_ERROR;
    }

    memcpy(destination, decoded, decoded_length + 1U);
    free(decoded);
    return MYLITE_OK;
}

static int validate_hash_index_key_part_order(
    struct mylite_db *database,
    bool uses_hash_index_type,
    const struct mylite_sql_ast_node *part_list
) {
    if (!uses_hash_index_type) {
        return MYLITE_OK;
    }
    if (part_list == NULL || (part_list->kind != MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST &&
                              part_list->kind != MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (size_t part_index = 0U; part_index < mylite_sql_ast_node_child_count(part_list);
         ++part_index) {
        if (secondary_index_part_has_explicit_direction(child_at(part_list, part_index))) {
            set_fulltext_explicit_order_error(database);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool secondary_index_part_has_explicit_direction(const struct mylite_sql_ast_node *part) {
    return secondary_index_part_direction_node(part) != NULL;
}

static bool index_option_nodes_have_type_option(struct index_option_nodes nodes) {
    if (nodes.index_type_node != NULL) {
        return true;
    }

    return index_option_list_has_type_option(nodes.option_list_node);
}

static bool index_option_list_has_type_option(const struct mylite_sql_ast_node *option_list) {
    const struct mylite_sql_ast_node *option = NULL;

    if (option_list == NULL) {
        return false;
    }
    if (option_list->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION) {
        return true;
    }
    if (option_list->kind != MYLITE_SQL_AST_INDEX_OPTION_LIST) {
        return false;
    }

    option = child_at(option_list, 0U);
    while (option != NULL) {
        if (option->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION) {
            return true;
        }
        option = option->next_sibling;
    }

    return false;
}

static int resolve_add_index_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const char *base_only_error,
    struct planned_alter_table_add_index *plan
) {
    if (planned_add_index_is_fulltext(plan)) {
        return resolve_fulltext_add_index_target(database, table_node, base_only_error, plan);
    }

    return resolve_non_fulltext_add_index_target(database, table_node, base_only_error, plan);
}

static int resolve_fulltext_add_index_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const char *base_only_error,
    struct planned_alter_table_add_index *plan
) {
    int rc =
        resolve_visible_writable_table_reference(database, table_node, &plan->target, &plan->table);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_temporary_fulltext_index_error(database);
        return MYLITE_ERROR;
    }
    if (plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, base_only_error);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_non_fulltext_add_index_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const char *base_only_error,
    struct planned_alter_table_add_index *plan
) {
    int rc =
        resolve_visible_writable_table_reference(database, table_node, &plan->target, &plan->table);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_unsupported_error(database, base_only_error);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool planned_add_index_is_fulltext(const struct planned_alter_table_add_index *plan) {
    if (plan == NULL) {
        return false;
    }
    if (plan->kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return true;
    }

    return false;
}

static bool planned_add_index_is_spatial(const struct planned_alter_table_add_index *plan) {
    if (plan == NULL) {
        return false;
    }
    if (plan->kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return true;
    }

    return false;
}

static int reject_temporary_spatial_add_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    if (plan == NULL || plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY ||
        !planned_add_index_is_spatial(plan)) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, "Temporary SPATIAL indexes are not yet supported");
    return MYLITE_ERROR;
}

static int extract_alter_table_add_index_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct alter_table_add_index_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *secondary_index = child_at(statement, 1U);
    const struct mylite_sql_ast_node *child = NULL;

    *out_nodes = (struct alter_table_add_index_nodes){0};

    if (secondary_index == NULL ||
        (secondary_index->kind != MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION &&
         secondary_index->kind != MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION &&
         secondary_index->kind != MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION &&
         secondary_index->kind != MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (secondary_index->kind == MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION) {
        out_nodes->kind = MYLITE_CATALOG_INDEX_KIND_FULLTEXT;
    } else if (secondary_index->kind == MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION) {
        out_nodes->kind = MYLITE_CATALOG_INDEX_KIND_SPATIAL;
    } else {
        out_nodes->kind = MYLITE_CATALOG_INDEX_KIND_SECONDARY;
    }
    out_nodes->is_unique = secondary_index->kind == MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION;

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
    if (out_nodes->part_list == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int extract_create_index_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct create_index_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *child = NULL;

    *out_nodes = (struct create_index_nodes){0};

    if (statement == NULL || (statement->kind != MYLITE_SQL_AST_CREATE_INDEX_STATEMENT &&
                              statement->kind != MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT &&
                              statement->kind != MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT &&
                              statement->kind != MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (statement->kind == MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT) {
        out_nodes->kind = MYLITE_CATALOG_INDEX_KIND_FULLTEXT;
    } else if (statement->kind == MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT) {
        out_nodes->kind = MYLITE_CATALOG_INDEX_KIND_SPATIAL;
    } else {
        out_nodes->kind = MYLITE_CATALOG_INDEX_KIND_SECONDARY;
    }

    child = child_at(statement, 0U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_INDEX_TYPE_OPTION &&
            out_nodes->options.index_type_node == NULL) {
            out_nodes->options.index_type_node = child;
        } else if (
            child->kind == MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST &&
            out_nodes->part_list_node == NULL
        ) {
            out_nodes->part_list_node = child;
        } else if (
            child->kind == MYLITE_SQL_AST_INDEX_OPTION_LIST &&
            out_nodes->options.option_list_node == NULL
        ) {
            out_nodes->options.option_list_node = child;
        } else if (out_nodes->index_name_node == NULL && child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            out_nodes->index_name_node = child;
        } else if (out_nodes->table_name_node == NULL) {
            out_nodes->table_name_node = child;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        child = child->next_sibling;
    }

    if (out_nodes->index_name_node == NULL ||
        out_nodes->index_name_node->kind != MYLITE_SQL_AST_IDENTIFIER ||
        out_nodes->part_list_node == NULL || out_nodes->table_name_node == NULL) {
        set_unsupported_error(database, "CREATE INDEX supports only unqualified key columns");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static void planned_alter_table_add_index_deinit(struct planned_alter_table_add_index *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->parts);
    *plan = (struct planned_alter_table_add_index){0};
}

static int append_loaded_add_index_part(
    struct mylite_db *database,
    struct planned_alter_table_add_index *plan,
    const struct mylite_sql_ast_node *part,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    const struct mylite_sql_ast_node *column_node = secondary_index_part_column_node(part);
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int64_t prefix_length = 0;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(
            database,
            index_part_unsupported_message(part, add_index_unqualified_column_error(plan->kind))
        );
        return MYLITE_ERROR;
    }

    rc = resolve_loaded_add_index_part_column(
        database,
        column_node,
        columns,
        column_count,
        column_name,
        sizeof(column_name),
        &column_index
    );
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_loaded_add_index_part_column(
            database,
            plan,
            &columns[column_index],
            column_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_loaded_add_index_part_attributes(
            database,
            plan,
            part,
            &columns[column_index],
            column_name,
            &prefix_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_alter_table_add_index_parts(database, plan, plan->part_count + 1U);
    }
    if (rc == MYLITE_OK) {
        plan->parts[plan->part_count] = (struct loaded_index_part){
            .index_column = make_loaded_add_index_column_descriptor(
                plan,
                part,
                &columns[column_index],
                prefix_length
            ),
            .column = columns[column_index],
            .column_index = column_index,
        };
        ++plan->part_count;
    }

    return rc;
}

static const char *add_index_unqualified_column_error(enum mylite_catalog_index_kind kind) {
    if (kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return "FULLTEXT indexes support only unqualified key columns";
    }
    if (kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return "SPATIAL indexes support only unqualified key columns";
    }

    return "Secondary indexes support only unqualified key columns";
}

static int resolve_loaded_add_index_part_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    char *column_name,
    size_t column_name_size,
    size_t *out_column_index
) {
    int rc = copy_identifier_text(column_node, column_name, column_name_size, database);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = find_column_index(columns, column_count, column_name, out_column_index);
    if (rc != MYLITE_OK) {
        set_key_column_missing_error(database, column_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_duplicate_loaded_add_index_part_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    const struct mylite_catalog_column_descriptor *column,
    const char *column_name
) {
    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (plan->parts[part_index].index_column.column_id == column->column_id) {
            set_duplicate_column_error(database, column_name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_loaded_add_index_part_attributes(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    const struct mylite_sql_ast_node *part,
    const struct mylite_catalog_column_descriptor *column,
    const char *column_name,
    int64_t *out_prefix_length
) {
    const struct mylite_sql_ast_node *prefix_node = secondary_index_part_prefix_node(part);
    const struct mylite_sql_ast_node *direction_node = secondary_index_part_direction_node(part);
    bool is_fulltext = planned_add_index_is_fulltext(plan);
    bool is_spatial = planned_add_index_is_spatial(plan);
    uint64_t key_bytes = 0U;
    int rc = MYLITE_OK;

    *out_prefix_length = 0;
    if ((is_fulltext || is_spatial) && direction_node != NULL) {
        set_fulltext_explicit_order_error(database);
        return MYLITE_ERROR;
    }
    if (prefix_node != NULL) {
        rc = parse_secondary_index_part_prefix_length(
            database,
            prefix_node,
            column_name,
            out_prefix_length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (is_spatial) {
            set_incorrect_prefix_key_error(database);
            return MYLITE_ERROR;
        }
        if (is_fulltext) {
            return validate_alter_table_add_fulltext_column(database, column);
        }
        return validate_secondary_index_prefix_for_column_descriptor(
            database,
            column,
            *out_prefix_length,
            &key_bytes
        );
    }
    if (is_fulltext) {
        return validate_alter_table_add_fulltext_column(database, column);
    }
    if (is_spatial) {
        return validate_alter_table_add_spatial_column(database, column);
    }

    return validate_alter_table_add_index_column(database, column);
}

static struct mylite_catalog_index_column_descriptor make_loaded_add_index_column_descriptor(
    const struct planned_alter_table_add_index *plan,
    const struct mylite_sql_ast_node *part,
    const struct mylite_catalog_column_descriptor *column,
    int64_t prefix_length
) {
    struct mylite_catalog_index_column_descriptor descriptor = {
        .column_id = column->column_id,
        .ordinal_position = (int64_t)plan->part_count + 1,
        .prefix_length = prefix_length,
        .sort_direction = index_part_sort_direction(part),
    };

    if (secondary_index_part_prefix_node(part) != NULL) {
        descriptor.has_prefix_length = true;
    }
    if (planned_add_index_is_fulltext(plan) || planned_add_index_is_spatial(plan)) {
        descriptor.has_prefix_length = false;
        descriptor.prefix_length = 0;
        descriptor.sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC;
    }

    return descriptor;
}

static int reserve_planned_alter_table_add_index_parts(
    struct mylite_db *database,
    struct planned_alter_table_add_index *plan,
    size_t required_capacity
) {
    enum { initial_part_capacity = 4 };

    struct loaded_index_part *parts = NULL;
    size_t capacity = 0U;

    if (plan == NULL) {
        set_runtime_error(database, "invalid secondary-index plan");
        return MYLITE_ERROR;
    }
    if (required_capacity <= plan->part_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->part_capacity == 0U ? initial_part_capacity : plan->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(plan->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->parts = parts;
    plan->part_capacity = capacity;
    return MYLITE_OK;
}

static int validate_secondary_index_prefix_for_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    int64_t prefix_length,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    uint64_t prefix = prefix_length < 0 ? 0U : (uint64_t)prefix_length;
    uint64_t max_bytes_per_character = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid secondary-index prefix column");
        return MYLITE_ERROR;
    }
    *out_key_bytes = 0U;
    if (column_descriptor_is_char(column)) {
        rc = char_length_for_column(database, column, &column_length);
    } else if (column_descriptor_is_varchar(column)) {
        rc = varchar_length_for_column(database, column, &column_length);
    } else if (column_descriptor_is_text_family(column)) {
        return text_family_prefix_key_bytes(database, column->logical_type, prefix, out_key_bytes);
    } else if (column_descriptor_is_binary_string_family(column)) {
        return binary_string_prefix_key_bytes(
            database,
            column->logical_type,
            prefix,
            out_key_bytes
        );
    } else if (column_descriptor_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    } else {
        set_incorrect_prefix_key_error(database);
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (prefix > column_length) {
        set_incorrect_prefix_key_error(database);
        return MYLITE_ERROR;
    }
    max_bytes_per_character = logical_type_max_bytes_per_character(column->logical_type);
    if (prefix > UINT64_MAX / max_bytes_per_character) {
        set_key_too_long_error(database, innodb_max_key_length_bytes);
        return MYLITE_ERROR;
    }

    *out_key_bytes = prefix * max_bytes_per_character;
    return MYLITE_OK;
}

static int column_descriptor_index_part_key_bytes(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid secondary-index key part");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(column)) {
        rc = char_length_for_column(database, column, &column_length);
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (column_descriptor_is_varchar(column)) {
        rc = varchar_length_for_column(database, column, &column_length);
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (column_descriptor_is_text_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_binary_string_family(column)) {
        struct binary_string_type_info storage = {0};
        const struct binary_string_type_info *info =
            binary_string_type_info_for_logical_type(column->logical_type, &storage);

        return binary_string_full_key_bytes(database, info, column->name, out_key_bytes);
    }
    if (column_descriptor_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_bit(column) || column_descriptor_is_enum(column) ||
        column_descriptor_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }

    return row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "Secondary indexes do not yet support this column type",
        out_key_bytes
    );
}

static int validate_loaded_index_part_list(
    struct mylite_db *database,
    const struct loaded_index_part *parts,
    size_t part_count,
    bool is_unique
) {
    uint64_t total = 0U;

    if (parts == NULL || part_count == 0U) {
        set_runtime_error(database, "invalid secondary-index key parts");
        return MYLITE_ERROR;
    }
    if (is_unique) {
        int rc = validate_loaded_unique_index_part_list(database, parts, part_count);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    for (size_t part_index = 0U; part_index < part_count; ++part_index) {
        const struct loaded_index_part *part = &parts[part_index];
        uint64_t part_bytes = 0U;
        int rc = MYLITE_OK;

        if (part->index_column.has_prefix_length) {
            rc = validate_secondary_index_prefix_for_column_descriptor(
                database,
                &part->column,
                part->index_column.prefix_length,
                &part_bytes
            );
        } else {
            rc = column_descriptor_index_part_key_bytes(database, &part->column, &part_bytes);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_loaded_unique_index_part_list(
    struct mylite_db *database,
    const struct loaded_index_part *parts,
    size_t part_count
) {
    if (parts == NULL || part_count == 0U) {
        set_runtime_error(database, "invalid unique-index key parts");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_create_table_primary_key_key_length(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    uint64_t total = 0U;

    if (plan == NULL || !plan->has_primary_key || plan->primary_key_part_count == 0U) {
        set_runtime_error(database, "invalid primary-key descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < plan->primary_key_part_count; ++part_index) {
        const struct planned_primary_key_part *part = &plan->primary_key_parts[part_index];
        const struct planned_column *column = NULL;
        uint64_t part_bytes = 0U;
        int rc = MYLITE_OK;

        if (part->column_index >= plan->column_count) {
            set_runtime_error(database, "invalid primary-key key part");
            return MYLITE_ERROR;
        }
        column = &plan->columns[part->column_index];
        rc = planned_primary_key_part_key_bytes(database, column, &part_bytes);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int planned_primary_key_part_key_bytes(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid primary-key key part");
        return MYLITE_ERROR;
    }
    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "string keys support only baseline CHAR descriptors",
            &column_length
        );
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (planned_column_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "string keys support only baseline VARCHAR descriptors",
            &column_length
        );
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (planned_column_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }

    return row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "PRIMARY KEY supports only integer columns",
        out_key_bytes
    );
}

static int column_descriptor_primary_key_part_key_bytes(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *out_key_bytes
) {
    size_t column_length = 0U;
    int rc = MYLITE_OK;

    if (column == NULL || out_key_bytes == NULL) {
        set_runtime_error(database, "invalid primary-key key part");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(column)) {
        rc = char_length_for_column(database, column, &column_length);
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (column_descriptor_is_varchar(column)) {
        rc = varchar_length_for_column(database, column, &column_length);
        if (rc == MYLITE_OK) {
            *out_key_bytes = (uint64_t)column_length *
                             logical_type_max_bytes_per_character(column->logical_type);
        }
        return rc;
    }
    if (column_descriptor_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }

    return row_size_for_column_descriptor(
        database,
        column->logical_type,
        column->physical_type,
        "PRIMARY KEY supports only integer columns",
        out_key_bytes
    );
}

static int add_secondary_index_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    bool physical_schema_changed = false;
    int rc = MYLITE_OK;

    if (plan->part_count == 0U || plan->index_name[0] == '\0') {
        set_runtime_error(database, "invalid secondary-index plan");
        return MYLITE_ERROR;
    }
    if (plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        return add_temporary_secondary_index_from_plan(database, plan);
    }

    rc = reserve_fulltext_add_warning_if_needed(database, plan);
    if (rc == MYLITE_OK) {
        rc = reserve_spatial_add_warning_if_needed(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = add_persistent_secondary_index_in_mutation(
            database,
            &mutation,
            plan,
            &physical_schema_changed
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to add index");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (planned_add_index_is_fulltext(plan)) {
        rc = append_fulltext_add_warning_if_needed(database, plan);
    } else if (planned_add_index_is_spatial(plan)) {
        rc = append_spatial_add_warning_if_needed(database, plan);
    } else {
        if (physical_schema_changed) {
            ++database->session.sqlite_schema_generation;
        }
        rc = append_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
    }

    return rc;
}

static int add_persistent_secondary_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_index *plan,
    bool *out_physical_schema_changed
) {
    char index_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    int64_t index_id = 0;
    int rc = MYLITE_OK;

    if (mutation == NULL || plan == NULL || out_physical_schema_changed == NULL) {
        set_runtime_error(database, "invalid persistent secondary-index mutation plan");
        return MYLITE_ERROR;
    }

    *out_physical_schema_changed = false;
    rc = mylite_catalog_allocate_index_id_in_mutation(database, mutation, &index_id);
    if (rc == MYLITE_OK) {
        rc = build_physical_index_name(index_id, index_physical_name, sizeof(index_physical_name));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_index_in_mutation(
            database,
            mutation,
            index_id,
            plan->table.table_id,
            plan->index_name,
            index_physical_name,
            plan->kind,
            plan->is_unique,
            plan->is_visible,
            plan->comment,
            plan->show_create_explicit_btree,
            NULL
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];
        const int64_t *prefix_length = NULL;

        if (part->index_column.has_prefix_length) {
            prefix_length = &part->index_column.prefix_length;
        }
        if (part->column.column_id <= 0) {
            set_runtime_error(database, "invalid secondary-index key part");
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_insert_index_column_in_mutation(
            database,
            mutation,
            index_id,
            plan->table.table_id,
            part->column.column_id,
            (int64_t)part_index + 1,
            prefix_length,
            part->index_column.sort_direction,
            NULL
        );
    }
    if (rc == MYLITE_OK && !planned_add_index_is_fulltext(plan) &&
        !planned_add_index_is_spatial(plan)) {
        rc = execute_physical_add_index(database, plan, index_physical_name);
        *out_physical_schema_changed = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }

    return rc;
}

static int add_temporary_secondary_index_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    struct mylite_catalog_index_descriptor index = {0};
    struct mylite_catalog_index_column_descriptor *index_columns = NULL;
    char index_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    int64_t index_id = 0;
    bool physical_index_created = false;
    int rc = MYLITE_OK;

    if (plan == NULL || plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY ||
        plan->part_count == 0U || plan->index_name[0] == '\0') {
        set_runtime_error(database, "invalid temporary secondary-index plan");
        return MYLITE_ERROR;
    }
    if (plan->part_count > SIZE_MAX / sizeof(*index_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = reserve_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
    if (rc == MYLITE_OK) {
        rc = mylite_temporary_catalog_allocate_index_identity(
            &database->session.temporary_catalog,
            &index_id,
            index_physical_name,
            sizeof(index_physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        index_columns = calloc(plan->part_count, sizeof(*index_columns));
        if (index_columns == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        index = (struct mylite_catalog_index_descriptor){
            .index_id = index_id,
            .table_id = plan->table.table_id,
            .kind = plan->kind,
            .is_unique = plan->is_unique,
            .is_visible = plan->is_visible,
            .show_create_explicit_btree = plan->show_create_explicit_btree,
        };
        snprintf(index.name, sizeof(index.name), "%s", plan->index_name);
        snprintf(index.physical_name, sizeof(index.physical_name), "%s", index_physical_name);
        snprintf(index.comment, sizeof(index.comment), "%s", plan->comment);
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];
        int64_t index_column_id = 0;

        if (part->column.column_id >= 0) {
            set_runtime_error(database, "invalid temporary secondary-index key part");
            rc = MYLITE_ERROR;
            break;
        }
        rc = mylite_temporary_catalog_allocate_index_column_id(
            &database->session.temporary_catalog,
            &index_column_id
        );
        if (rc != MYLITE_OK) {
            break;
        }
        index_columns[part_index] = (struct mylite_catalog_index_column_descriptor){
            .index_column_id = index_column_id,
            .index_id = index_id,
            .table_id = plan->table.table_id,
            .column_id = part->column.column_id,
            .ordinal_position = (int64_t)part_index + 1,
            .has_prefix_length = part->index_column.has_prefix_length,
            .prefix_length = part->index_column.prefix_length,
            .sort_direction = part->index_column.sort_direction,
        };
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_add_index(database, plan, index_physical_name);
        physical_index_created = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_temporary_catalog_append_index(
            &database->session.temporary_catalog,
            plan->table.table_id,
            &index,
            index_columns,
            plan->part_count
        );
    }
    if (rc != MYLITE_OK) {
        if (physical_index_created) {
            (void)execute_physical_drop_index_by_name(database, index_physical_name);
        }
        free(index_columns);
        set_internal_error_if_clear(database, rc, "failed to add temporary index");
        return rc;
    }

    free(index_columns);
    ++database->session.sqlite_schema_generation;
    return append_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
}

static int reserve_fulltext_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    size_t warning_count = 0U;

    if (!planned_add_index_is_fulltext(plan) || plan->table.fulltext_doc_id_initialized) {
        return MYLITE_OK;
    }
    warning_count = mylite_diagnostics_warning_count(diagnostics);
    if (warning_count == SIZE_MAX) {
        mylite_diagnostics_set_error(diagnostics, MYLITE_NOMEM, "HY001", "too many warnings");
        return MYLITE_NOMEM;
    }

    return mylite_diagnostics_reserve_warning_capacity(diagnostics, warning_count + 1U);
}

static int append_fulltext_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    if (!planned_add_index_is_fulltext(plan) || plan->table.fulltext_doc_id_initialized) {
        return MYLITE_OK;
    }

    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_innodb_rebuild_fulltext,
        "HY000",
        "InnoDB rebuilding table to add column FTS_DOC_ID"
    );
}

static size_t planned_create_table_spatial_index_warning_count(
    const struct planned_create_table *plan
) {
    size_t warning_count = 0U;

    if (plan == NULL) {
        return 0U;
    }
    if (plan->suppress_spatial_index_warnings) {
        return 0U;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (plan->secondary_indexes[index].kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            ++warning_count;
        }
    }

    return warning_count;
}

static size_t planned_create_table_hash_index_warning_count(
    const struct planned_create_table *plan
) {
    size_t warning_count = 0U;

    if (plan == NULL) {
        return 0U;
    }
    if (plan->primary_key_uses_hash_index_type) {
        ++warning_count;
    }
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        if (plan->secondary_indexes[index].uses_hash_index_type) {
            ++warning_count;
        }
    }

    return warning_count;
}

static int reserve_spatial_index_warnings(struct mylite_db *database, size_t warning_count) {
    return reserve_additional_warning_capacity(database, warning_count);
}

static int append_spatial_index_warnings(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_OK;
    }
    if (plan->suppress_spatial_index_warnings) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->secondary_index_count; ++index) {
        const struct planned_secondary_index *secondary = &plan->secondary_indexes[index];

        if (secondary->kind != MYLITE_CATALOG_INDEX_KIND_SPATIAL || secondary->part_count == 0U ||
            secondary->parts[0].column_index >= plan->column_count) {
            continue;
        }
        rc = append_spatial_index_no_srid_warning(
            database,
            plan->columns[secondary->parts[0].column_index].name
        );
    }

    return rc;
}

static int append_spatial_index_no_srid_warning(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "The spatial index on column '%s' will not be used by the query optimizer since the "
        "column does not have an SRID attribute. Consider adding an SRID attribute to the column.",
        column_name == NULL ? "" : column_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format SPATIAL index warning");
        return MYLITE_ERROR;
    }

    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_spatial_index_no_srid,
        "HY000",
        message
    );
}

static int reserve_hash_index_warnings(struct mylite_db *database, size_t additional_count) {
    return reserve_additional_warning_capacity(database, additional_count);
}

static int append_hash_index_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_hash_index_unsupported,
            "HY000",
            "This storage engine does not support the HASH index algorithm, storage engine "
            "default was used instead."
        );
    }

    return rc;
}

static int reserve_hash_index_warning_if_needed(
    struct mylite_db *database,
    bool uses_hash_index_type
) {
    size_t warning_count = 0U;

    if (uses_hash_index_type) {
        warning_count = 1U;
    }

    return reserve_hash_index_warnings(database, warning_count);
}

static int append_hash_index_warning_if_needed(
    struct mylite_db *database,
    bool uses_hash_index_type
) {
    size_t warning_count = 0U;

    if (uses_hash_index_type) {
        warning_count = 1U;
    }

    return append_hash_index_warnings(database, warning_count);
}

static int reserve_spatial_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    size_t warning_count = 0U;

    if (planned_add_index_is_spatial(plan)) {
        warning_count = 1U;
    }

    return reserve_spatial_index_warnings(database, warning_count);
}

static int append_spatial_add_warning_if_needed(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    if (!planned_add_index_is_spatial(plan)) {
        return MYLITE_OK;
    }
    if (plan == NULL || plan->part_count == 0U) {
        set_runtime_error(database, "invalid SPATIAL index plan");
        return MYLITE_ERROR;
    }

    return append_spatial_index_no_srid_warning(database, plan->parts[0].column.name);
}

static int reserve_additional_warning_capacity(
    struct mylite_db *database,
    size_t additional_count
) {
    struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    size_t warning_count = 0U;

    if (additional_count == 0U) {
        return MYLITE_OK;
    }

    warning_count = mylite_diagnostics_warning_count(diagnostics);
    if (warning_count > SIZE_MAX - additional_count) {
        mylite_diagnostics_set_error(diagnostics, MYLITE_NOMEM, "HY001", "too many warnings");
        return MYLITE_NOMEM;
    }

    return mylite_diagnostics_reserve_warning_capacity(
        diagnostics,
        warning_count + additional_count
    );
}

static int plan_alter_table_add_foreign_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_foreign_key *out_plan
) {
    struct foreign_key_definition_nodes nodes = {0};
    struct foreign_key_column_names names = {0};
    struct alter_table_add_foreign_key_child_metadata child_metadata = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_foreign_key){0};
    rc =
        parse_alter_table_add_foreign_key_definition(database, statement, &nodes, &names, out_plan);
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_foreign_key_target(database, statement, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = load_alter_table_add_foreign_key_child_metadata(database, out_plan, &child_metadata);
    }
    if (rc == MYLITE_OK) {
        rc = ensure_alter_table_foreign_key_name(database, &child_metadata, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_alter_table_foreign_key_name(database, &child_metadata, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_foreign_key_child_column(
            database,
            &child_metadata,
            &names,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_foreign_key_set_null_columns(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_foreign_key_parent(database, &nodes, &names, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = find_alter_table_foreign_key_parent_index(database, out_plan, &out_plan->parent_index);
    }
    if (rc == MYLITE_OK) {
        rc = choose_alter_table_foreign_key_child_index(
            database,
            child_metadata.indexes,
            child_metadata.index_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_foreign_key_existing_child_rows(database, out_plan, false);
    }

    foreign_key_column_names_deinit(&names);
    alter_table_add_foreign_key_child_metadata_deinit(&child_metadata);
    if (rc != MYLITE_OK) {
        planned_alter_table_add_foreign_key_deinit(out_plan);
    }
    return rc;
}

static int parse_alter_table_add_foreign_key_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct foreign_key_definition_nodes *out_nodes,
    struct foreign_key_column_names *out_names,
    struct planned_alter_table_add_foreign_key *out_plan
) {
    const struct mylite_sql_ast_node *foreign_key = child_at(statement, 1U);
    int rc = extract_foreign_key_definition_nodes(database, foreign_key, out_nodes);

    if (rc == MYLITE_OK && out_nodes->constraint_name_node != NULL) {
        out_plan->has_explicit_name = true;
        rc = copy_identifier_text(
            out_nodes->constraint_name_node,
            out_plan->name,
            sizeof(out_plan->name),
            database
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_index_name(
            database,
            out_nodes->index_name_node,
            out_plan->child_index_name,
            sizeof(out_plan->child_index_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = parse_foreign_key_actions(
            database,
            out_nodes->action_list_node,
            out_plan->update_rule,
            sizeof(out_plan->update_rule),
            out_plan->delete_rule,
            sizeof(out_plan->delete_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_part_names(
            database,
            out_nodes->child_parts_node,
            &out_names->child_parts,
            &out_names->child_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_foreign_key_part_names(
            database,
            out_nodes->parent_parts_node,
            &out_names->parent_parts,
            &out_names->parent_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_foreign_key_part_counts(
            database,
            out_names->child_part_count,
            out_names->parent_part_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_foreign_key_child_parts(database, out_names);
    }

    return rc;
}

static int plan_alter_table_add_foreign_key_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_foreign_key *out_plan
) {
    int rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD FOREIGN KEY supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int load_alter_table_add_foreign_key_child_metadata(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    struct alter_table_add_foreign_key_child_metadata *out_metadata
) {
    int rc = MYLITE_OK;

    *out_metadata = (struct alter_table_add_foreign_key_child_metadata){0};
    rc = load_table_columns(
        database,
        plan->table.table_id,
        &out_metadata->columns,
        &out_metadata->column_count
    );
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            plan->table.table_id,
            out_metadata->columns,
            out_metadata->column_count,
            &out_metadata->indexes,
            &out_metadata->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            plan->table.table_id,
            out_metadata->columns,
            out_metadata->column_count,
            &out_metadata->foreign_keys,
            &out_metadata->foreign_key_count
        );
    }

    return rc;
}

static void alter_table_add_foreign_key_child_metadata_deinit(
    struct alter_table_add_foreign_key_child_metadata *metadata
) {
    if (metadata == NULL) {
        return;
    }

    loaded_foreign_key_infos_deinit(&metadata->foreign_keys, &metadata->foreign_key_count);
    loaded_index_infos_deinit(&metadata->indexes, &metadata->index_count);
    free(metadata->columns);
    *metadata = (struct alter_table_add_foreign_key_child_metadata){0};
}

static int ensure_alter_table_foreign_key_name(
    struct mylite_db *database,
    const struct alter_table_add_foreign_key_child_metadata *metadata,
    struct planned_alter_table_add_foreign_key *plan
) {
    unsigned int suffix = 1U;

    if (plan->name[0] != '\0') {
        return MYLITE_OK;
    }
    while (suffix < UINT_MAX) {
        char candidate[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        bool used = false;
        int written =
            snprintf(candidate, sizeof(candidate), "%s_ibfk_%u", plan->target.table_name, suffix);

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_identifier_too_long_error(database, "constraint");
            return MYLITE_ERROR;
        }
        for (size_t index = 0U; index < metadata->foreign_key_count; ++index) {
            if (text_equals_ascii_case_insensitive(
                    metadata->foreign_keys[index].foreign_key.name,
                    candidate
                )) {
                used = true;
                break;
            }
        }
        if (!used) {
            int copied = snprintf(plan->name, sizeof(plan->name), "%s", candidate);

            if (copied < 0 || (size_t)copied >= sizeof(plan->name)) {
                set_identifier_too_long_error(database, "constraint");
                return MYLITE_ERROR;
            }
            return MYLITE_OK;
        }
        ++suffix;
    }

    set_identifier_too_long_error(database, "constraint");
    return MYLITE_ERROR;
}

static int reject_duplicate_alter_table_foreign_key_name(
    struct mylite_db *database,
    const struct alter_table_add_foreign_key_child_metadata *metadata,
    const struct planned_alter_table_add_foreign_key *plan
) {
    for (size_t index = 0U; index < metadata->foreign_key_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                metadata->foreign_keys[index].foreign_key.name,
                plan->name
            )) {
            set_duplicate_foreign_key_error(database, plan->name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_foreign_key_set_null_columns(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan
) {
    if (!foreign_key_rule_is_set_null(plan->delete_rule) &&
        !foreign_key_rule_is_set_null(plan->update_rule)) {
        return MYLITE_OK;
    }

    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (!plan->parts[part_index].child_column.is_nullable) {
            set_foreign_key_set_null_not_nullable_error(
                database,
                plan->parts[part_index].child_column.name,
                plan->name
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int plan_alter_table_add_foreign_key_child_column(
    struct mylite_db *database,
    const struct alter_table_add_foreign_key_child_metadata *metadata,
    const struct foreign_key_column_names *names,
    struct planned_alter_table_add_foreign_key *out_plan
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < names->child_part_count;
         ++part_index) {
        size_t column_index = 0U;

        rc = find_column_index(
            metadata->columns,
            metadata->column_count,
            names->child_parts[part_index].name,
            &column_index
        );
        if (rc != MYLITE_OK) {
            set_key_column_missing_error(database, names->child_parts[part_index].name);
            return MYLITE_ERROR;
        }
        rc = append_alter_table_foreign_key_part(
            database,
            out_plan,
            &metadata->columns[column_index],
            column_index
        );
    }

    return rc;
}

static int plan_alter_table_add_foreign_key_parent(
    struct mylite_db *database,
    const struct foreign_key_definition_nodes *nodes,
    const struct foreign_key_column_names *names,
    struct planned_alter_table_add_foreign_key *out_plan
) {
    struct table_name_resolution parent_resolution = {0};
    struct mylite_catalog_column_descriptor *parent_columns = NULL;
    size_t parent_column_count = 0U;
    int rc = resolve_table_name(database, nodes->parent_table_node, &parent_resolution);

    if (rc == MYLITE_OK &&
        parent_resolution.schema.schema_id != out_plan->target.schema.schema_id) {
        set_unsupported_error(database, "FOREIGN KEY does not yet support cross-schema references");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            parent_resolution.schema.schema_id,
            parent_resolution.table_name,
            &out_plan->parent_table
        );
        if (rc != MYLITE_OK) {
            set_failed_to_open_referenced_table_error(database, parent_resolution.table_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->parent_table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "FOREIGN KEY references only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->parent_table.table_id,
            &parent_columns,
            &parent_column_count
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < names->parent_part_count;
         ++part_index) {
        size_t parent_column_index = 0U;

        if (part_index >= out_plan->part_count) {
            set_runtime_error(database, "invalid foreign-key descriptor");
            rc = MYLITE_ERROR;
            break;
        }
        rc = find_column_index(
            parent_columns,
            parent_column_count,
            names->parent_parts[part_index].name,
            &parent_column_index
        );
        if (rc != MYLITE_OK) {
            set_key_column_missing_error(database, names->parent_parts[part_index].name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = validate_foreign_key_integer_columns(
                database,
                out_plan->parts[part_index].child_column.logical_type,
                parent_columns[parent_column_index].logical_type
            );
        }
        if (rc == MYLITE_OK) {
            out_plan->parts[part_index].parent_column = parent_columns[parent_column_index];
            out_plan->parts[part_index].parent_column_index = parent_column_index;
        }
    }

    free(parent_columns);
    return rc;
}

static void planned_alter_table_add_foreign_key_deinit(
    struct planned_alter_table_add_foreign_key *plan
) {
    if (plan == NULL) {
        return;
    }
    planned_alter_table_add_index_deinit(&plan->child_index_plan);
    free(plan->parts);
    *plan = (struct planned_alter_table_add_foreign_key){0};
}

static int alter_table_add_foreign_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    char child_index_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY] = "";
    int64_t foreign_key_id = 0;
    int64_t child_index_id = plan->child_index.index_id;
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK && plan->create_child_index) {
        rc = mylite_catalog_allocate_index_id_in_mutation(database, &mutation, &child_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_allocate_foreign_key_id_in_mutation(
            database,
            &mutation,
            &foreign_key_id
        );
    }
    if (rc == MYLITE_OK && plan->create_child_index) {
        rc = build_physical_index_name(
            child_index_id,
            child_index_physical_name,
            sizeof(child_index_physical_name)
        );
    }
    if (rc == MYLITE_OK && plan->create_child_index) {
        rc = mylite_catalog_insert_index_in_mutation(
            database,
            &mutation,
            child_index_id,
            plan->table.table_id,
            plan->child_index_plan.index_name,
            child_index_physical_name,
            MYLITE_CATALOG_INDEX_KIND_SECONDARY,
            false,
            true,
            "",
            false,
            NULL
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && plan->create_child_index &&
                                 part_index < plan->child_index_plan.part_count;
         ++part_index) {
        rc = mylite_catalog_insert_index_column_in_mutation(
            database,
            &mutation,
            child_index_id,
            plan->table.table_id,
            plan->child_index_plan.parts[part_index].column.column_id,
            (int64_t)part_index + 1,
            NULL,
            plan->child_index_plan.parts[part_index].index_column.sort_direction,
            NULL
        );
    }
    if (rc == MYLITE_OK && plan->create_child_index) {
        rc = execute_physical_add_index(
            database,
            &plan->child_index_plan,
            child_index_physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = insert_alter_table_foreign_key_catalog_rows(
            database,
            &mutation,
            foreign_key_id,
            child_index_id,
            plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to add foreign key");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;
    return MYLITE_OK;
}

static int choose_alter_table_foreign_key_child_index(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    struct planned_alter_table_add_foreign_key *plan
) {
    for (size_t index = 0U; index < index_count; ++index) {
        const struct loaded_index_info *candidate = &indexes[index];
        bool matches = false;

        if (candidate->part_count < plan->part_count) {
            continue;
        }
        matches = true;
        for (size_t part_index = 0U; matches && part_index < plan->part_count; ++part_index) {
            if (candidate->parts[part_index].index_column.has_prefix_length ||
                candidate->parts[part_index].column.column_id !=
                    plan->parts[part_index].child_column.column_id) {
                matches = false;
            }
        }
        if (matches) {
            plan->child_index = candidate->index;
            return MYLITE_OK;
        }
    }

    return prepare_alter_table_foreign_key_child_index_plan(database, indexes, index_count, plan);
}

static int prepare_alter_table_foreign_key_child_index_plan(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    struct planned_alter_table_add_foreign_key *plan
) {
    struct planned_alter_table_add_index *index_plan = &plan->child_index_plan;
    const char *default_name = NULL;
    int rc = MYLITE_OK;

    if (plan->part_count == 0U) {
        set_runtime_error(database, "invalid foreign-key descriptor");
        return MYLITE_ERROR;
    }

    default_name = plan->parts[0].child_column.name;
    if (plan->has_explicit_name) {
        default_name = plan->name;
    } else if (plan->child_index_name[0] != '\0') {
        default_name = plan->child_index_name;
    }

    if (text_equals_ascii_case_insensitive(default_name, "PRIMARY")) {
        set_incorrect_index_name_error(database, default_name);
        return MYLITE_ERROR;
    }
    if (loaded_index_name_is_used(indexes, index_count, default_name)) {
        set_duplicate_key_name_error(database, default_name);
        return MYLITE_ERROR;
    }
    index_plan->target = plan->target;
    index_plan->table = plan->table;
    snprintf(index_plan->index_name, sizeof(index_plan->index_name), "%s", default_name);
    rc = reserve_planned_alter_table_add_index_parts(database, index_plan, plan->part_count);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        index_plan->parts[part_index] = (struct loaded_index_part){
            .index_column =
                {
                    .column_id = plan->parts[part_index].child_column.column_id,
                    .ordinal_position = (int64_t)part_index + 1,
                    .has_prefix_length = false,
                    .prefix_length = 0,
                    .sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC,
                },
            .column = plan->parts[part_index].child_column,
            .column_index = plan->parts[part_index].child_column_index,
        };
        ++index_plan->part_count;
    }
    if (rc == MYLITE_OK) {
        index_plan->is_unique = false;
        plan->create_child_index = true;
    }

    return rc;
}

static int append_alter_table_foreign_key_part(
    struct mylite_db *database,
    struct planned_alter_table_add_foreign_key *plan,
    const struct mylite_catalog_column_descriptor *child_column,
    size_t child_column_index
) {
    int rc = reserve_alter_table_foreign_key_parts(database, plan, plan->part_count + 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }

    plan->parts[plan->part_count] = (struct loaded_foreign_key_part){
        .child_column = *child_column,
        .child_column_index = child_column_index,
    };
    ++plan->part_count;
    return MYLITE_OK;
}

static int reserve_alter_table_foreign_key_parts(
    struct mylite_db *database,
    struct planned_alter_table_add_foreign_key *plan,
    size_t required_capacity
) {
    enum { initial_alter_table_foreign_key_part_capacity = 4 };

    struct loaded_foreign_key_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->part_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->part_capacity == 0U ? initial_alter_table_foreign_key_part_capacity
                                         : plan->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(plan->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->parts = parts;
    plan->part_capacity = capacity;
    return MYLITE_OK;
}

static int validate_foreign_key_existing_child_rows(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    bool parent_write
) {
    struct loaded_foreign_key_info loaded = {
        .child_table = plan->table,
        .parent_table = plan->parent_table,
        .parts = plan->parts,
        .part_count = plan->part_count,
    };

    return validate_foreign_key_references(database, &loaded, parent_write);
}

static int insert_alter_table_foreign_key_catalog_rows(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_index_id,
    const struct planned_alter_table_add_foreign_key *plan
) {
    int rc = mylite_catalog_insert_foreign_key_in_mutation(
        database,
        mutation,
        foreign_key_id,
        plan->table.table_id,
        plan->parent_table.table_id,
        plan->name,
        plan->parent_index.index_id,
        child_index_id,
        plan->update_rule,
        plan->delete_rule,
        "NONE",
        NULL
    );

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        rc = mylite_catalog_insert_foreign_key_column_in_mutation(
            database,
            mutation,
            foreign_key_id,
            plan->table.table_id,
            plan->parent_table.table_id,
            plan->parts[part_index].child_column.column_id,
            plan->parts[part_index].parent_column.column_id,
            (int64_t)part_index + 1,
            (int64_t)part_index + 1,
            NULL
        );
    }

    return rc;
}

static int plan_alter_table_drop_foreign_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_foreign_key *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    char foreign_key_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    size_t resolved_foreign_key = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_foreign_key){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            foreign_key_name,
            sizeof(foreign_key_name),
            database
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE DROP FOREIGN KEY supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = find_loaded_foreign_key_by_name(
            foreign_keys,
            foreign_key_count,
            foreign_key_name,
            &resolved_foreign_key
        );
        if (rc != MYLITE_OK) {
            set_cant_drop_field_or_key_error(database, foreign_key_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && foreign_keys == NULL) {
        set_runtime_error(database, "invalid foreign-key descriptor lookup");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->foreign_key = foreign_keys[resolved_foreign_key];
        foreign_keys[resolved_foreign_key] = (struct loaded_foreign_key_info){0};
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_foreign_key_deinit(out_plan);
    }
    return rc;
}

static void planned_alter_table_drop_foreign_key_deinit(
    struct planned_alter_table_drop_foreign_key *plan
) {
    if (plan == NULL) {
        return;
    }
    loaded_foreign_key_info_deinit(&plan->foreign_key);
    *plan = (struct planned_alter_table_drop_foreign_key){0};
}

static int alter_table_drop_foreign_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_foreign_key *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_foreign_key_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->foreign_key.foreign_key.foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop foreign key");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_drop_constraint(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_constraint *out_plan
) {
    struct alter_table_drop_constraint_context context = {0};
    struct alter_table_drop_constraint_resolution resolution = {
        .kind = PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE,
        .match_count = 0U,
        .unique_index = 0U,
    };
    char constraint_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_constraint){0};
    rc = load_alter_table_drop_constraint_context(
        database,
        statement,
        &context,
        constraint_name,
        sizeof(constraint_name)
    );
    if (rc == MYLITE_OK) {
        rc = resolve_alter_table_drop_constraint(&context, constraint_name, &resolution);
    }
    if (rc == MYLITE_OK && resolution.match_count == 0U) {
        set_constraint_does_not_exist_error(database, constraint_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && resolution.match_count > 1U) {
        set_drop_constraint_ambiguous_error(database, constraint_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_drop_constraint_algorithm_lock_options(
            database,
            statement,
            resolution.kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = populate_alter_table_drop_constraint_plan(
            database,
            statement,
            &context,
            &resolution,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->kind = resolution.kind;
    }

    alter_table_drop_constraint_context_deinit(&context);
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_constraint_deinit(out_plan);
    }
    return rc;
}

static int load_alter_table_drop_constraint_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct alter_table_drop_constraint_context *context,
    char *constraint_name,
    size_t constraint_name_size
) {
    int rc = resolve_writable_table_name(database, child_at(statement, 0U), &context->target);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(context->target.table_name)) {
        set_reserved_name_error(database, "table", context->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            constraint_name,
            constraint_name_size,
            database
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            context->target.schema.schema_id,
            context->target.table_name,
            &context->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                context->target.schema.name,
                context->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && context->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE DROP CONSTRAINT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            context->table.table_id,
            &context->columns,
            &context->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            context->table.table_id,
            context->columns,
            context->column_count,
            &context->indexes,
            &context->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            context->table.table_id,
            context->columns,
            context->column_count,
            &context->foreign_keys,
            &context->foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_check_constraint_infos(
            database,
            context->table.table_id,
            &context->check_constraints,
            &context->check_constraint_count
        );
    }
    return rc;
}

static int resolve_alter_table_drop_constraint(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
) {
    *resolution = (struct alter_table_drop_constraint_resolution){
        .kind = PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE,
        .match_count = 0U,
        .unique_index = 0U,
    };
    match_alter_table_drop_constraint_indexes(context, constraint_name, resolution);
    match_alter_table_drop_constraint_foreign_keys(context, constraint_name, resolution);
    match_alter_table_drop_constraint_checks(context, constraint_name, resolution);
    return MYLITE_OK;
}

static void match_alter_table_drop_constraint_indexes(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
) {
    for (size_t index = 0U; index < context->index_count; ++index) {
        const struct loaded_index_info *candidate = &context->indexes[index];

        if (!text_equals_ascii_case_insensitive(candidate->index.name, constraint_name)) {
            continue;
        }
        if (candidate->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            resolution->kind = PLANNED_ALTER_TABLE_DROP_CONSTRAINT_PRIMARY_KEY;
            ++resolution->match_count;
        } else if (
            candidate->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
            candidate->index.is_unique
        ) {
            resolution->kind = PLANNED_ALTER_TABLE_DROP_CONSTRAINT_UNIQUE_INDEX;
            resolution->unique_index = index;
            ++resolution->match_count;
        }
    }
}

static void match_alter_table_drop_constraint_foreign_keys(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
) {
    for (size_t index = 0U; index < context->foreign_key_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                context->foreign_keys[index].foreign_key.name,
                constraint_name
            )) {
            resolution->kind = PLANNED_ALTER_TABLE_DROP_CONSTRAINT_FOREIGN_KEY;
            ++resolution->match_count;
        }
    }
}

static void match_alter_table_drop_constraint_checks(
    const struct alter_table_drop_constraint_context *context,
    const char *constraint_name,
    struct alter_table_drop_constraint_resolution *resolution
) {
    for (size_t index = 0U; index < context->check_constraint_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                context->check_constraints[index].check_constraint.name,
                constraint_name
            )) {
            resolution->kind = PLANNED_ALTER_TABLE_DROP_CONSTRAINT_CHECK;
            ++resolution->match_count;
        }
    }
}

static int populate_alter_table_drop_constraint_plan(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct alter_table_drop_constraint_context *context,
    const struct alter_table_drop_constraint_resolution *resolution,
    struct planned_alter_table_drop_constraint *out_plan
) {
    int rc = MYLITE_OK;

    switch (resolution->kind) {
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_PRIMARY_KEY:
        return plan_alter_table_drop_primary_key(database, statement, &out_plan->primary_key);
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_UNIQUE_INDEX:
        rc = validate_drop_index_auto_increment(
            database,
            &context->indexes[resolution->unique_index],
            context->indexes,
            context->index_count
        );
        if (rc == MYLITE_OK) {
            rc = reject_index_used_by_foreign_key(
                database,
                &context->indexes[resolution->unique_index].index
            );
        }
        if (rc == MYLITE_OK) {
            out_plan->unique_index.target = context->target;
            out_plan->unique_index.table = context->table;
            out_plan->unique_index.index = context->indexes[resolution->unique_index];
            context->indexes[resolution->unique_index] = (struct loaded_index_info){0};
        }
        return rc;
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_FOREIGN_KEY:
        return plan_alter_table_drop_foreign_key(database, statement, &out_plan->foreign_key);
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_CHECK:
        return plan_alter_table_drop_check(database, statement, &out_plan->check_constraint);
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE:
        set_runtime_error(database, "invalid DROP CONSTRAINT resolution");
        return MYLITE_ERROR;
    }

    set_runtime_error(database, "invalid DROP CONSTRAINT resolution");
    return MYLITE_ERROR;
}

static int validate_alter_table_drop_constraint_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum planned_alter_table_drop_constraint_kind kind
) {
    enum mylite_sql_ast_alter_algorithm algorithm = mylite_sql_ast_node_alter_algorithm(statement);
    enum mylite_sql_ast_alter_lock lock = mylite_sql_ast_node_alter_lock(statement);
    int rc = validate_alter_table_algorithm_lock_option_values(database, algorithm, lock);

    if (rc != MYLITE_OK) {
        return rc;
    }

    switch (kind) {
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_PRIMARY_KEY:
        return validate_alter_table_rebuild_algorithm_options(database, algorithm);
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_UNIQUE_INDEX:
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_FOREIGN_KEY:
        return validate_alter_table_online_metadata_algorithm_options(database, algorithm);
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_CHECK:
    case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static void alter_table_drop_constraint_context_deinit(
    struct alter_table_drop_constraint_context *context
) {
    if (context == NULL) {
        return;
    }
    loaded_check_constraint_infos_deinit(
        &context->check_constraints,
        &context->check_constraint_count
    );
    loaded_foreign_key_infos_deinit(&context->foreign_keys, &context->foreign_key_count);
    loaded_index_infos_deinit(&context->indexes, &context->index_count);
    free(context->columns);
    *context = (struct alter_table_drop_constraint_context){0};
}

static void planned_alter_table_drop_constraint_deinit(
    struct planned_alter_table_drop_constraint *plan
) {
    if (plan == NULL) {
        return;
    }
    planned_alter_table_drop_primary_key_deinit(&plan->primary_key);
    planned_drop_index_deinit(&plan->unique_index);
    planned_alter_table_drop_foreign_key_deinit(&plan->foreign_key);
    planned_alter_table_check_constraint_deinit(&plan->check_constraint);
    *plan = (struct planned_alter_table_drop_constraint){0};
}

static int find_loaded_foreign_key_by_name(
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count,
    const char *foreign_key_name,
    size_t *out_foreign_key
) {
    if (out_foreign_key == NULL) {
        return MYLITE_MISUSE;
    }
    *out_foreign_key = 0U;

    for (size_t index = 0U; index < foreign_key_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                foreign_keys[index].foreign_key.name,
                foreign_key_name
            )) {
            *out_foreign_key = index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int reject_table_referenced_by_foreign_key(struct mylite_db *database, int64_t table_id) {
    struct foreign_key_exists_context context = {.found = false};
    int rc = mylite_catalog_for_each_foreign_key_for_parent_table(
        database,
        table_id,
        collect_foreign_key_exists,
        &context
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (context.found) {
        set_row_is_referenced_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_table_with_foreign_keys(struct mylite_db *database, int64_t table_id) {
    struct foreign_key_exists_context context = {.found = false};
    int rc = mylite_catalog_for_each_foreign_key_in_child_table(
        database,
        table_id,
        collect_foreign_key_exists,
        &context
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (context.found) {
        set_unsupported_error(database, "TRUNCATE TABLE does not yet support foreign keys");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_foreign_key_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
) {
    struct foreign_key_exists_context context = {.found = false};
    int rc = mylite_catalog_for_each_foreign_key_in_child_table(
        database,
        table_id,
        collect_foreign_key_exists,
        &context
    );

    if (rc == MYLITE_OK && !context.found) {
        rc = mylite_catalog_for_each_foreign_key_for_parent_table(
            database,
            table_id,
            collect_foreign_key_exists,
            &context
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (context.found) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_index_used_by_foreign_key(
    struct mylite_db *database,
    const struct mylite_catalog_index_descriptor *index
) {
    struct foreign_key_index_exists_context context = {
        .index_id = 0,
        .found = false,
    };
    int rc = MYLITE_OK;

    if (index == NULL) {
        set_runtime_error(database, "invalid foreign-key index lookup");
        return MYLITE_ERROR;
    }

    context.index_id = index->index_id;
    rc = mylite_catalog_for_each_foreign_key_for_parent_table(
        database,
        index->table_id,
        collect_foreign_key_parent_index_exists,
        &context
    );

    if (rc == MYLITE_OK && !context.found) {
        rc = mylite_catalog_for_each_foreign_key_in_child_table(
            database,
            index->table_id,
            collect_foreign_key_child_index_exists,
            &context
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (context.found) {
        set_cannot_drop_index_needed_foreign_key_error(database, index->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int collect_foreign_key_exists(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
) {
    struct foreign_key_exists_context *context = user_data;

    (void)foreign_key;
    if (context == NULL) {
        return MYLITE_MISUSE;
    }
    context->found = true;
    return MYLITE_OK;
}

static int collect_foreign_key_parent_index_exists(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
) {
    struct foreign_key_index_exists_context *context = user_data;

    if (foreign_key == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }
    if (foreign_key->parent_index_id == context->index_id) {
        context->found = true;
    }
    return MYLITE_OK;
}

static int collect_foreign_key_child_index_exists(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
) {
    struct foreign_key_index_exists_context *context = user_data;

    if (foreign_key == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }
    if (foreign_key->child_index_id == context->index_id) {
        context->found = true;
    }
    return MYLITE_OK;
}

static int plan_alter_table_add_index_name(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    const struct mylite_sql_ast_node *index_name_node,
    const char *column_name,
    char *destination,
    size_t destination_size
) {
    int rc = MYLITE_OK;

    destination[0] = '\0';
    if (index_name_node == NULL) {
        return generate_alter_table_add_index_name(
            database,
            indexes,
            index_count,
            column_name,
            destination,
            destination_size
        );
    }

    rc = copy_identifier_text(index_name_node, destination, destination_size, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(destination, "PRIMARY")) {
        set_incorrect_index_name_error(database, destination);
        return MYLITE_ERROR;
    }
    if (loaded_index_name_is_used(indexes, index_count, destination)) {
        set_duplicate_key_name_error(database, destination);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int generate_alter_table_add_index_name(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    const char *base_name,
    char *destination,
    size_t destination_size
) {
    char candidate[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    unsigned int suffix = 1U;

    while (suffix < UINT_MAX) {
        int written = 0;

        if (suffix == 1U) {
            written = snprintf(candidate, sizeof(candidate), "%s", base_name);
        } else {
            written = snprintf(candidate, sizeof(candidate), "%s_%u", base_name, suffix);
        }
        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            set_identifier_too_long_error(database, "index");
            return MYLITE_ERROR;
        }
        if (!text_equals_ascii_case_insensitive(candidate, "PRIMARY") &&
            !loaded_index_name_is_used(indexes, index_count, candidate)) {
            int copied = snprintf(destination, destination_size, "%s", candidate);

            if (copied < 0 || (size_t)copied >= destination_size) {
                set_identifier_too_long_error(database, "index");
                return MYLITE_ERROR;
            }
            return MYLITE_OK;
        }
        ++suffix;
    }

    set_identifier_too_long_error(database, "index");
    return MYLITE_ERROR;
}

static bool loaded_index_name_is_used(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const char *index_name
) {
    for (size_t index = 0U; index < index_count; ++index) {
        if (text_equals_ascii_case_insensitive(indexes[index].index.name, index_name)) {
            return true;
        }
    }

    return false;
}

static int validate_alter_table_add_index_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (column == NULL) {
        set_runtime_error(database, "invalid secondary-index column");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_text_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_binary_blob_family(column)) {
        set_blob_key_without_length_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return MYLITE_OK;
    }
    if (column_descriptor_is_json(column)) {
        set_json_key_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_bit(column) || column_descriptor_is_enum(column) ||
        column_descriptor_is_set(column)) {
        set_unsupported_error(database, "Secondary indexes do not yet support this column type");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char_or_varchar(column)) {
        return validate_descriptor_string_key_column(database, column);
    }
    if (column_descriptor_is_decimal(column) || column_descriptor_is_year(column) ||
        column_descriptor_is_date(column) || column_descriptor_is_time(column) ||
        column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
        return MYLITE_OK;
    }

    return integer_range_for_column(
        database,
        column,
        "Secondary indexes do not yet support this column type",
        &range
    );
}

static int validate_alter_table_add_fulltext_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct planned_column planned = {0};

    if (column == NULL) {
        set_runtime_error(database, "invalid FULLTEXT index column");
        return MYLITE_ERROR;
    }

    planned_column_from_catalog_descriptor(column, NULL, &planned);
    return validate_fulltext_index_column(database, &planned);
}

static int validate_alter_table_add_spatial_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        set_runtime_error(database, "invalid SPATIAL index column");
        return MYLITE_ERROR;
    }
    if (!column_descriptor_is_spatial(column)) {
        set_spatial_index_non_geometric_error(database);
        return MYLITE_ERROR;
    }
    if (column->is_nullable) {
        set_spatial_index_must_be_not_null_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_unique_index_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    int rc = validate_create_unique_index_string_values(database, plan);

    if (rc == MYLITE_OK) {
        rc = validate_create_unique_index_duplicates(database, plan);
    }
    return rc;
}

static int validate_create_unique_index_string_values(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    size_t string_part_count = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    if (plan == NULL || plan->part_count == 0U) {
        set_runtime_error(database, "invalid unique-index plan");
        return MYLITE_ERROR;
    }
    string_part_count = count_create_unique_index_string_validation_parts(plan);
    if (string_part_count == 0U) {
        return MYLITE_OK;
    }

    rc = build_create_unique_index_string_validation_sql(plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        rc = validate_create_unique_index_string_row(database, statement, string_part_count);
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static size_t count_create_unique_index_string_validation_parts(
    const struct planned_alter_table_add_index *plan
) {
    size_t string_part_count = 0U;

    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (loaded_index_part_requires_string_key_validation(&plan->parts[part_index])) {
            ++string_part_count;
        }
    }

    return string_part_count;
}

static int validate_create_unique_index_string_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t string_part_count
) {
    for (size_t column_index = 0U; column_index < string_part_count; ++column_index) {
        const unsigned char *text = NULL;
        int text_length = 0;

        if (column_index >= (size_t)INT_MAX) {
            set_runtime_error(database, "invalid string key validation column");
            return MYLITE_ERROR;
        }
        if (sqlite3_column_type(statement, (int)column_index) == SQLITE_NULL) {
            continue;
        }

        text = sqlite3_column_text(statement, (int)column_index);
        text_length = sqlite3_column_bytes(statement, (int)column_index);
        if (text_length < 0 ||
            !text_value_is_supported_string_key((const char *)text, (size_t)text_length)) {
            set_non_ascii_string_key_error(database);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_create_unique_index_duplicates(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    sqlite3_stmt *statement = NULL;
    char duplicate_value[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_create_unique_index_duplicate_validation_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = format_sqlite_key_tuple(
                statement,
                plan->parts,
                plan->part_count,
                duplicate_value,
                sizeof(duplicate_value)
            );
            if (rc == MYLITE_OK) {
                set_duplicate_key_error(
                    database,
                    plan->target.table_name,
                    plan->index_name,
                    duplicate_value
                );
                rc = MYLITE_ERROR;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int execute_physical_add_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan,
    const char *index_physical_name
) {
    char *sql = NULL;
    int rc = build_add_index_sql(plan, index_physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_drop_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_drop_index *out_plan
) {
    return plan_drop_index(
        database,
        (struct drop_index_nodes){
            .table_name_node = child_at(statement, 0U),
            .index_name_node = child_at(statement, 1U),
        },
        "ALTER TABLE DROP INDEX",
        out_plan
    );
}

static int plan_drop_index(
    struct mylite_db *database,
    struct drop_index_nodes nodes,
    const char *statement_name,
    struct planned_drop_index *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t resolved_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_drop_index){0};
    rc = resolve_visible_writable_table_reference(
        database,
        nodes.table_name_node,
        &out_plan->target,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(nodes.index_name_node, index_name, sizeof(index_name), database);
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
        int written = snprintf(
            message,
            sizeof(message),
            "%s supports only persistent base tables",
            statement_name
        );

        set_unsupported_error(
            database,
            written < 0 || (size_t)written >= sizeof(message)
                ? "DROP INDEX supports only persistent base tables"
                : message
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = find_loaded_index_by_name(indexes, index_count, index_name, &resolved_index);
        if (rc != MYLITE_OK) {
            set_cant_drop_field_or_key_error(database, index_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK &&
        indexes[resolved_index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        set_unsupported_error(
            database,
            "DROP INDEX does not drop primary keys; use DROP PRIMARY KEY"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        !loaded_index_kind_is_secondary_or_fulltext(indexes[resolved_index].index.kind)) {
        set_unsupported_error(database, "DROP INDEX supports only secondary indexes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_drop_index_auto_increment(
            database,
            &indexes[resolved_index],
            indexes,
            index_count
        );
    }
    if (rc == MYLITE_OK && out_plan->table.table_id >= 0) {
        rc = reject_index_used_by_foreign_key(database, &indexes[resolved_index].index);
    }
    if (rc == MYLITE_OK) {
        out_plan->index = indexes[resolved_index];
        indexes[resolved_index] = (struct loaded_index_info){0};
    }
    if (rc == MYLITE_OK && out_plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        rc = read_show_table_status_row_count(database, &out_plan->table, &out_plan->affected_rows);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static void planned_drop_index_deinit(struct planned_drop_index *plan) {
    if (plan == NULL) {
        return;
    }
    loaded_index_info_deinit(&plan->index);
    *plan = (struct planned_drop_index){0};
}

static int drop_index_from_plan(struct mylite_db *database, const struct planned_drop_index *plan) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    bool physical_schema_changed = false;
    int rc = MYLITE_OK;

    if (plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        if (plan->index.index.kind != MYLITE_CATALOG_INDEX_KIND_FULLTEXT &&
            plan->index.index.kind != MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            rc = execute_physical_drop_index(database, plan);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_temporary_catalog_remove_index_by_id(
                &database->session.temporary_catalog,
                plan->table.table_id,
                plan->index.index.index_id
            );
        }
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to drop temporary index");
            return rc;
        }
        if (plan->index.index.kind != MYLITE_CATALOG_INDEX_KIND_FULLTEXT &&
            plan->index.index.kind != MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            ++database->session.sqlite_schema_generation;
        }
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = drop_persistent_index_in_mutation(database, &mutation, plan, &physical_schema_changed);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop index");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (physical_schema_changed) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int drop_persistent_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_drop_index *plan,
    bool *out_physical_schema_changed
) {
    int rc = MYLITE_OK;

    if (mutation == NULL || plan == NULL || out_physical_schema_changed == NULL) {
        set_runtime_error(database, "invalid persistent drop-index mutation plan");
        return MYLITE_ERROR;
    }

    *out_physical_schema_changed = false;
    rc = mylite_catalog_delete_index_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        plan->index.index.index_id
    );
    if (rc == MYLITE_OK && plan->index.index.kind != MYLITE_CATALOG_INDEX_KIND_FULLTEXT &&
        plan->index.index.kind != MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        rc = execute_physical_drop_index(database, plan);
        *out_physical_schema_changed = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }

    return rc;
}

static int plan_alter_table_rename_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_index *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    char old_index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t resolved_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_rename_index){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            old_index_name,
            sizeof(old_index_name),
            database
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 2U),
            out_plan->new_index_name,
            sizeof(out_plan->new_index_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->new_index_name)) {
        set_reserved_name_error(database, "index", out_plan->new_index_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE RENAME INDEX supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = find_loaded_index_by_name(indexes, index_count, old_index_name, &resolved_index);
        if (rc != MYLITE_OK) {
            set_key_does_not_exist_in_table_error(
                database,
                old_index_name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK &&
        indexes[resolved_index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        set_incorrect_index_name_error(database, old_index_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        !loaded_index_kind_is_secondary_or_fulltext(indexes[resolved_index].index.kind)) {
        set_unsupported_error(database, "ALTER TABLE RENAME INDEX supports only secondary indexes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        text_equals_ascii_case_insensitive(out_plan->new_index_name, "PRIMARY")) {
        set_incorrect_index_name_error(database, out_plan->new_index_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && loaded_index_name_is_used_by_other(
                               indexes,
                               index_count,
                               &indexes[resolved_index],
                               out_plan->new_index_name
                           )) {
        set_duplicate_key_name_error(database, out_plan->new_index_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->index = indexes[resolved_index];
        indexes[resolved_index] = (struct loaded_index_info){0};
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static bool loaded_index_kind_is_secondary_or_fulltext(enum mylite_catalog_index_kind kind) {
    if (kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        return true;
    }
    if (kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return true;
    }
    if (kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return true;
    }

    return false;
}

static void planned_rename_index_deinit(struct planned_rename_index *plan) {
    if (plan == NULL) {
        return;
    }
    loaded_index_info_deinit(&plan->index);
    *plan = (struct planned_rename_index){0};
}

static int rename_index_from_plan(
    struct mylite_db *database,
    const struct planned_rename_index *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_rename_index_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->index.index.index_id,
            plan->new_index_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename index");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_add_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_check_constraint){
        .action = PLANNED_ALTER_TABLE_CHECK_ADD,
    };
    rc = plan_alter_table_check_target(database, child_at(statement, 0U), out_plan);
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_add_check_definition(database, child_at(statement, 1U), out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_drop_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
) {
    char check_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t check_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_check_constraint){
        .action = PLANNED_ALTER_TABLE_CHECK_DROP,
    };
    rc = plan_alter_table_check_target(database, child_at(statement, 0U), out_plan);
    if (rc == MYLITE_OK) {
        rc =
            copy_identifier_text(child_at(statement, 1U), check_name, sizeof(check_name), database);
    }
    if (rc == MYLITE_OK &&
        find_planned_check_constraint_by_name(&out_plan->rebuild, check_name, &check_index) !=
            MYLITE_OK) {
        set_check_constraint_not_found_error(database, check_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->target_check_constraint_id =
            out_plan->rebuild.check_constraints[check_index].check_constraint_id;
        out_plan->check_constraint_index = check_index;
        out_plan->has_check_constraint_index = true;
        out_plan->catalog_change_required = true;
        out_plan->physical_rebuild_required =
            out_plan->rebuild.check_constraints[check_index].is_enforced;
        remove_planned_check_constraint_at(&out_plan->rebuild, check_index);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_alter_check(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_check_constraint *out_plan
) {
    const struct mylite_sql_ast_node *enforcement = child_at(statement, 2U);
    char check_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t check_index = 0U;
    bool target_enforced = true;
    bool source_enforced = false;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_check_constraint){
        .action = PLANNED_ALTER_TABLE_CHECK_ALTER,
    };
    rc = plan_alter_table_check_target(database, child_at(statement, 0U), out_plan);
    if (rc == MYLITE_OK) {
        rc =
            copy_identifier_text(child_at(statement, 1U), check_name, sizeof(check_name), database);
    }
    if (rc == MYLITE_OK &&
        find_planned_check_constraint_by_name(&out_plan->rebuild, check_name, &check_index) !=
            MYLITE_OK) {
        set_check_constraint_not_found_error(database, check_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        if (enforcement == NULL || !check_constraint_node_is_enforcement(enforcement)) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
        } else {
            target_enforced = enforcement->kind == MYLITE_SQL_AST_CHECK_ENFORCEMENT_ENFORCED;
        }
    }
    if (rc == MYLITE_OK) {
        source_enforced = out_plan->rebuild.check_constraints[check_index].is_enforced;
        out_plan->target_check_constraint_id =
            out_plan->rebuild.check_constraints[check_index].check_constraint_id;
        out_plan->check_constraint_index = check_index;
        out_plan->has_check_constraint_index = true;
        out_plan->target_enforced = target_enforced;
        out_plan->catalog_change_required = source_enforced != target_enforced;
        out_plan->physical_rebuild_required = source_enforced != target_enforced;
        out_plan->validating_rebuild = false;
        if (!source_enforced) {
            if (target_enforced) {
                out_plan->validating_rebuild = true;
            }
        }
        out_plan->rebuild.check_constraints[check_index].is_enforced = target_enforced;
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_check_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = resolve_writable_table_name(database, table_node, &out_plan->target);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "ALTER TABLE CHECK supports only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = populate_alter_table_check_rebuild_plan(database, out_plan);
    }

    return rc;
}

static int populate_alter_table_check_rebuild_plan(
    struct mylite_db *database,
    struct planned_alter_table_check_constraint *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t check_constraint_count = 0U;
    int rc = MYLITE_OK;

    out_plan->rebuild.target = out_plan->target;
    out_plan->rebuild.auto_increment_next = out_plan->table.auto_increment_next;
    snprintf(
        out_plan->rebuild.default_charset,
        sizeof(out_plan->rebuild.default_charset),
        "%s",
        out_plan->table.default_charset
    );
    snprintf(
        out_plan->rebuild.default_collation,
        sizeof(out_plan->rebuild.default_collation),
        "%s",
        out_plan->table.default_collation
    );

    rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = copy_alter_table_check_columns(database, columns, column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_alter_table_check_indexes(database, indexes, index_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_check_constraint_infos(
            database,
            out_plan->table.table_id,
            &check_constraints,
            &check_constraint_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_alter_table_check_constraints(
            database,
            check_constraints,
            check_constraint_count,
            out_plan
        );
    }

    loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int copy_alter_table_check_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_check_constraint *out_plan
) {
    if (column_count > SIZE_MAX / sizeof(*out_plan->rebuild.columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->rebuild.columns = calloc(column_count, sizeof(*out_plan->rebuild.columns));
    if (out_plan->rebuild.columns == NULL && column_count != 0U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->rebuild.column_count = column_count;
    for (size_t index = 0U; index < column_count; ++index) {
        planned_column_from_catalog_descriptor(
            &columns[index],
            NULL,
            &out_plan->rebuild.columns[index]
        );
    }

    return MYLITE_OK;
}

static int copy_alter_table_check_indexes(
    struct mylite_db *database,
    const struct loaded_index_info *indexes,
    size_t index_count,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            rc = copy_alter_table_check_primary_index(database, &indexes[index], out_plan);
        } else if (
            indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY ||
            indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
            indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL
        ) {
            rc = copy_alter_table_check_secondary_index(database, &indexes[index], out_plan);
        }
    }

    return rc;
}

static int copy_alter_table_check_primary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = reserve_planned_primary_key_parts(database, &out_plan->rebuild, index->part_count);

    if (rc == MYLITE_OK) {
        out_plan->rebuild.has_primary_key = true;
        out_plan->rebuild.primary_key_index_id = index->index.index_id;
        snprintf(
            out_plan->rebuild.primary_key_physical_name,
            sizeof(out_plan->rebuild.primary_key_physical_name),
            "%s",
            index->index.physical_name
        );
        out_plan->rebuild.primary_key_show_create_explicit_btree =
            index->index.show_create_explicit_btree;
        snprintf(
            out_plan->rebuild.primary_key_comment,
            sizeof(out_plan->rebuild.primary_key_comment),
            "%s",
            index->index.comment
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        out_plan->rebuild.primary_key_parts[out_plan->rebuild.primary_key_part_count] =
            (struct planned_primary_key_part){
                .column_index = column_index,
                .sort_direction = index->parts[part_index].index_column.sort_direction,
            };
        ++out_plan->rebuild.primary_key_part_count;
        out_plan->rebuild.columns[column_index].is_primary_key = true;
        out_plan->rebuild.columns[column_index].is_nullable = false;
    }

    return rc;
}

static int copy_alter_table_check_secondary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    struct planned_alter_table_check_constraint *out_plan
) {
    struct planned_secondary_index *target = NULL;
    int rc = reserve_planned_secondary_indexes(
        database,
        &out_plan->rebuild,
        out_plan->rebuild.secondary_index_count + 1U
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    target = &out_plan->rebuild.secondary_indexes[out_plan->rebuild.secondary_index_count];
    *target = (struct planned_secondary_index){0};
    target->index_id = index->index.index_id;
    target->kind = index->index.kind;
    target->is_unique = index->index.is_unique;
    target->is_visible = index->index.is_visible;
    target->show_create_explicit_btree = index->index.show_create_explicit_btree;
    snprintf(target->name, sizeof(target->name), "%s", index->index.name);
    snprintf(target->comment, sizeof(target->comment), "%s", index->index.comment);
    snprintf(
        target->physical_name,
        sizeof(target->physical_name),
        "%s",
        index->index.physical_name
    );
    rc = reserve_planned_secondary_index_parts(database, target, index->part_count);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        target->parts[target->part_count] = (struct planned_secondary_index_part){
            .column_index = index->parts[part_index].column_index,
            .has_prefix_length = index->parts[part_index].index_column.has_prefix_length,
            .prefix_length = index->parts[part_index].index_column.prefix_length,
            .sort_direction = index->parts[part_index].index_column.sort_direction,
        };
        ++target->part_count;
    }
    if (rc == MYLITE_OK) {
        ++out_plan->rebuild.secondary_index_count;
    } else {
        free(target->parts);
        *target = (struct planned_secondary_index){0};
    }

    return rc;
}

static int copy_alter_table_check_constraints(
    struct mylite_db *database,
    const struct loaded_check_constraint_info *check_constraints,
    size_t check_constraint_count,
    struct planned_alter_table_check_constraint *out_plan
) {
    int rc = MYLITE_OK;

    if (check_constraint_count == 0U) {
        return MYLITE_OK;
    }
    rc = reserve_planned_check_constraints(database, &out_plan->rebuild, check_constraint_count);
    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        const struct mylite_catalog_check_constraint_descriptor *source =
            &check_constraints[index].check_constraint;
        struct planned_check_constraint *target =
            &out_plan->rebuild.check_constraints[out_plan->rebuild.check_constraint_count];

        *target = (struct planned_check_constraint){
            .check_constraint_id = source->check_constraint_id,
            .generated_ordinal = source->generated_ordinal,
            .ordinal_position = source->ordinal_position,
            .is_enforced = source->is_enforced,
            .name_is_generated = source->name_is_generated,
        };
        snprintf(target->name, sizeof(target->name), "%s", source->name);
        snprintf(target->physical_name, sizeof(target->physical_name), "%s", source->physical_name);
        snprintf(target->check_clause, sizeof(target->check_clause), "%s", source->check_clause);
        snprintf(
            target->sqlite_expression,
            sizeof(target->sqlite_expression),
            "%s",
            source->sqlite_expression
        );
        ++out_plan->rebuild.check_constraint_count;
    }

    return rc;
}

static int plan_alter_table_add_check_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *check_constraint,
    struct planned_alter_table_check_constraint *out_plan
) {
    const struct mylite_sql_ast_node *expression = child_at(check_constraint, 0U);
    const struct mylite_sql_ast_node *child = child_at(check_constraint, 1U);
    const struct mylite_sql_ast_node *name_node = NULL;
    struct planned_check_constraint planned = {
        .generated_ordinal = next_planned_check_constraint_generated_ordinal(&out_plan->rebuild),
        .ordinal_position = (int64_t)out_plan->rebuild.check_constraint_count + 1,
        .is_enforced = true,
    };
    int rc = MYLITE_OK;

    if (check_constraint == NULL ||
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

    if (name_node == NULL) {
        rc = next_available_planned_check_constraint_generated_ordinal(
            database,
            &out_plan->rebuild,
            &planned.generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_create_table_check_constraint_name(
            database,
            &out_plan->rebuild,
            name_node,
            &planned
        );
    }
    if (rc == MYLITE_OK) {
        struct mylite_dynamic_string check_string;
        struct mylite_dynamic_string sqlite_string;
        struct check_expression_render_context context = {
            .database = database,
            .plan = &out_plan->rebuild,
            .inline_column = NULL,
            .inline_column_index = 0U,
            .alter_check_constraint_name = planned.name,
            .check_clause = &check_string,
            .sqlite_expression = &sqlite_string,
        };
        bool is_boolean = false;

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
        if (rc == MYLITE_OK && (check_string.length >= sizeof(planned.check_clause) ||
                                sqlite_string.length >= sizeof(planned.sqlite_expression))) {
            set_unsupported_error(database, "CHECK constraint expression is too long");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            memcpy(planned.check_clause, check_string.text, check_string.length + 1U);
            memcpy(planned.sqlite_expression, sqlite_string.text, sqlite_string.length + 1U);
        }
        mylite_dynamic_string_deinit(&check_string);
        mylite_dynamic_string_deinit(&sqlite_string);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_planned_check_constraints(
            database,
            &out_plan->rebuild,
            out_plan->rebuild.check_constraint_count + 1U
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->check_constraint_index = out_plan->rebuild.check_constraint_count;
        out_plan->has_check_constraint_index = true;
        out_plan->catalog_change_required = true;
        out_plan->physical_rebuild_required = planned.is_enforced;
        out_plan->validating_rebuild = planned.is_enforced;
        out_plan->rebuild.check_constraints[out_plan->rebuild.check_constraint_count] = planned;
        ++out_plan->rebuild.check_constraint_count;
    }

    return rc;
}

static int next_available_planned_check_constraint_generated_ordinal(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t *out_ordinal
) {
    if (out_ordinal == NULL) {
        return MYLITE_MISUSE;
    }
    *out_ordinal = 0;
    if (plan == NULL) {
        *out_ordinal = 1;
        return MYLITE_OK;
    }

    for (int64_t ordinal = 1; ordinal < INT64_MAX; ++ordinal) {
        bool ordinal_is_used = false;

        for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
            const struct planned_check_constraint *check_constraint =
                &plan->check_constraints[index];

            if (check_constraint->name_is_generated &&
                check_constraint->generated_ordinal == ordinal) {
                ordinal_is_used = true;
                break;
            }
        }
        if (!ordinal_is_used) {
            *out_ordinal = ordinal;
            return MYLITE_OK;
        }
    }

    set_runtime_error(database, "generated CHECK constraint ordinal is too large");
    return MYLITE_ERROR;
}

static int find_planned_check_constraint_by_name(
    const struct planned_create_table *plan,
    const char *check_constraint_name,
    size_t *out_index
) {
    if (out_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index = 0U;
    if (plan == NULL || check_constraint_name == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < plan->check_constraint_count; ++index) {
        if (text_equals_ascii_case_insensitive(
                plan->check_constraints[index].name,
                check_constraint_name
            )) {
            *out_index = index;
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static void remove_planned_check_constraint_at(struct planned_create_table *plan, size_t index) {
    if (plan == NULL || index >= plan->check_constraint_count) {
        return;
    }
    if (index + 1U < plan->check_constraint_count) {
        memmove(
            &plan->check_constraints[index],
            &plan->check_constraints[index + 1U],
            (plan->check_constraint_count - index - 1U) * sizeof(plan->check_constraints[0])
        );
    }
    --plan->check_constraint_count;
}

static int alter_table_check_constraint_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_check_constraint *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (!plan->catalog_change_required) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK && plan->action == PLANNED_ALTER_TABLE_CHECK_ADD) {
        rc = insert_alter_table_check_constraint_catalog_row(database, &mutation, plan);
    } else if (rc == MYLITE_OK && plan->action == PLANNED_ALTER_TABLE_CHECK_DROP) {
        rc = mylite_catalog_delete_check_constraint_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->target_check_constraint_id
        );
    } else if (rc == MYLITE_OK && plan->action == PLANNED_ALTER_TABLE_CHECK_ALTER) {
        rc = mylite_catalog_update_check_constraint_enforcement_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->target_check_constraint_id,
            plan->target_enforced
        );
    }
    if (rc == MYLITE_OK && plan->physical_rebuild_required) {
        rc = execute_physical_alter_table_check_constraint(
            database,
            plan,
            mylite_catalog_mutation_generation(&mutation),
            &plan->affected_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to alter CHECK constraint");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (plan->physical_rebuild_required) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int insert_alter_table_check_constraint_catalog_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_alter_table_check_constraint *plan
) {
    struct planned_check_constraint *check_constraint =
        &plan->rebuild.check_constraints[plan->check_constraint_index];
    int64_t check_constraint_id = 0;
    int rc = mylite_catalog_allocate_check_constraint_id_in_mutation(
        database,
        mutation,
        &check_constraint_id
    );

    if (rc == MYLITE_OK) {
        check_constraint->check_constraint_id = check_constraint_id;
        rc = build_physical_check_constraint_name(
            check_constraint_id,
            check_constraint->physical_name,
            sizeof(check_constraint->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_check_constraint_in_mutation(
            database,
            mutation,
            check_constraint->check_constraint_id,
            plan->table.table_id,
            check_constraint->name,
            check_constraint->physical_name,
            check_constraint->check_clause,
            check_constraint->sqlite_expression,
            check_constraint->is_enforced,
            check_constraint->name_is_generated,
            check_constraint->generated_ordinal,
            check_constraint->ordinal_position,
            NULL
        );
    }

    return rc;
}

static int execute_physical_alter_table_check_constraint(
    struct mylite_db *database,
    const struct planned_alter_table_check_constraint *plan,
    uint64_t sqlite_schema_generation,
    int64_t *out_affected_rows
) {
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool copy_failed = false;
    int64_t affected_rows = 0;
    int rc = build_alter_table_check_temporary_physical_name(
        plan,
        sqlite_schema_generation,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    *out_affected_rows = 0;
    if (rc == MYLITE_OK) {
        rc = build_alter_table_check_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_check_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_alter_table_check_copy_rows(database, plan, sql, &affected_rows, &copy_failed);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_create_table_indexes_sql(&plan->rebuild, plan->table.physical_name, &sql);
    }
    if (rc == MYLITE_OK && sql != NULL) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK && plan->validating_rebuild) {
        *out_affected_rows = affected_rows;
    }

    return rc;
}

static int execute_alter_table_check_copy_rows(
    struct mylite_db *database,
    const struct planned_alter_table_check_constraint *plan,
    const char *sql,
    int64_t *out_affected_rows,
    bool *out_copy_failed
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_sqlite_statement(database, sql, &statement);

    *out_affected_rows = 0;
    *out_copy_failed = false;
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_DONE) {
            *out_affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            bool was_check_violation = false;

            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_status_is_constraint(sqlite_rc)) {
                int check_rc = handle_check_constraint_violation(
                    database,
                    plan->table.table_id,
                    false,
                    &was_check_violation
                );

                if (check_rc != MYLITE_OK || was_check_violation) {
                    rc = check_rc;
                }
            }
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    if (rc != MYLITE_OK) {
        *out_copy_failed = true;
    }

    return rc;
}

static void planned_alter_table_check_constraint_deinit(
    struct planned_alter_table_check_constraint *plan
) {
    if (plan == NULL) {
        return;
    }
    planned_create_table_deinit(&plan->rebuild);
    *plan = (struct planned_alter_table_check_constraint){0};
}

static bool loaded_index_name_is_used_by_other(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const struct loaded_index_info *skipped_index,
    const char *index_name
) {
    for (size_t index = 0U; index < index_count; ++index) {
        if (&indexes[index] == skipped_index) {
            continue;
        }
        if (text_equals_ascii_case_insensitive(indexes[index].index.name, index_name)) {
            return true;
        }
    }

    return false;
}

static int find_loaded_index_by_name(
    const struct loaded_index_info *indexes,
    size_t index_count,
    const char *index_name,
    size_t *out_index
) {
    if (out_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index = 0U;

    for (size_t index = 0U; index < index_count; ++index) {
        if (text_equals_ascii_case_insensitive(indexes[index].index.name, index_name)) {
            *out_index = index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int validate_drop_index_auto_increment(
    struct mylite_db *database,
    const struct loaded_index_info *target,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    for (size_t part_index = 0U; part_index < target->part_count; ++part_index) {
        const struct mylite_catalog_column_descriptor *column = &target->parts[part_index].column;
        const struct loaded_index_column_lookup lookup = {
            .indexes = indexes,
            .index_count = index_count,
            .skipped_index_id = target->index.index_id,
            .column_id = column->column_id,
        };

        if (!column->is_auto_increment) {
            continue;
        }
        if (!loaded_other_index_contains_column(&lookup)) {
            set_wrong_auto_key_error(database);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool loaded_other_index_contains_column(const struct loaded_index_column_lookup *lookup) {
    for (size_t index = 0U; index < lookup->index_count; ++index) {
        const struct loaded_index_info *candidate = &lookup->indexes[index];

        if (candidate->index.index_id == lookup->skipped_index_id) {
            continue;
        }
        for (size_t part_index = 0U; part_index < candidate->part_count; ++part_index) {
            if (candidate->parts[part_index].column.column_id == lookup->column_id) {
                return true;
            }
        }
    }

    return false;
}

static int execute_physical_drop_index(
    struct mylite_db *database,
    const struct planned_drop_index *plan
) {
    return execute_physical_drop_index_by_name(database, plan->index.index.physical_name);
}

static int execute_physical_drop_index_by_name(struct mylite_db *database, const char *name) {
    char *sql = NULL;
    int rc = build_drop_index_sql(name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_drop_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_primary_key *out_plan
) {
    return plan_alter_table_drop_primary_key_with_options(database, statement, true, out_plan);
}

static int plan_alter_table_drop_primary_key_for_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_primary_key *out_plan
) {
    return plan_alter_table_drop_primary_key_with_options(database, statement, false, out_plan);
}

static int plan_alter_table_drop_primary_key_with_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool validate_auto_increment,
    struct planned_alter_table_drop_primary_key *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct primary_key_info primary_key = primary_key_info_init();
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_primary_key){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE DROP PRIMARY KEY supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &primary_key
        );
    }
    if (rc == MYLITE_OK && !primary_key.has_primary_key) {
        set_cant_drop_field_or_key_error(database, "PRIMARY");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && validate_auto_increment &&
        sql_require_primary_key_session_enabled(database)) {
        set_sql_require_primary_key_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->primary_key = primary_key.index;
        out_plan->parts = primary_key.parts;
        out_plan->part_count = primary_key.part_count;
        primary_key.parts = NULL;
        primary_key.part_count = 0U;
    }
    if (rc == MYLITE_OK && validate_auto_increment) {
        rc = validate_alter_table_drop_primary_key_auto_increment(
            database,
            out_plan,
            columns,
            column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_index_used_by_foreign_key(database, &out_plan->primary_key);
    }
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(database, &out_plan->table, &out_plan->affected_rows);
    }

    primary_key_info_deinit(&primary_key);
    free(columns);
    return rc;
}

static void planned_alter_table_drop_primary_key_deinit(
    struct planned_alter_table_drop_primary_key *plan
) {
    if (plan == NULL) {
        return;
    }
    free(plan->parts);
    *plan = (struct planned_alter_table_drop_primary_key){0};
}

static int validate_alter_table_drop_primary_key_auto_increment(
    struct mylite_db *database,
    const struct planned_alter_table_drop_primary_key *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    struct loaded_index_info *indexes = NULL;
    size_t index_count = 0U;
    int64_t auto_increment_column_id = 0;
    int rc = MYLITE_OK;
    bool found_secondary_key = false;

    if (!primary_key_auto_increment_column_id(plan, &auto_increment_column_id)) {
        return MYLITE_OK;
    }

    rc = load_table_index_infos(
        database,
        plan->table.table_id,
        columns,
        column_count,
        &indexes,
        &index_count
    );
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
            indexes[index].part_count == 0U) {
            continue;
        }
        if (indexes[index].parts[0].column.column_id == auto_increment_column_id) {
            found_secondary_key = true;
            break;
        }
    }
    loaded_index_infos_deinit(&indexes, &index_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!found_secondary_key) {
        set_wrong_auto_key_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool primary_key_auto_increment_column_id(
    const struct planned_alter_table_drop_primary_key *plan,
    int64_t *out_column_id
) {
    if (out_column_id != NULL) {
        *out_column_id = 0;
    }
    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (plan->parts[part_index].column.is_auto_increment) {
            if (out_column_id != NULL) {
                *out_column_id = plan->parts[part_index].column.column_id;
            }
            return true;
        }
    }

    return false;
}

static int alter_table_drop_primary_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_primary_key *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = alter_table_drop_primary_key_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop primary key");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int alter_table_drop_primary_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_drop_primary_key *plan
) {
    int rc = MYLITE_OK;

    if (mutation == NULL || plan == NULL) {
        set_runtime_error(database, "invalid drop-primary-key mutation plan");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_delete_index_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        plan->primary_key.index_id
    );
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_drop_primary_key(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }

    return rc;
}

static int execute_physical_alter_table_drop_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_drop_primary_key *plan
) {
    char *sql = NULL;
    int rc = build_drop_index_sql(plan->primary_key.physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_auto_increment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_auto_increment *out_plan
) {
    const struct mylite_sql_ast_node *option = child_at(statement, 1U);
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_auto_increment){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = parse_alter_table_auto_increment_option(database, option, &out_plan->requested_next);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE AUTO_INCREMENT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = find_alter_table_auto_increment_column(
            database,
            out_plan,
            &out_plan->column,
            &out_plan->has_auto_increment_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = compute_alter_table_auto_increment_effective_next(
            database,
            out_plan,
            &out_plan->effective_next
        );
    }

    return rc;
}

static int parse_alter_table_auto_increment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    int64_t *out_next
) {
    const uint64_t sqlite_int64_positive_max = 9223372036854775807ULL;
    const struct mylite_sql_ast_node *literal = child_at(option, 0U);
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (option == NULL || option->kind != MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION ||
        literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
    if (rc != MYLITE_OK || magnitude > sqlite_int64_positive_max) {
        set_unsupported_error(
            database,
            "AUTO_INCREMENT table option supports only signed 64-bit nonnegative values"
        );
        return MYLITE_ERROR;
    }

    *out_next = magnitude == 0U ? 1 : (int64_t)magnitude;
    return MYLITE_OK;
}

static int find_alter_table_auto_increment_column(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_found
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t auto_increment_count = 0U;
    int rc = load_table_columns(database, plan->table.table_id, &columns, &column_count);

    *out_column = (struct mylite_catalog_column_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            if (!column_descriptor_is_auto_increment(&columns[column_index])) {
                continue;
            }
            if (auto_increment_count == 0U) {
                *out_column = columns[column_index];
                *out_found = true;
            }
            ++auto_increment_count;
        }
    }
    free(columns);

    if (rc == MYLITE_OK && auto_increment_count > 1U) {
        set_runtime_error(database, "invalid auto-increment table descriptor");
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int alter_table_auto_increment_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan
) {
    int rc = MYLITE_OK;

    if (!plan->has_auto_increment_column ||
        plan->effective_next == plan->table.auto_increment_next) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_update_table_auto_increment_next(
        database,
        plan->table.table_id,
        plan->effective_next
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update auto-increment counter");
        return rc;
    }

    return MYLITE_OK;
}

static int compute_alter_table_auto_increment_effective_next(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan,
    int64_t *out_effective_next
) {
    struct integer_column_range range = {0};
    int64_t row_next = 1;
    int rc = MYLITE_OK;

    *out_effective_next = plan->requested_next;
    if (!plan->has_auto_increment_column) {
        return MYLITE_OK;
    }

    rc = integer_range_for_column(
        database,
        &plan->column,
        "AUTO_INCREMENT supports only integer columns",
        &range
    );
    if (rc == MYLITE_OK) {
        rc = read_alter_table_auto_increment_row_next(database, plan, &range, &row_next);
    }
    if (rc == MYLITE_OK && row_next > *out_effective_next) {
        *out_effective_next = row_next;
    }

    return rc;
}

static int read_alter_table_auto_increment_row_next(
    struct mylite_db *database,
    const struct planned_alter_table_auto_increment *plan,
    const struct integer_column_range *range,
    int64_t *out_row_next
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_auto_increment_max_sql(plan, &sql);

    *out_row_next = 1;
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            const int column_type = sqlite3_column_type(statement, 0);
            if (column_type == SQLITE_NULL) {
                *out_row_next = 1;
            } else if (column_type == SQLITE_INTEGER) {
                const int64_t max_value = (int64_t)sqlite3_column_int64(statement, 0);
                if (max_value > 0) {
                    rc = next_auto_increment_after_explicit_insert_value(
                        1,
                        max_value,
                        range,
                        out_row_next
                    );
                }
            } else {
                set_physical_sqlite_row_error(database);
                rc = MYLITE_ERROR;
            }
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int plan_alter_table_drop_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_column){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE DROP COLUMN supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE DROP COLUMN does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, out_plan->column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_cant_drop_field_or_key_error(database, column_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->column_count <= 1U) {
        set_cant_remove_all_fields_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && columns[column_index].is_visible &&
        count_visible_columns(columns, out_plan->column_count) <= 1U) {
        set_must_have_visible_column_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_drop_column_dependencies(database, columns, out_plan);
    }

    free(columns);
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_column_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_drop_column_dependencies(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_drop_column *out_plan
) {
    int rc = reject_drop_column_foreign_key_dependencies(
        database,
        out_plan,
        columns,
        out_plan->column_count
    );

    if (rc == MYLITE_OK) {
        rc = plan_drop_column_key_dependencies(database, columns, out_plan->column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_drop_column_unique_key_updates(
            database,
            out_plan,
            columns,
            out_plan->column_count
        );
    }
    if (rc == MYLITE_OK && out_plan->removes_one_part_primary_key) {
        rc = read_show_table_status_row_count(database, &out_plan->table, &out_plan->affected_rows);
    }

    return rc;
}

static void planned_alter_table_drop_column_deinit(struct planned_alter_table_drop_column *plan) {
    if (plan == NULL) {
        return;
    }
    for (size_t index = 0U; index < plan->index_update_count; ++index) {
        free(plan->index_updates[index].remaining_parts);
    }
    free(plan->index_updates);
    *plan = (struct planned_alter_table_drop_column){0};
}

static int reject_drop_column_foreign_key_dependencies(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    struct loaded_foreign_key_info *child_foreign_keys = NULL;
    struct loaded_foreign_key_info *parent_foreign_keys = NULL;
    size_t child_foreign_key_count = 0U;
    size_t parent_foreign_key_count = 0U;
    int rc = load_table_foreign_key_infos(
        database,
        plan->table.table_id,
        columns,
        column_count,
        &child_foreign_keys,
        &child_foreign_key_count
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < child_foreign_key_count; ++index) {
        const struct loaded_foreign_key_info *foreign_key = &child_foreign_keys[index];

        for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
            if (foreign_key->parts[part_index].child_column.column_id == plan->column.column_id) {
                set_drop_column_foreign_key_child_error(
                    database,
                    plan->column.name,
                    foreign_key->foreign_key.name
                );
                rc = MYLITE_ERROR;
                break;
            }
        }
    }

    if (rc == MYLITE_OK) {
        rc = load_parent_foreign_key_infos(
            database,
            plan->table.table_id,
            &parent_foreign_keys,
            &parent_foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < parent_foreign_key_count; ++index) {
        const struct loaded_foreign_key_info *foreign_key = &parent_foreign_keys[index];

        for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
            if (foreign_key->parts[part_index].parent_column.column_id == plan->column.column_id) {
                set_drop_column_foreign_key_parent_error(
                    database,
                    plan->column.name,
                    foreign_key->foreign_key.name,
                    foreign_key->child_table.name
                );
                rc = MYLITE_ERROR;
                break;
            }
        }
    }

    loaded_foreign_key_infos_deinit(&child_foreign_keys, &child_foreign_key_count);
    loaded_foreign_key_infos_deinit(&parent_foreign_keys, &parent_foreign_key_count);
    return rc;
}

static int plan_drop_column_key_dependencies(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_drop_column *out_plan
) {
    struct loaded_index_info *indexes = NULL;
    size_t index_count = 0U;
    int rc = load_table_index_infos(
        database,
        out_plan->table.table_id,
        columns,
        column_count,
        &indexes,
        &index_count
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_drop_column_index_update(database, out_plan, &indexes[index]);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    return rc;
}

static int append_drop_column_index_update(
    struct mylite_db *database,
    struct planned_alter_table_drop_column *plan,
    const struct loaded_index_info *index
) {
    struct planned_drop_column_index_update *update = NULL;
    int64_t removed_ordinal_position = 0;
    int rc = MYLITE_OK;

    if (!loaded_index_find_column_ordinal(
            index,
            plan->column.column_id,
            &removed_ordinal_position
        )) {
        return MYLITE_OK;
    }

    rc = reserve_drop_column_index_updates(plan, plan->index_update_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    update = &plan->index_updates[plan->index_update_count];
    *update = (struct planned_drop_column_index_update){
        .index = index->index,
        .remaining_parts = NULL,
        .remaining_part_count = 0U,
        .removed_ordinal_position = removed_ordinal_position,
        .drops_index = index->part_count == 1U,
    };
    if (update->drops_index && index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        plan->removes_one_part_primary_key = true;
    }
    if (!update->drops_index) {
        if (index->part_count - 1U > SIZE_MAX / sizeof(*update->remaining_parts)) {
            return MYLITE_NOMEM;
        }
        update->remaining_parts = calloc(index->part_count - 1U, sizeof(*update->remaining_parts));
        if (update->remaining_parts == NULL) {
            return MYLITE_NOMEM;
        }
        for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
            if (index->parts[part_index].column.column_id == plan->column.column_id) {
                continue;
            }
            update->remaining_parts[update->remaining_part_count] = index->parts[part_index];
            ++update->remaining_part_count;
        }
        if (update->remaining_part_count == 0U) {
            set_runtime_error(database, "invalid drop-column key update");
            return MYLITE_ERROR;
        }
    }

    ++plan->index_update_count;
    return MYLITE_OK;
}

static int reserve_drop_column_index_updates(
    struct planned_alter_table_drop_column *plan,
    size_t required_capacity
) {
    enum { initial_drop_column_index_update_capacity = 4 };

    struct planned_drop_column_index_update *resized = NULL;
    size_t capacity = plan->index_update_capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_drop_column_index_update_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*plan->index_updates)) {
        return MYLITE_NOMEM;
    }

    resized = realloc(plan->index_updates, capacity * sizeof(*plan->index_updates));
    if (resized == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = plan->index_update_capacity; index < capacity; ++index) {
        resized[index] = (struct planned_drop_column_index_update){0};
    }
    plan->index_updates = resized;
    plan->index_update_capacity = capacity;
    return MYLITE_OK;
}

static bool loaded_index_find_column_ordinal(
    const struct loaded_index_info *index,
    int64_t column_id,
    int64_t *out_ordinal_position
) {
    if (out_ordinal_position != NULL) {
        *out_ordinal_position = 0;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column.column_id == column_id) {
            if (out_ordinal_position != NULL) {
                *out_ordinal_position = index->parts[part_index].index_column.ordinal_position;
            }
            return true;
        }
    }
    return false;
}

static int validate_drop_column_unique_key_updates(
    struct mylite_db *database,
    struct planned_alter_table_drop_column *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_update_count; ++index) {
        const struct planned_drop_column_index_update *update = &plan->index_updates[index];

        if (update->drops_index || !update->index.is_unique) {
            continue;
        }
        if (plan->rowid_alias == NULL) {
            rc = choose_sqlite_rowid_alias(
                database,
                columns,
                column_count,
                "ALTER TABLE DROP COLUMN requires an unshadowed SQLite rowid alias",
                &plan->rowid_alias
            );
        }
        if (rc == MYLITE_OK) {
            rc = validate_drop_column_unique_key_update(database, plan, update);
        }
    }

    return rc;
}

static int validate_drop_column_unique_key_update(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan,
    const struct planned_drop_column_index_update *update
) {
    struct planned_alter_table_add_index validation = {
        .target = plan->target,
        .table = plan->table,
        .parts = update->remaining_parts,
        .part_count = update->remaining_part_count,
        .part_capacity = update->remaining_part_count,
        .rowid_alias = plan->rowid_alias,
        .is_unique = true,
    };
    const char *index_name =
        update->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ? "PRIMARY" : update->index.name;
    int written = snprintf(validation.index_name, sizeof(validation.index_name), "%s", index_name);

    if (written < 0 || (size_t)written >= sizeof(validation.index_name)) {
        set_runtime_error(database, "invalid narrowed unique-key name");
        return MYLITE_ERROR;
    }
    return validate_create_unique_index_existing_rows(database, &validation);
}

static int alter_table_drop_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_update_count; ++index) {
        const struct planned_drop_column_index_update *update = &plan->index_updates[index];

        if (update->drops_index) {
            rc = mylite_catalog_delete_index_in_mutation(
                database,
                &mutation,
                plan->table.table_id,
                update->index.index_id
            );
        } else {
            rc = mylite_catalog_delete_index_column_in_mutation(
                database,
                &mutation,
                plan->table.table_id,
                update->index.index_id,
                plan->column.column_id,
                update->removed_ordinal_position
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_column_prepare_indexes(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->column.ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_drop_column(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_column_recreate_indexes(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop table column");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int execute_physical_drop_column_prepare_indexes(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_update_count; ++index) {
        const struct planned_drop_column_index_update *update = &plan->index_updates[index];
        char *sql = NULL;

        if (update->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
            update->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            continue;
        }
        rc = build_drop_index_sql(update->index.physical_name, &sql);
        if (rc == MYLITE_OK) {
            rc = execute_sqlite_schema_sql(database, sql);
        }
        free(sql);
    }

    return rc;
}

static int execute_physical_drop_column_recreate_indexes(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_update_count; ++index) {
        const struct planned_drop_column_index_update *update = &plan->index_updates[index];

        if (update->drops_index || update->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
            update->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            continue;
        }
        rc = execute_physical_drop_column_create_index(database, plan, update);
    }

    return rc;
}

static int execute_physical_drop_column_create_index(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan,
    const struct planned_drop_column_index_update *update
) {
    struct planned_alter_table_add_index index_plan = {
        .target = plan->target,
        .table = plan->table,
        .parts = update->remaining_parts,
        .part_count = update->remaining_part_count,
        .part_capacity = update->remaining_part_count,
        .rowid_alias = NULL,
        .is_unique = update->index.is_unique,
    };
    char *sql = NULL;
    int rc = build_add_index_sql(&index_plan, update->index.physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    return rc;
}

static int execute_physical_alter_table_drop_column(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
) {
    char *sql = NULL;
    int rc = build_alter_table_drop_column_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_rename_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_rename_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char old_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_rename_column){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            old_column_name,
            sizeof(old_column_name),
            database
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 2U),
            out_plan->new_column_name,
            sizeof(out_plan->new_column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(old_column_name)) {
        set_reserved_name_error(database, "column", old_column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->new_column_name)) {
        set_reserved_name_error(database, "column", out_plan->new_column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE RENAME COLUMN supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE RENAME COLUMN does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        size_t column_count = 0U;

        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
        if (rc == MYLITE_OK) {
            rc = plan_alter_table_rename_column_from_columns(
                database,
                out_plan,
                columns,
                column_count,
                old_column_name
            );
        }
    }

    free(columns);

    return rc;
}

static int plan_alter_table_rename_column_from_columns(
    struct mylite_db *database,
    struct planned_alter_table_rename_column *out_plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *old_column_name
) {
    size_t old_column_index = 0U;
    size_t new_column_index = 0U;
    int rc = find_column_index(columns, column_count, old_column_name, &old_column_index);

    if (rc != MYLITE_OK) {
        set_unknown_column_in_table_error(database, old_column_name, out_plan->table.name);
        return MYLITE_ERROR;
    }

    out_plan->column = columns[old_column_index];
    out_plan->is_noop = strcmp(out_plan->column.name, out_plan->new_column_name) == 0;

    if (out_plan->is_noop) {
        return MYLITE_OK;
    }
    if (find_column_index(columns, column_count, out_plan->new_column_name, &new_column_index) ==
            MYLITE_OK &&
        new_column_index != old_column_index) {
        set_duplicate_column_error(database, columns[new_column_index].name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int alter_table_rename_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan->is_noop) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_rename_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->new_column_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_rename_column(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table column");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int execute_physical_alter_table_rename_column(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
) {
    char *sql = NULL;
    int rc = build_alter_table_rename_column_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_modify_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_modify_column){0};
    out_plan->unsupported_object_message =
        "ALTER TABLE MODIFY COLUMN supports only persistent base tables";
    out_plan->rowid_alias_message =
        "ALTER TABLE MODIFY COLUMN requires an unshadowed SQLite rowid alias";
    out_plan->integer_support_message =
        "ALTER TABLE MODIFY COLUMN supports only compatible baseline integer, character, "
        "text, binary string, and temporal column replacements";
    out_plan->row_count_overflow_message =
        "too many rows to validate for ALTER TABLE MODIFY COLUMN";
    out_plan->failure_message = "failed to modify table column";

    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 1U), &out_plan->column);
    }
    if (rc == MYLITE_OK && out_plan->column.is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE MODIFY COLUMN does not support generated columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_inline_primary_key_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE MODIFY COLUMN does not support PRIMARY KEY"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_auto_increment_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE MODIFY COLUMN does not support AUTO_INCREMENT"
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_modify_column_position(database, child_at(statement, 2U), out_plan);
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->lookup_column_name,
            out_plan->column.name,
            sizeof(out_plan->lookup_column_name)
        );
        rc = resolve_alter_table_column_replacement_plan(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_modify_column){0};
    out_plan->checks_duplicate_replacement = true;
    out_plan->unsupported_object_message =
        "ALTER TABLE CHANGE COLUMN supports only persistent base tables";
    out_plan->rowid_alias_message =
        "ALTER TABLE CHANGE COLUMN requires an unshadowed SQLite rowid alias";
    out_plan->integer_support_message =
        "ALTER TABLE CHANGE COLUMN supports only compatible baseline integer, character, "
        "text, binary string, and temporal column replacements";
    out_plan->row_count_overflow_message =
        "too many rows to validate for ALTER TABLE CHANGE COLUMN";
    out_plan->failure_message = "failed to change table column";

    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            out_plan->lookup_column_name,
            sizeof(out_plan->lookup_column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->lookup_column_name)) {
        set_reserved_name_error(database, "column", out_plan->lookup_column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 2U), &out_plan->column);
    }
    if (rc == MYLITE_OK && out_plan->column.is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE CHANGE COLUMN does not support generated columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_inline_primary_key_column_definition(
            database,
            child_at(statement, 2U),
            "ALTER TABLE CHANGE COLUMN does not support PRIMARY KEY"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_auto_increment_column_definition(
            database,
            child_at(statement, 2U),
            "ALTER TABLE CHANGE COLUMN does not support AUTO_INCREMENT"
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_modify_column_position(database, child_at(statement, 3U), out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = resolve_alter_table_column_replacement_plan(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_modify_column_position(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_modify_column *out_plan
) {
    if (position == NULL) {
        return MYLITE_OK;
    }

    out_plan->has_position = true;
    if (position->kind == MYLITE_SQL_AST_COLUMN_POSITION_FIRST) {
        out_plan->position_first = true;
        return MYLITE_OK;
    }
    if (position->kind != MYLITE_SQL_AST_COLUMN_POSITION_AFTER) {
        set_runtime_error(database, "invalid ALTER TABLE column position");
        return MYLITE_ERROR;
    }

    if (copy_identifier_text(
            child_at(position, 0U),
            out_plan->after_column_name,
            sizeof(out_plan->after_column_name),
            database
        ) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    if (mylite_catalog_name_is_reserved(out_plan->after_column_name)) {
        set_reserved_name_error(database, "column", out_plan->after_column_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_alter_table_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_set_default *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_set_default){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ALTER COLUMN SET DEFAULT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->original_column = columns[column_index];
        planned_column_from_catalog_descriptor(
            &out_plan->original_column,
            child_at(statement, 2U),
            &out_plan->column
        );
        if (column_descriptor_is_text_family(&out_plan->original_column) ||
            column_descriptor_is_binary_blob_family(&out_plan->original_column)) {
            set_invalid_default_error(database, out_plan->original_column.name);
            rc = MYLITE_ERROR;
        } else {
            rc =
                validate_column_default(database, out_plan->column.default_node, &out_plan->column);
        }
    }
    if (rc == MYLITE_OK && planned_column_is_char_or_varchar(&out_plan->column) &&
        column_default_value_is_parenthesized_expression(out_plan->column.default_node)) {
        rc = finalize_planned_column_parenthesized_string_literal_default(
            database,
            &out_plan->column
        );
    } else if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = collect_no_zero_date_default_warnings(
            database,
            columns,
            column_count,
            (struct zero_temporal_default_warning_filter){
                .skipped_column_id = out_plan->original_column.column_id,
            },
            &out_plan->zero_temporal_default_warnings
        );
    }

    free(columns);

    return rc;
}

static void planned_alter_table_set_default_deinit(struct planned_alter_table_set_default *plan) {
    if (plan == NULL) {
        return;
    }

    zero_temporal_default_warnings_deinit(&plan->zero_temporal_default_warnings);
    *plan = (struct planned_alter_table_set_default){0};
}

static int alter_table_set_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_set_default *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = alter_table_set_default_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set column default");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int alter_table_set_default_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_set_default *plan
) {
    int rc = mylite_catalog_replace_column_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        plan->original_column.column_id,
        plan->original_column.name,
        plan->original_column.logical_type,
        plan->original_column.physical_type,
        plan->original_column.is_nullable,
        plan->original_column.is_visible,
        plan->original_column.is_auto_increment,
        plan->column.default_kind,
        plan->column.default_integer,
        plan->column.default_text,
        plan->original_column.on_update_current_timestamp,
        plan->original_column.character_set_name,
        plan->original_column.collation_name,
        plan->original_column.comment,
        plan->original_column.is_generated,
        plan->original_column.generated_kind,
        plan->original_column.generation_expression,
        plan->original_column.sqlite_generation_expression
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    return rc;
}

static int plan_alter_table_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_default *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_default){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ALTER COLUMN DROP DEFAULT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }

    free(columns);

    return rc;
}

static int alter_table_drop_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_default *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = alter_table_drop_default_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop column default");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int alter_table_drop_default_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_drop_default *plan
) {
    int rc = mylite_catalog_replace_column_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        plan->column.column_id,
        plan->column.name,
        plan->column.logical_type,
        plan->column.physical_type,
        plan->column.is_nullable,
        plan->column.is_visible,
        plan->column.is_auto_increment,
        MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT,
        0,
        NULL,
        plan->column.on_update_current_timestamp,
        plan->column.character_set_name,
        plan->column.collation_name,
        plan->column.comment,
        plan->column.is_generated,
        plan->column.generated_kind,
        plan->column.generation_expression,
        plan->column.sqlite_generation_expression
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    return rc;
}

static int plan_alter_table_column_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_column_visibility *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    size_t visible_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_column_visibility){0};
    out_plan->is_visible = mylite_sql_ast_node_column_visibility(statement) ==
                           MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ALTER COLUMN SET VISIBLE/INVISIBLE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        visible_count = count_visible_columns(columns, column_count);
        if (!out_plan->is_visible && columns[column_index].is_visible && visible_count <= 1U) {
            set_must_have_visible_column_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }

    free(columns);

    return rc;
}

static int alter_table_column_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_column_visibility *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_set_column_visibility_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->is_visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set column visibility");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_index_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_index_visibility *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t resolved_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_index_visibility){0};
    out_plan->is_visible = mylite_sql_ast_node_column_visibility(statement) ==
                           MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc =
            copy_identifier_text(child_at(statement, 1U), index_name, sizeof(index_name), database);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ALTER INDEX supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = find_loaded_index_by_name(indexes, index_count, index_name, &resolved_index);
        if (rc != MYLITE_OK) {
            set_key_does_not_exist_in_table_error(
                database,
                index_name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK &&
        indexes[resolved_index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        set_primary_key_index_invisible_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        !loaded_index_kind_is_secondary_or_fulltext(indexes[resolved_index].index.kind)) {
        set_unsupported_error(database, "ALTER TABLE ALTER INDEX supports only secondary indexes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->index = indexes[resolved_index];
        indexes[resolved_index] = (struct loaded_index_info){0};
    }
    if (rc == MYLITE_OK &&
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        rc = read_show_table_status_row_count(database, &out_plan->table, &out_plan->affected_rows);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int alter_table_index_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_index_visibility *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_set_index_visibility_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->index.index.index_id,
            plan->is_visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set index visibility");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static void planned_alter_table_index_visibility_deinit(
    struct planned_alter_table_index_visibility *plan
) {
    if (plan == NULL) {
        return;
    }
    loaded_index_info_deinit(&plan->index);
    *plan = (struct planned_alter_table_index_visibility){0};
}

static int plan_alter_table_default_charset_collation(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_default_charset_collation *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_default_charset_collation){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE DEFAULT CHARSET/COLLATE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->default_charset,
            out_plan->table.default_charset,
            strlen(out_plan->table.default_charset) + 1U
        );
        memcpy(
            out_plan->default_collation,
            out_plan->table.default_collation,
            strlen(out_plan->table.default_collation) + 1U
        );
        rc = validate_alter_table_default_charset_collation_options(
            database,
            child_at(statement, 1U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_alter_table_default_charset_collation_options(
            database,
            child_at(statement, 1U),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->changes_descriptor = false;
        if (strcmp(out_plan->default_charset, out_plan->table.default_charset) != 0) {
            out_plan->changes_descriptor = true;
        }
        if (strcmp(out_plan->default_collation, out_plan->table.default_collation) != 0) {
            out_plan->changes_descriptor = true;
        }
    }

    return rc;
}

static int apply_alter_table_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_default_charset_collation *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    bool has_charset_option = false;
    bool has_collation_option = false;
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            has_charset_option = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            has_collation_option = true;
        }
        table_option = table_option->next_sibling;
    }
    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = apply_table_charset_option(
                database,
                table_option,
                false,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = apply_table_collation_option(
                database,
                table_option,
                false,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK && has_charset_option && !has_collation_option) {
        rc = copy_default_collation_for_charset(
            database,
            plan->default_charset,
            plan->default_collation,
            sizeof(plan->default_collation)
        );
    }

    return rc;
}

static int plan_alter_table_convert_character_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_convert_character_set *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_convert_character_set){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE CONVERT TO CHARACTER SET supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->default_charset,
            out_plan->table.default_charset,
            strlen(out_plan->table.default_charset) + 1U
        );
        memcpy(
            out_plan->default_collation,
            out_plan->table.default_collation,
            strlen(out_plan->table.default_collation) + 1U
        );
        rc = apply_alter_table_convert_character_set_options(
            database,
            child_at(statement, 1U),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_convert_character_set_columns(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->changes_descriptor =
            (strcmp(out_plan->default_charset, out_plan->table.default_charset) != 0 ||
             strcmp(out_plan->default_collation, out_plan->table.default_collation) != 0) != 0;
        for (size_t index = 0U; !out_plan->changes_descriptor && index < out_plan->column_count;
             ++index) {
            if (alter_table_convert_column_needs_explicit_metadata_update(
                    &out_plan->columns[index],
                    out_plan
                )) {
                out_plan->changes_descriptor = true;
            }
        }
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_convert_character_set_deinit(out_plan);
    }

    return rc;
}

static int apply_alter_table_convert_character_set_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_convert_character_set *plan
) {
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    charset_option = child_at(table_options, 0U);
    collation_option = child_at(table_options, 1U);
    if (charset_option == NULL || charset_option->kind != MYLITE_SQL_AST_TABLE_CHARSET_OPTION ||
        child_at(table_options, 2U) != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (table_charset_option_is_default_keyword(charset_option)) {
        if (collation_option != NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = apply_alter_table_convert_character_set_default(database, plan);
    } else {
        rc = copy_table_charset_option_name(
            database,
            charset_option,
            charset_name,
            sizeof(charset_name)
        );
        if (rc == MYLITE_OK) {
            rc = copy_normalized_charset_name(
                database,
                charset_name,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        }
        if (rc == MYLITE_OK && collation_option == NULL) {
            rc = copy_default_collation_for_charset(
                database,
                plan->default_charset,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
    }
    if (rc == MYLITE_OK && collation_option != NULL) {
        rc = copy_table_collation_option_name(
            database,
            collation_option,
            collation_name,
            sizeof(collation_name)
        );
        if (rc == MYLITE_OK) {
            rc = copy_normalized_collation_name(
                database,
                collation_name,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
    }
    if (rc == MYLITE_OK && !text_equals_ascii_case_insensitive(plan->default_charset, "utf8mb4")) {
        set_unsupported_error(
            database,
            "ALTER TABLE CONVERT TO CHARACTER SET supports only utf8mb4 target character set"
        );
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values_with_binary(
            database,
            true,
            plan->default_charset,
            (collation_option != NULL || table_charset_option_is_default_keyword(charset_option)) !=
                0,
            plan->default_collation,
            true
        );
    }

    return rc;
}

static int apply_alter_table_convert_character_set_default(
    struct mylite_db *database,
    struct planned_alter_table_convert_character_set *plan
) {
    char charset_buffer[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_buffer[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *charset_name = NULL;
    const char *collation_name = NULL;
    int rc = database_character_set_system_variable_value(
        database,
        false,
        charset_buffer,
        sizeof(charset_buffer),
        &charset_name
    );

    if (rc == MYLITE_OK) {
        rc = database_collation_system_variable_value(
            database,
            false,
            collation_buffer,
            sizeof(collation_buffer),
            &collation_name
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (charset_name == NULL || strlen(charset_name) >= sizeof(plan->default_charset) ||
        collation_name == NULL || strlen(collation_name) >= sizeof(plan->default_collation)) {
        set_runtime_error(database, "database charset descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(plan->default_charset, charset_name, strlen(charset_name) + 1U);
    memcpy(plan->default_collation, collation_name, strlen(collation_name) + 1U);
    return MYLITE_OK;
}

static int validate_alter_table_convert_character_set_columns(
    struct mylite_db *database,
    const struct planned_alter_table_convert_character_set *plan
) {
    for (size_t index = 0U; index < plan->column_count; ++index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[index];
        const char *effective_charset = NULL;

        if (column_descriptor_is_national_char_or_varchar(column)) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support national character "
                "columns"
            );
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_enum(column) || column_descriptor_is_set(column)) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support ENUM or SET columns"
            );
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_binary_string_family(column) ||
            !alter_table_convert_column_participates(column)) {
            continue;
        }

        effective_charset = column_effective_character_set_name(&plan->table, column);
        if (effective_charset == NULL ||
            !text_equals_ascii_case_insensitive(effective_charset, "utf8mb4")) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support cross-character-set "
                "conversion"
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool table_charset_option_is_default_keyword(
    const struct mylite_sql_ast_node *charset_option
) {
    const struct mylite_sql_ast_node *name_node = child_at(charset_option, 0U);
    char keyword[sizeof("DEFAULT")];

    if (name_node == NULL || name_node->kind != MYLITE_SQL_AST_IDENTIFIER ||
        name_node->span.text == NULL || name_node->span.length != sizeof("DEFAULT") - 1U) {
        return false;
    }

    memcpy(keyword, name_node->span.text, name_node->span.length);
    keyword[name_node->span.length] = '\0';
    return text_equals_ascii_case_insensitive(keyword, "DEFAULT");
}

static bool alter_table_convert_column_participates(
    const struct mylite_catalog_column_descriptor *column
) {
    return column_descriptor_is_string_family(column);
}

static bool alter_table_convert_column_needs_explicit_metadata_update(
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_alter_table_convert_character_set *plan
) {
    if (!alter_table_convert_column_participates(column)) {
        return false;
    }
    if (column->character_set_name[0] == '\0' && column->collation_name[0] == '\0') {
        return false;
    }
    return (strcmp(column->character_set_name, plan->default_charset) != 0 ||
            strcmp(column->collation_name, plan->default_collation) != 0) != 0;
}

static void planned_alter_table_convert_character_set_deinit(
    struct planned_alter_table_convert_character_set *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_alter_table_convert_character_set){0};
}

static int plan_alter_table_comment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_comment *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_comment){0};
    rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target,
        &out_plan->table
    );
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_unsupported_error(database, "ALTER TABLE COMMENT supports only base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = decode_table_comment_option(
            database,
            child_at(statement, 1U),
            out_plan->target.table_name,
            out_plan->comment,
            sizeof(out_plan->comment)
        );
    }

    return rc;
}

static int plan_alter_table_order_by(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_order_by *out_plan
) {
    struct select_source_context source_context = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_order_by){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ORDER BY supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_primary_key_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support primary-key tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_secondary_index_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support secondary-index tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        source_context.source = &out_plan->target;
        rc = plan_alter_table_order_by_items(
            database,
            child_at(statement, 1U),
            &source_context,
            out_plan
        );
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_order_by_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_order_by_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_items,
    const struct select_source_context *source_context,
    struct planned_alter_table_order_by *out_plan
) {
    size_t item_count = mylite_sql_ast_node_child_count(order_items);
    int rc = MYLITE_OK;

    if (order_items == NULL || order_items->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST ||
        item_count == 0U) {
        set_unsupported_error(database, "ALTER TABLE ORDER BY requires descriptor order columns");
        return MYLITE_ERROR;
    }
    if (item_count > SIZE_MAX / sizeof(*out_plan->items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->items = calloc(item_count, sizeof(*out_plan->items));
    if (out_plan->items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->item_count = item_count;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < item_count; ++item_index) {
        rc = plan_alter_table_order_by_item(
            database,
            child_at(order_items, item_index),
            source_context,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->items[item_index]
        );
    }

    return rc;
}

static int plan_alter_table_order_by_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_alter_table_order_by_item *out_item
) {
    const struct mylite_sql_ast_node *direction = NULL;
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    *out_item = (struct planned_alter_table_order_by_item){0};
    if (item == NULL || item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
        set_unsupported_error(database, "ALTER TABLE ORDER BY requires descriptor order columns");
        return MYLITE_ERROR;
    }

    rc = resolve_order_column(
        database,
        child_at(item, 0U),
        source_context,
        table_columns,
        table_column_count,
        &out_item->column,
        &source_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_item->direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(item, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_item->direction = PLANNED_SELECT_ORDER_DESC;
    }

    return MYLITE_OK;
}

static void planned_alter_table_order_by_deinit(struct planned_alter_table_order_by *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    free(plan->items);
    *plan = (struct planned_alter_table_order_by){0};
}

static int alter_table_order_by_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_order_by *plan
) {
    int rc = execute_physical_alter_table_order_by(database, plan, &plan->affected_rows);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_internal_error_if_clear(database, rc, "failed to rebuild table order");
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_force(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_force *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_force){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "ALTER TABLE FORCE supports only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_primary_key_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support primary-key tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_secondary_index_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support secondary-index tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_force_deinit(out_plan);
    }

    return rc;
}

static void planned_alter_table_force_deinit(struct planned_alter_table_force *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_alter_table_force){0};
}

static int alter_table_force_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_force *plan
) {
    int rc = execute_physical_alter_table_force(database, plan);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_internal_error_if_clear(database, rc, "failed to force rebuild table");
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_key_maintenance(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_key_maintenance *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_key_maintenance){0};
    rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target,
        &out_plan->table
    );
    if (rc != MYLITE_OK) {
        *out_plan = (struct planned_alter_table_key_maintenance){0};
    }
    return rc;
}

static void planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *descriptor,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
) {
    *out_column = (struct planned_column){0};
    snprintf(out_column->name, sizeof(out_column->name), "%s", descriptor->name);
    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        descriptor->logical_type
    );
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        descriptor->physical_type
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    out_column->is_nullable = descriptor->is_nullable;
    out_column->is_visible = descriptor->is_visible;
    out_column->is_auto_increment = descriptor->is_auto_increment;
    out_column->on_update_current_timestamp = descriptor->on_update_current_timestamp;
    out_column->default_node = default_node;
    out_column->default_kind = descriptor->default_kind;
    out_column->default_integer = descriptor->default_integer;
    snprintf(
        out_column->default_text,
        sizeof(out_column->default_text),
        "%s",
        descriptor->default_text
    );
    snprintf(
        out_column->character_set_name,
        sizeof(out_column->character_set_name),
        "%s",
        descriptor->character_set_name
    );
    snprintf(
        out_column->collation_name,
        sizeof(out_column->collation_name),
        "%s",
        descriptor->collation_name
    );
    snprintf(out_column->comment, sizeof(out_column->comment), "%s", descriptor->comment);
    out_column->is_generated = descriptor->is_generated;
    out_column->generated_kind = descriptor->generated_kind;
    snprintf(
        out_column->generation_expression,
        sizeof(out_column->generation_expression),
        "%s",
        descriptor->generation_expression
    );
    snprintf(
        out_column->sqlite_generation_expression,
        sizeof(out_column->sqlite_generation_expression),
        "%s",
        descriptor->sqlite_generation_expression
    );
}

static int resolve_alter_table_column_replacement_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = mylite_catalog_read_table_by_name(
        database,
        out_plan->target.schema.schema_id,
        out_plan->target.table_name,
        &out_plan->table
    );

    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(
            database,
            out_plan->target.schema.name,
            out_plan->target.table_name
        );
        return MYLITE_ERROR;
    }
    if (out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, out_plan->unsupported_object_message);
        return MYLITE_ERROR;
    }
    rc = normalize_text_length_argument_for_default_charset(
        database,
        &out_plan->column,
        out_plan->table.default_charset
    );
    if (rc == MYLITE_OK) {
        rc = apply_default_binary_charset_to_column(
            database,
            out_plan->table.default_charset,
            &out_plan->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_column_default(database, out_plan->column.default_node, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reject_check_constraint_table_alter(
        database,
        out_plan->table.table_id,
        "ALTER TABLE column replacement does not yet support CHECK-constrained tables"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reject_foreign_key_table_alter(
        database,
        out_plan->table.table_id,
        "ALTER TABLE column replacement does not yet support foreign-key constrained tables"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, out_plan->lookup_column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(
                database,
                out_plan->lookup_column_name,
                out_plan->table.name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->checks_duplicate_replacement) {
        size_t replacement_index = 0U;

        if (find_column_index(columns, column_count, out_plan->column.name, &replacement_index) ==
                MYLITE_OK &&
            replacement_index != column_index) {
            set_duplicate_column_error(database, columns[replacement_index].name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->original_column = columns[column_index];
        out_plan->column_index = column_index;
        out_plan->column_count = column_count;
        rc = resolve_alter_table_modify_column_position(database, columns, column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = collect_no_zero_date_default_warnings(
            database,
            columns,
            column_count,
            (struct zero_temporal_default_warning_filter){
                .skipped_column_id = out_plan->original_column.column_id,
            },
            &out_plan->zero_temporal_default_warnings
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &out_plan->indexes,
            &out_plan->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_alter_table_modify_primary_key_nullability(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_modify_indexes(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = complete_alter_table_modify_column_plan(database, columns, out_plan);
    }

    free(columns);

    return rc;
}

static int apply_alter_table_modify_primary_key_nullability(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    for (size_t index = 0U; index < out_plan->index_count; ++index) {
        const struct loaded_index_info *key = &out_plan->indexes[index];

        if (key->index.kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
            !loaded_index_contains_column_id(key, out_plan->original_column.column_id)) {
            continue;
        }
        if (out_plan->column.nullability == MYLITE_SQL_AST_NULLABILITY_NULL) {
            set_primary_key_part_null_error(database);
            return MYLITE_ERROR;
        }
        if (out_plan->column.nullability == MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED) {
            out_plan->column.is_nullable = false;
        }
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_indexes(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    int rc = refresh_alter_table_modify_index_parts(plan);

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            rc = validate_alter_table_modify_primary_index(database, key);
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
            rc = validate_loaded_index_part_list(
                database,
                key->parts,
                key->part_count,
                key->index.is_unique
            );
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
            rc = validate_alter_table_modify_fulltext_index(database, key);
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            rc = validate_alter_table_modify_spatial_index(database, key);
        }
    }

    return rc;
}

static int refresh_alter_table_modify_index_parts(struct planned_alter_table_modify_column *plan) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        struct loaded_index_info *key = &plan->indexes[index];

        for (size_t part_index = 0U; part_index < key->part_count; ++part_index) {
            struct loaded_index_part *part = &key->parts[part_index];

            if (part->index_column.column_id == plan->original_column.column_id) {
                make_modify_target_descriptor(plan, &part->column);
            }
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_primary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    uint64_t total = 0U;

    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        uint64_t part_bytes = 0U;
        int rc = MYLITE_OK;

        if (part->column.is_nullable) {
            set_primary_key_part_null_error(database);
            return MYLITE_ERROR;
        }
        rc = column_descriptor_primary_key_part_key_bytes(database, &part->column, &part_bytes);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_fulltext_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid FULLTEXT index descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        struct planned_column column = {0};

        planned_column_from_catalog_descriptor(&index->parts[part_index].column, NULL, &column);
        int rc = validate_fulltext_index_column(database, &column);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_spatial_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    if (index == NULL || index->part_count != 1U) {
        set_runtime_error(database, "invalid SPATIAL index descriptor");
        return MYLITE_ERROR;
    }

    return validate_alter_table_add_spatial_column(database, &index->parts[0].column);
}

static int resolve_alter_table_modify_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    size_t after_column_index = 0U;
    int rc = MYLITE_OK;

    out_plan->target_column_index = out_plan->column_index;
    if (!out_plan->has_position) {
        return MYLITE_OK;
    }
    if (out_plan->position_first) {
        out_plan->target_column_index = 0U;
        out_plan->changes_position = out_plan->target_column_index != out_plan->column_index;
        return MYLITE_OK;
    }

    rc = find_column_index(columns, column_count, out_plan->after_column_name, &after_column_index);
    if (rc != MYLITE_OK || after_column_index == out_plan->column_index) {
        set_unknown_column_in_table_error(
            database,
            out_plan->after_column_name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }

    if (after_column_index < out_plan->column_index) {
        out_plan->target_column_index = after_column_index + 1U;
    } else {
        out_plan->target_column_index = after_column_index;
    }
    out_plan->changes_position = out_plan->target_column_index != out_plan->column_index;
    return MYLITE_OK;
}

static int complete_alter_table_modify_column_plan(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_modify_column *out_plan
) {
    bool supports_char_varchar_replacement = modify_column_char_varchar_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_text_family_replacement = modify_column_text_family_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_binary_string_replacement = modify_column_binary_string_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_temporal_replacement =
        modify_column_temporal_replacement_supported(&out_plan->original_column, &out_plan->column);
    int rc = MYLITE_OK;

    if (!supports_char_varchar_replacement && !supports_text_family_replacement &&
        !supports_binary_string_replacement && !supports_temporal_replacement &&
        (column_descriptor_is_string_family(&out_plan->original_column) ||
         column_descriptor_is_binary_string_family(&out_plan->original_column) ||
         column_descriptor_is_json(&out_plan->original_column) ||
         column_descriptor_is_bit(&out_plan->original_column) ||
         column_descriptor_is_decimal(&out_plan->original_column) ||
         column_descriptor_is_approximate(&out_plan->original_column) ||
         column_descriptor_is_date(&out_plan->original_column) ||
         column_descriptor_is_time(&out_plan->original_column) ||
         column_descriptor_is_datetime(&out_plan->original_column) ||
         column_descriptor_is_timestamp(&out_plan->original_column) ||
         column_descriptor_is_year(&out_plan->original_column) ||
         column_descriptor_is_enum(&out_plan->original_column) ||
         column_descriptor_is_set(&out_plan->original_column) ||
         column_descriptor_is_spatial(&out_plan->original_column) ||
         planned_column_is_string_family(&out_plan->column) ||
         planned_column_is_binary_string_family(&out_plan->column) ||
         planned_column_is_json(&out_plan->column) || planned_column_is_bit(&out_plan->column) ||
         planned_column_is_enum(&out_plan->column) || planned_column_is_set(&out_plan->column) ||
         planned_column_is_decimal(&out_plan->column) ||
         planned_column_is_approximate(&out_plan->column) ||
         planned_column_is_date(&out_plan->column) || planned_column_is_time(&out_plan->column) ||
         planned_column_is_datetime(&out_plan->column) ||
         planned_column_is_timestamp(&out_plan->column) ||
         planned_column_is_year(&out_plan->column) ||
         planned_column_is_spatial(&out_plan->column))) {
        set_unsupported_error(database, out_plan->integer_support_message);
        return MYLITE_ERROR;
    }
    if (modify_column_definition_matches(&out_plan->original_column, &out_plan->column)) {
        if (modify_column_name_matches(&out_plan->original_column, &out_plan->column)) {
            if (out_plan->changes_position) {
                goto rebuild_column;
            }
            if (out_plan->original_column.is_visible) {
                out_plan->is_noop = true;
            } else {
                out_plan->is_metadata_only = true;
            }
            return MYLITE_OK;
        }
        if (out_plan->changes_position) {
            goto rebuild_column;
        }
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }
    if (!out_plan->changes_position &&
        modify_column_metadata_only_replacement(&out_plan->original_column, &out_plan->column)) {
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }

rebuild_column:
    if (!modify_column_type_matches(&out_plan->original_column, &out_plan->column)) {
        out_plan->reports_rebuild_row_count = true;
    }
    rc = choose_sqlite_rowid_alias(
        database,
        columns,
        out_plan->column_count,
        out_plan->rowid_alias_message,
        &out_plan->rowid_alias
    );
    if (rc == MYLITE_OK) {
        rc = collect_modify_column_rebuild_columns(
            database,
            columns,
            out_plan->column_count,
            out_plan
        );
    }

    return rc;
}

static bool modify_column_definition_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (original_column->on_update_current_timestamp !=
        replacement_column->on_update_current_timestamp) {
        return false;
    }
    if (strcmp(original_column->character_set_name, replacement_column->character_set_name) != 0) {
        return false;
    }
    if (strcmp(original_column->collation_name, replacement_column->collation_name) != 0) {
        return false;
    }
    if (strcmp(original_column->comment, replacement_column->comment) != 0) {
        return false;
    }
    if (original_column->default_kind != replacement_column->default_kind) {
        return false;
    }
    if (original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
        original_column->default_integer != replacement_column->default_integer) {
        return false;
    }
    if (original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION &&
        original_column->default_integer != replacement_column->default_integer) {
        return false;
    }
    if ((original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
         column_default_kind_is_expression(original_column->default_kind)) &&
        strcmp(original_column->default_text, replacement_column->default_text) != 0) {
        return false;
    }

    return true;
}

static bool modify_column_type_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }

    return true;
}

static bool modify_column_metadata_only_replacement(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (!modify_column_integer_value_domain_matches(original_column, replacement_column)) {
        return false;
    }

    return true;
}

static bool modify_column_name_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->name, replacement_column->name) != 0) {
        return false;
    }

    return true;
}

static void planned_alter_table_modify_column_deinit(
    struct planned_alter_table_modify_column *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    zero_temporal_default_warnings_deinit(&plan->zero_temporal_default_warnings);
    loaded_index_infos_deinit(&plan->indexes, &plan->index_count);
    *plan = (struct planned_alter_table_modify_column){0};
}

static int alter_table_modify_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int64_t validated_row_count = 0;
    int rc = MYLITE_OK;

    if (plan->is_noop) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK && !plan->is_metadata_only) {
        rc = validate_modify_column_existing_rows(database, plan, &validated_row_count);
        if (rc == MYLITE_OK && plan->reports_rebuild_row_count) {
            plan->affected_rows = validated_row_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_replace_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->original_column.column_id,
            plan->column.name,
            plan->column.logical_type,
            plan->column.physical_type,
            plan->column.is_nullable,
            true,
            plan->column.is_auto_increment,
            plan->column.default_kind,
            plan->column.default_integer,
            plan->column.default_text,
            plan->column.on_update_current_timestamp,
            plan->column.character_set_name,
            plan->column.collation_name,
            plan->column.comment,
            plan->column.is_generated,
            plan->column.generated_kind,
            plan->column.generation_expression,
            plan->column.sqlite_generation_expression
        );
    }
    if (rc == MYLITE_OK && plan->changes_position) {
        const struct mylite_catalog_column_reorder reorder = {
            .table_id = plan->table.table_id,
            .columns = plan->columns,
            .column_count = plan->column_count,
            .metadata_replaced_column_id = plan->original_column.column_id,
        };

        rc = mylite_catalog_reorder_columns_in_mutation(database, &mutation, &reorder);
    }
    if (rc == MYLITE_OK && plan->is_metadata_only) {
        if (!modify_column_name_matches(&plan->original_column, &plan->column)) {
            struct planned_alter_table_rename_column rename_plan = {
                .target = plan->target,
                .table = plan->table,
                .column = plan->original_column,
                .is_noop = false,
            };

            memcpy(
                rename_plan.new_column_name,
                plan->column.name,
                sizeof(rename_plan.new_column_name)
            );
            rc = execute_physical_alter_table_rename_column(database, &rename_plan);
        }
    } else if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_modify_column(database, plan, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, plan->failure_message);
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (!plan->is_metadata_only ||
        !modify_column_name_matches(&plan->original_column, &plan->column)) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int collect_modify_column_rebuild_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *rebuild_columns = NULL;

    if (column_count > SIZE_MAX / sizeof(*rebuild_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rebuild_columns = calloc(column_count, sizeof(*rebuild_columns));
    if (rebuild_columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (!out_plan->changes_position) {
        for (size_t index = 0U; index < column_count; ++index) {
            rebuild_columns[index] = columns[index];
            if (index == out_plan->column_index) {
                make_modify_target_descriptor(out_plan, &rebuild_columns[index]);
            }
        }
    } else {
        size_t source_index = 0U;

        for (size_t index = 0U; index < column_count; ++index) {
            if (index == out_plan->target_column_index) {
                make_modify_target_descriptor(out_plan, &rebuild_columns[index]);
            } else {
                while (source_index < column_count && source_index == out_plan->column_index) {
                    ++source_index;
                }
                if (source_index >= column_count) {
                    free(rebuild_columns);
                    set_runtime_error(database, "failed to plan column reorder");
                    return MYLITE_ERROR;
                }
                rebuild_columns[index] = columns[source_index];
                ++source_index;
            }
        }
    }

    out_plan->columns = rebuild_columns;
    return MYLITE_OK;
}

static int validate_modify_column_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    int64_t *out_row_count
) {
    sqlite3_stmt *statement = NULL;
    struct mylite_catalog_column_descriptor target_column = {0};
    char *sql = NULL;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_modify_validation_sql(plan, &sql);

    *out_row_count = 0;
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    make_modify_target_descriptor(plan, &target_column);
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        int sqlite_type = sqlite3_column_type(statement, 0);

        if (row_number >= (size_t)INT64_MAX) {
            set_runtime_error(database, plan->row_count_overflow_message);
            rc = MYLITE_ERROR;
            break;
        }
        ++row_number;
        if (sqlite_type == SQLITE_NULL) {
            rc = validate_modify_column_null_value(database, &target_column, row_number);
        } else if (sqlite_type == SQLITE_INTEGER) {
            rc = validate_existing_integer_for_column(
                database,
                (int64_t)sqlite3_column_int64(statement, 0),
                &target_column,
                row_number,
                plan->integer_support_message
            );
        } else if (
            column_descriptor_is_binary_string_family(&target_column) &&
            (sqlite_type == SQLITE_TEXT || sqlite_type == SQLITE_BLOB)
        ) {
            rc = validate_existing_binary_bytes_for_column(
                database,
                sqlite3_column_blob(statement, 0),
                sqlite3_column_bytes(statement, 0),
                &target_column,
                row_number
            );
        } else if (sqlite_type == SQLITE_TEXT) {
            rc = validate_existing_text_for_column(
                database,
                sqlite3_column_text(statement, 0),
                sqlite3_column_bytes(statement, 0),
                &target_column,
                row_number
            );
        } else {
            set_physical_sqlite_row_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_OK) {
        *out_row_count = (int64_t)row_number;
    } else if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_modify_column_null_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    if (target_column->is_nullable) {
        return MYLITE_OK;
    }
    if (column_descriptor_is_text_family(target_column)) {
        set_data_truncated_error(database, target_column->name, row_number);
    } else {
        set_invalid_use_of_null_error(database);
    }

    return MYLITE_ERROR;
}

static int validate_existing_integer_for_column(
    struct mylite_db *database,
    int64_t value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    const char *unsupported_message
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(database, column, unsupported_message, &range);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value < 0) {
        const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + 1U;
        uint64_t magnitude = value == INT64_MIN ? int64_min_magnitude : (uint64_t)(-value);

        if (range.negative_abs_max == 0U || magnitude > range.negative_abs_max) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    if ((uint64_t)value > range.positive_max) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_existing_text_for_column(
    struct mylite_db *database,
    const unsigned char *text,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_varchar(column)) {
        return validate_varchar_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_char(column)) {
        return validate_canonical_char_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return validate_text_family_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_datetime(column)) {
        if (predicate_datetime_text_admitted(database, (const char *)text, (size_t)byte_count)) {
            return MYLITE_OK;
        }
        set_incorrect_datetime_value_error(database, (const char *)text, column->name, row_number);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_timestamp(column)) {
        if (predicate_timestamp_text_admitted(database, (const char *)text, (size_t)byte_count)) {
            return MYLITE_OK;
        }
        set_incorrect_datetime_value_error(database, (const char *)text, column->name, row_number);
        return MYLITE_ERROR;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static int validate_existing_binary_bytes_for_column(
    struct mylite_db *database,
    const void *bytes,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    char *copy = NULL;
    size_t copy_length = 0U;
    int rc = MYLITE_OK;

    if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    copy_length = (size_t)byte_count;
    copy = malloc(copy_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (copy_length != 0U) {
        memcpy(copy, bytes, copy_length);
    }
    copy[copy_length] = '\0';
    rc = convert_binary_bytes_for_column(database, column, row_number, false, &copy, &copy_length);
    free(copy);
    return rc;
}

static void make_modify_target_descriptor(
    const struct planned_alter_table_modify_column *plan,
    struct mylite_catalog_column_descriptor *out_column
) {
    *out_column = plan->original_column;
    memcpy(out_column->name, plan->column.name, sizeof(out_column->name));
    snprintf(
        out_column->logical_type,
        sizeof(out_column->logical_type),
        "%s",
        plan->column.logical_type
    );
    snprintf(
        out_column->physical_type,
        sizeof(out_column->physical_type),
        "%s",
        plan->column.physical_type
    );
    out_column->is_nullable = plan->column.is_nullable;
    out_column->on_update_current_timestamp = plan->column.on_update_current_timestamp;
    out_column->default_kind = plan->column.default_kind;
    out_column->default_integer = plan->column.default_integer;
    snprintf(
        out_column->default_text,
        sizeof(out_column->default_text),
        "%s",
        plan->column.default_text
    );
    snprintf(
        out_column->character_set_name,
        sizeof(out_column->character_set_name),
        "%s",
        plan->column.character_set_name
    );
    snprintf(
        out_column->collation_name,
        sizeof(out_column->collation_name),
        "%s",
        plan->column.collation_name
    );
}

static int execute_physical_alter_table_modify_column(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation
) {
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    int rc = build_modify_temporary_physical_name(
        plan,
        mutation,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        if (alter_table_modify_needs_binary_copy_materialization(plan)) {
            rc = execute_physical_alter_table_modify_copy_rows(
                database,
                plan,
                temporary_physical_name
            );
        } else {
            rc = build_alter_table_modify_copy_sql(plan, temporary_physical_name, &sql);
            if (rc == MYLITE_OK) {
                rc = execute_sqlite_schema_sql(database, sql);
            }
        }
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_indexes_sql(plan, &sql);
    }
    if (rc == MYLITE_OK && sql != NULL) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int execute_physical_alter_table_order_by(
    struct mylite_db *database,
    const struct planned_alter_table_order_by *plan,
    int64_t *out_affected_rows
) {
    sqlite3_stmt *statement = NULL;
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool transaction_started = false;
    bool copy_failed = false;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_order_temporary_physical_name(
        plan,
        database->session.sqlite_schema_generation + 1U,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    *out_affected_rows = 0;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = build_alter_table_order_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_order_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc != MYLITE_OK) {
        copy_failed = true;
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK) {
        ++database->session.sqlite_schema_generation;
        *out_affected_rows = affected_rows;
    }

    return rc;
}

static int execute_physical_alter_table_force(
    struct mylite_db *database,
    const struct planned_alter_table_force *plan
) {
    sqlite3_stmt *statement = NULL;
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool transaction_started = false;
    bool copy_failed = false;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_force_temporary_physical_name(
        plan,
        database->session.sqlite_schema_generation + 1U,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = build_alter_table_force_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_force_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc != MYLITE_OK) {
        copy_failed = true;
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK) {
        ++database->session.sqlite_schema_generation;
    }

    return rc;
}

static int build_alter_table_check_temporary_physical_name(
    const struct planned_alter_table_check_constraint *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (plan == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_table_%" PRId64 "_check_%" PRIu64,
        plan->table.table_id,
        sqlite_schema_generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_check_create_sql(
    const struct planned_alter_table_check_constraint *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    return build_create_table_definition_sql(
        &plan->rebuild,
        temporary_physical_name,
        false,
        out_sql
    );
}

static int build_alter_table_check_copy_sql(
    const struct planned_alter_table_check_constraint *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_check_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_check_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_alter_table_check_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_check_constraint *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->rebuild.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->rebuild.columns[column_index].name
            );
        }
    }

    return rc;
}

static int rename_table_from_plan_with_policy(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    struct mylite_catalog_table_descriptor source = {0};
    int rc = MYLITE_OK;

    if (allow_same_object_noop) {
        rc = mylite_catalog_read_table_by_name(
            database,
            plan->source.schema.schema_id,
            plan->source.table_name,
            &source
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                plan->source.schema.name,
                plan->source.table_name
            );
            return MYLITE_ERROR;
        }
        if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
            set_unsupported_error(database, unsupported_object_message);
            return MYLITE_ERROR;
        }
        if (source.schema_id == plan->target.schema.schema_id &&
            text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
            return MYLITE_OK;
        }
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = rename_table_pair_in_mutation(
            database,
            &mutation,
            plan,
            allow_same_object_noop,
            unsupported_object_message
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table descriptor");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int rename_table_pair_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_table_descriptor source = {0};
    struct mylite_catalog_table_descriptor target = {0};
    bool found = false;
    int rc = mylite_catalog_read_table_by_name(
        database,
        plan->source.schema.schema_id,
        plan->source.table_name,
        &source
    );

    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, plan->source.schema.name, plan->source.table_name);
        return MYLITE_ERROR;
    }
    if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, unsupported_object_message);
        return MYLITE_ERROR;
    }
    if (allow_same_object_noop && source.schema_id == plan->target.schema.schema_id &&
        text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &target,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read target table descriptor");
        return rc;
    }
    if (found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    rc = reject_rename_table_check_constraint_collisions(database, &source, plan);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_update_table_identity_in_mutation(
        database,
        mutation,
        source.table_id,
        plan->target.schema.schema_id,
        plan->target.table_name,
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_rename_generated_check_constraints_in_mutation(
            database,
            mutation,
            source.table_id,
            plan->target.table_name
        );
    }

    return rc;
}

static int reject_rename_table_check_constraint_collisions(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan
) {
    struct loaded_check_constraint_info *checks = NULL;
    size_t check_count = 0U;
    char check_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = load_table_check_constraint_infos(database, source->table_id, &checks, &check_count);

    for (size_t index = 0U; rc == MYLITE_OK && index < check_count; ++index) {
        rc = build_renamed_check_constraint_name(
            database,
            plan,
            &checks[index].check_constraint,
            check_name,
            sizeof(check_name)
        );
        for (size_t previous = 0U; rc == MYLITE_OK && previous < index; ++previous) {
            char previous_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

            rc = build_renamed_check_constraint_name(
                database,
                plan,
                &checks[previous].check_constraint,
                previous_name,
                sizeof(previous_name)
            );
            if (rc == MYLITE_OK && text_equals_ascii_case_insensitive(check_name, previous_name)) {
                set_duplicate_check_constraint_error(database, check_name);
                rc = MYLITE_ERROR;
            }
        }
        if (rc == MYLITE_OK) {
            rc = reject_rename_table_check_constraint_schema_collision(
                database,
                source,
                plan,
                check_name
            );
        }
    }

    loaded_check_constraint_infos_deinit(&checks, &check_count);
    return rc;
}

static int build_renamed_check_constraint_name(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (!check_constraint->name_is_generated) {
        written = snprintf(destination, destination_size, "%s", check_constraint->name);
    } else {
        written = snprintf(
            destination,
            destination_size,
            "%s_chk_%" PRId64,
            plan->target.table_name,
            check_constraint->generated_ordinal
        );
    }
    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "renamed CHECK constraint name is too long");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_rename_table_check_constraint_schema_collision(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan,
    const char *check_constraint_name
) {
    struct check_constraint_name_collision_context context = {
        .name = check_constraint_name,
        .excluded_table_id = source->table_id,
        .found = false,
    };
    int rc = mylite_catalog_for_each_check_constraint_in_schema(
        database,
        plan->target.schema.schema_id,
        reject_rename_table_check_constraint_schema_collision_callback,
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

static int reject_rename_table_check_constraint_schema_collision_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
) {
    struct check_constraint_name_collision_context *context = user_data;

    if (check_constraint == NULL || context == NULL || context->name == NULL) {
        return MYLITE_MISUSE;
    }
    if (check_constraint->table_id != context->excluded_table_id &&
        text_equals_ascii_case_insensitive(check_constraint->name, context->name)) {
        context->found = true;
    }

    return MYLITE_OK;
}

static int plan_load_data_infile(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_load_data_infile *out_plan
) {
    const struct mylite_sql_ast_node *file_name = child_at(statement, 0U);
    const struct mylite_sql_ast_node *table_name = child_at(statement, 1U);
    const struct mylite_sql_ast_node *ignore_lines = NULL;
    const struct mylite_sql_ast_node *column_list = NULL;
    struct primary_key_info primary_key = primary_key_info_init();
    int rc = MYLITE_OK;

    *out_plan = (struct planned_load_data_infile){0};
    for (size_t child_index = 2U; child_index < mylite_sql_ast_node_child_count(statement);
         ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(statement, child_index);

        if (child->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(child) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            ignore_lines = child;
        } else if (child->kind == MYLITE_SQL_AST_IDENTIFIER_LIST) {
            column_list = child;
        }
    }

    rc = decode_sql_string_literal(
        database,
        file_name,
        "LOAD DATA INFILE supports only string literal file names",
        "LOAD DATA INFILE file names do not support NUL bytes",
        &out_plan->file_path,
        &out_plan->file_path_length
    );
    if (rc == MYLITE_OK) {
        rc =
            parse_load_data_ignore_line_count(database, ignore_lines, &out_plan->ignore_line_count);
    }
    if (rc == MYLITE_OK) {
        rc = resolve_visible_writable_table_reference(
            database,
            table_name,
            &out_plan->insert.target,
            &out_plan->insert.table
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->insert.table.table_id,
            &out_plan->insert.columns,
            &out_plan->insert.column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            out_plan->insert.table.table_id,
            out_plan->insert.columns,
            out_plan->insert.column_count,
            &primary_key
        );
    }
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        out_plan->insert.has_primary_key = true;
        if (primary_key.part_count == 1U) {
            out_plan->insert.primary_key_column_index = primary_key.parts[0].column_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->insert.table.table_id,
            out_plan->insert.columns,
            out_plan->insert.column_count,
            &out_plan->insert.indexes,
            &out_plan->insert.index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_insert_target_foreign_key_infos(database, &out_plan->insert);
    }
    if (rc == MYLITE_OK) {
        rc = initialize_insert_auto_increment_plan(database, &out_plan->insert);
    }
    if (rc == MYLITE_OK) {
        rc = collect_load_data_target_indexes(
            database,
            column_list,
            &out_plan->insert,
            &out_plan->target_indexes,
            &out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_target_duplicate(
            database,
            out_plan->insert.columns,
            out_plan->target_indexes,
            out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_load_data_target_columns(
            database,
            &out_plan->insert,
            out_plan->target_indexes,
            out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_omitted_columns(
            database,
            &out_plan->insert,
            out_plan->target_indexes,
            out_plan->target_count
        );
    }

    primary_key_info_deinit(&primary_key);
    return rc;
}

static void planned_load_data_infile_deinit(struct planned_load_data_infile *plan) {
    if (plan == NULL) {
        return;
    }

    planned_insert_deinit(&plan->insert);
    free(plan->file_path);
    free(plan->target_indexes);
    *plan = (struct planned_load_data_infile){0};
}

static int collect_load_data_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t **out_indexes,
    size_t *out_index_count
) {
    size_t explicit_column_count = mylite_sql_ast_node_child_count(column_list);
    size_t column_count = explicit_column_count;
    bool omitted_column_list = false;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (column_list != NULL && column_list->kind != MYLITE_SQL_AST_IDENTIFIER_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    omitted_column_list = insert_column_list_is_omitted(column_list);
    if (explicit_column_count == 0U) {
        if (!omitted_column_list) {
            return MYLITE_OK;
        }
        column_count = count_visible_insert_target_columns(plan);
    }
    if (column_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (column_count == 0U) {
        return MYLITE_OK;
    }

    indexes = calloc(column_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (explicit_column_count == 0U) {
        collect_visible_insert_target_indexes(plan, indexes);
    } else {
        int rc = collect_explicit_insert_target_indexes(
            database,
            column_list,
            plan,
            indexes,
            column_count
        );

        if (rc != MYLITE_OK) {
            free(indexes);
            return rc;
        }
    }

    *out_indexes = indexes;
    *out_index_count = column_count;
    return MYLITE_OK;
}

static int validate_load_data_target_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t target_index = 0U; target_index < target_count; ++target_index) {
        const struct mylite_catalog_column_descriptor *column = NULL;
        size_t column_index = target_indexes[target_index];

        if (column_index >= plan->column_count) {
            set_runtime_error(database, "invalid LOAD DATA target column index");
            return MYLITE_ERROR;
        }
        column = &plan->columns[column_index];
        if (column->is_generated) {
            set_generated_column_value_error(database, column->name, plan->table.name);
            return MYLITE_ERROR;
        }
        if (strcmp(column->physical_type, "INTEGER") == 0 || column_descriptor_is_char(column) ||
            column_descriptor_is_varchar(column) || column_descriptor_is_text_family(column) ||
            column_descriptor_is_year(column) || column_descriptor_is_date(column) ||
            column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
            column_descriptor_is_timestamp(column)) {
            continue;
        }
        set_unsupported_error(
            database,
            "LOAD DATA INFILE supports only baseline integer, character, and temporal targets"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int parse_load_data_ignore_line_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
) {
    *out_value = 0U;
    if (literal == NULL) {
        return MYLITE_OK;
    }
    if (literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, out_value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "LOAD DATA IGNORE LINES supports only unsigned integer literals"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
