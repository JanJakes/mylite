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
