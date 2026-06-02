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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
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

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
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

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
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
