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
        rc = assign_create_table_foreign_key_ids(database, &mutation, table_id, plan);
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
    int64_t table_id,
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
        if (foreign_key->parent_is_self_reference) {
            foreign_key->parent_table.table_id = table_id;
            foreign_key->parent_index.table_id = table_id;
            if (foreign_key->parent_index_is_primary) {
                foreign_key->parent_index.index_id = plan->primary_key_index_id;
            } else if (foreign_key->parent_secondary_index_index < plan->secondary_index_count) {
                foreign_key->parent_index.index_id =
                    plan->secondary_indexes[foreign_key->parent_secondary_index_index].index_id;
            }
            if (foreign_key->parent_index.index_id <= 0) {
                set_runtime_error(database, "invalid self-referencing foreign-key parent index");
                return MYLITE_ERROR;
            }
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
        plan->auto_increment_next > 1 ? plan->auto_increment_next : 0,
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
        plan->min_rows,
        plan->max_rows,
        plan->avg_row_length,
        plan->delay_key_write,
        created_time_utc_epoch,
        0,
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
        int64_t parent_table_id =
            foreign_key->parent_is_self_reference ? table_id : foreign_key->parent_table.table_id;

        if (foreign_key->part_count == 0U || foreign_key->foreign_key_id <= 0 ||
            parent_table_id <= 0 || foreign_key->parent_index.index_id <= 0 ||
            foreign_key->child_index_id <= 0) {
            set_runtime_error(database, "invalid foreign-key descriptor");
            return MYLITE_ERROR;
        }

        rc = mylite_catalog_insert_foreign_key_in_mutation(
            database,
            mutation,
            foreign_key->foreign_key_id,
            table_id,
            parent_table_id,
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
            int64_t parent_column_id = part->parent_column.column_id;

            if (part->child_column_index >= plan->column_count ||
                column_ids[part->child_column_index] <= 0) {
                set_runtime_error(database, "invalid foreign-key descriptor");
                return MYLITE_ERROR;
            }
            if (foreign_key->parent_is_self_reference) {
                if (part->parent_column_index >= plan->column_count ||
                    column_ids[part->parent_column_index] <= 0) {
                    set_runtime_error(database, "invalid self-referencing foreign-key descriptor");
                    return MYLITE_ERROR;
                }
                parent_column_id = column_ids[part->parent_column_index];
            }
            if (parent_column_id <= 0) {
                set_runtime_error(database, "invalid foreign-key descriptor");
                return MYLITE_ERROR;
            }
            child_column_id = column_ids[part->child_column_index];
            rc = mylite_catalog_insert_foreign_key_column_in_mutation(
                database,
                mutation,
                foreign_key->foreign_key_id,
                table_id,
                parent_table_id,
                child_column_id,
                parent_column_id,
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
        .auto_increment_status = plan->auto_increment_next > 1 ? plan->auto_increment_next : 0,
        .key_block_size = plan->key_block_size,
        .pack_keys = plan->pack_keys,
        .checksum = plan->checksum,
        .stats_persistent = plan->stats_persistent,
        .stats_auto_recalc = plan->stats_auto_recalc,
        .stats_sample_pages = plan->stats_sample_pages,
        .min_rows = plan->min_rows,
        .max_rows = plan->max_rows,
        .avg_row_length = plan->avg_row_length,
        .delay_key_write = plan->delay_key_write,
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
            .default_text = column->default_text,
            .on_update_current_timestamp = column->on_update_current_timestamp,
            .comment = column->comment,
            .is_generated = column->is_generated,
            .generated_kind = column->generated_kind,
            .generation_expression = column->generation_expression,
            .sqlite_generation_expression = column->sqlite_generation_expression,
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
        rc = intern_temporary_column_descriptor_strings(database, descriptor);
    }
    if (rc != MYLITE_OK) {
        free(column_ids);
        return rc;
    }

    *out_column_ids = column_ids;
    return MYLITE_OK;
}

static int intern_temporary_column_descriptor_strings(
    struct mylite_db *database,
    struct mylite_catalog_column_descriptor *column
) {
    int rc = mylite_catalog_string_pool_intern_c_string(
        &database->catalog_strings,
        column->default_text,
        &column->default_text
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_string_pool_intern_c_string(
            &database->catalog_strings,
            column->comment,
            &column->comment
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_string_pool_intern_c_string(
            &database->catalog_strings,
            column->generation_expression,
            &column->generation_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_string_pool_intern_c_string(
            &database->catalog_strings,
            column->sqlite_generation_expression,
            &column->sqlite_generation_expression
        );
    }
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
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
