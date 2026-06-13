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
    rc = mylite_execution_load_table_columns(
        database,
        plan->table.table_id,
        &out_metadata->columns,
        &out_metadata->column_count
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            plan->table.table_id,
            out_metadata->columns,
            out_metadata->column_count,
            &out_metadata->indexes,
            &out_metadata->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
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

    mylite_execution_loaded_foreign_key_infos_deinit(&metadata->foreign_keys, &metadata->foreign_key_count);
    mylite_execution_loaded_index_infos_deinit(&metadata->indexes, &metadata->index_count);
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
        rc = mylite_execution_load_table_columns(
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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
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

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
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
    mylite_execution_loaded_foreign_key_info_deinit(&plan->foreign_key);
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
        rc = mylite_execution_load_table_columns(
            database,
            context->table.table_id,
            &context->columns,
            &context->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            context->table.table_id,
            context->columns,
            context->column_count,
            &context->indexes,
            &context->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            context->table.table_id,
            context->columns,
            context->column_count,
            &context->foreign_keys,
            &context->foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_check_constraint_infos(
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
    mylite_execution_loaded_check_constraint_infos_deinit(
        &context->check_constraints,
        &context->check_constraint_count
    );
    mylite_execution_loaded_foreign_key_infos_deinit(&context->foreign_keys, &context->foreign_key_count);
    mylite_execution_loaded_index_infos_deinit(&context->indexes, &context->index_count);
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
        return MYLITE_OK;
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
    if (column_descriptor_is_char_or_varchar(column)) {
        return validate_descriptor_string_key_column(database, column);
    }
    if (column_descriptor_is_bit(column) || column_descriptor_is_enum(column) ||
        column_descriptor_is_set(column) || column_descriptor_is_decimal(column) ||
        column_descriptor_is_approximate(column) || column_descriptor_is_year(column) ||
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

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static void planned_drop_index_deinit(struct planned_drop_index *plan) {
    if (plan == NULL) {
        return;
    }
    mylite_execution_loaded_index_info_deinit(&plan->index);
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

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
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
    mylite_execution_loaded_index_info_deinit(&plan->index);
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
