static int execute_start_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *characteristic_list = child_at(statement, 0U);
    struct transaction_characteristics characteristics = {
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    enum mylite_transaction_isolation effective_isolation =
        MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    enum mylite_transaction_access_mode effective_access_mode =
        MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    bool needs_consistent_snapshot_warning = false;
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (characteristic_list != NULL) {
        rc = collect_transaction_characteristics(database, characteristic_list, &characteristics);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }
    if (characteristics.has_isolation) {
        effective_isolation = characteristics.isolation;
    } else {
        effective_isolation = current_transaction_isolation(database);
    }
    effective_access_mode = effective_start_transaction_access_mode(database, &characteristics);
    if (characteristics.has_consistent_snapshot) {
        if (effective_isolation != MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ) {
            needs_consistent_snapshot_warning = true;
        }
    }
    if (needs_consistent_snapshot_warning) {
        rc = reserve_consistent_snapshot_ignored_warning(database);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }

    clear_session_table_locks(database);
    if (database->session.user_transaction_active) {
        rc = reconcile_persistent_auto_increment_high_waters(database);
        if (rc == MYLITE_OK) {
            rc = normalize_sqlite_control_rc(
                database,
                execute_sqlite_control_sql(database, "COMMIT")
            );
        }
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
            clear_persistent_auto_increment_high_waters(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = normalize_sqlite_control_rc(
            database,
            execute_sqlite_control_sql(database, "BEGIN IMMEDIATE")
        );
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = true;
            database->session.active_transaction_read_only =
                effective_access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY;
            clear_next_transaction_characteristics(database);
        }
    }
    if (rc == MYLITE_OK && needs_consistent_snapshot_warning) {
        rc = append_consistent_snapshot_ignored_warning(database);
    }
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_commit_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (database->session.user_transaction_active) {
        rc = reconcile_persistent_auto_increment_high_waters(database);
        if (rc == MYLITE_OK) {
            rc = normalize_sqlite_control_rc(
                database,
                execute_sqlite_control_sql(database, "COMMIT")
            );
        }
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
            clear_persistent_auto_increment_high_waters(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_rollback_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (database->session.user_transaction_active) {
        rc =
            normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "ROLLBACK"));
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
            rc = reconcile_temporary_physical_tables_after_user_rollback(database);
            if (rc == MYLITE_OK) {
                rc = reconcile_persistent_auto_increment_high_waters(database);
            }
            if (rc == MYLITE_OK) {
                clear_persistent_auto_increment_high_waters(database);
            }
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int ensure_persistent_auto_increment_high_water_slot(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_session_state *session = NULL;
    struct mylite_session_auto_increment_high_water *high_waters = NULL;
    size_t new_capacity = 0U;

    if (database == NULL || table == NULL || !database->session.user_transaction_active ||
        table->kind != MYLITE_CATALOG_TABLE_KIND_BASE || table->table_id <= 0) {
        return MYLITE_OK;
    }

    session = &database->session;
    for (size_t index = 0U; index < session->auto_increment_high_water_count; ++index) {
        if (session->auto_increment_high_waters[index].table_id == table->table_id) {
            return MYLITE_OK;
        }
    }

    if (session->auto_increment_high_water_count == session->auto_increment_high_water_capacity) {
        new_capacity = session->auto_increment_high_water_capacity == 0U
                           ? 4U
                           : session->auto_increment_high_water_capacity * 2U;
        if (new_capacity < session->auto_increment_high_water_capacity ||
            new_capacity > SIZE_MAX / sizeof(*high_waters)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        high_waters = (struct mylite_session_auto_increment_high_water *)
            realloc(session->auto_increment_high_waters, new_capacity * sizeof(*high_waters));
        if (high_waters == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        session->auto_increment_high_waters = high_waters;
        session->auto_increment_high_water_capacity = new_capacity;
    }

    session->auto_increment_high_waters[session->auto_increment_high_water_count] =
        (struct mylite_session_auto_increment_high_water){
            .table_id = table->table_id,
            .next_value = 0,
        };
    ++session->auto_increment_high_water_count;
    return MYLITE_OK;
}

static int record_persistent_auto_increment_high_water(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t next_value
) {
    struct mylite_session_state *session = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || table == NULL || !database->session.user_transaction_active ||
        table->kind != MYLITE_CATALOG_TABLE_KIND_BASE || table->table_id <= 0 || next_value <= 0) {
        return MYLITE_OK;
    }

    session = &database->session;
    for (size_t index = 0U; index < session->auto_increment_high_water_count; ++index) {
        struct mylite_session_auto_increment_high_water *high_water =
            &session->auto_increment_high_waters[index];

        if (high_water->table_id == table->table_id) {
            if (next_value > high_water->next_value) {
                high_water->next_value = next_value;
            }
            return MYLITE_OK;
        }
    }

    rc = ensure_persistent_auto_increment_high_water_slot(database, table);
    for (size_t index = 0U; rc == MYLITE_OK && index < session->auto_increment_high_water_count;
         ++index) {
        struct mylite_session_auto_increment_high_water *high_water =
            &session->auto_increment_high_waters[index];

        if (high_water->table_id == table->table_id) {
            high_water->next_value = next_value;
            break;
        }
    }
    return rc;
}

static int reconcile_persistent_auto_increment_high_waters(struct mylite_db *database) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    bool started_transaction = false;
    int rc = MYLITE_OK;

    if (session == NULL || session->auto_increment_high_water_count == 0U ||
        !has_persistent_auto_increment_high_water_updates(session)) {
        return MYLITE_OK;
    }

    rc = begin_persistent_auto_increment_high_water_reconciliation(database, &started_transaction);
    if (rc == MYLITE_OK) {
        rc = apply_persistent_auto_increment_high_waters(
            database,
            session->auto_increment_high_waters,
            session->auto_increment_high_water_count
        );
    }
    if (started_transaction) {
        if (rc == MYLITE_OK) {
            rc = commit_persistent_auto_increment_high_water_reconciliation(database);
            if (rc != MYLITE_OK) {
                rollback_persistent_auto_increment_high_water_reconciliation(database);
            }
        } else {
            rollback_persistent_auto_increment_high_water_reconciliation(database);
        }
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to reconcile auto-increment counters");
    }
    return rc;
}

static bool has_persistent_auto_increment_high_water_updates(
    const struct mylite_session_state *session
) {
    for (size_t index = 0U; index < session->auto_increment_high_water_count; ++index) {
        const struct mylite_session_auto_increment_high_water *high_water =
            &session->auto_increment_high_waters[index];

        if (high_water->table_id > 0 && high_water->next_value > 0) {
            return true;
        }
    }

    return false;
}

static int begin_persistent_auto_increment_high_water_reconciliation(
    struct mylite_db *database,
    bool *out_started_transaction
) {
    int rc = MYLITE_OK;

    *out_started_transaction = false;
    if (database->session.user_transaction_active ||
        sqlite3_get_autocommit(database->sqlite) == 0) {
        return MYLITE_OK;
    }

    rc = normalize_sqlite_control_rc(
        database,
        execute_sqlite_control_sql(database, "BEGIN IMMEDIATE")
    );

    if (rc == MYLITE_OK) {
        *out_started_transaction = true;
    }
    return rc;
}

static int commit_persistent_auto_increment_high_water_reconciliation(struct mylite_db *database) {
    return normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
}

static void rollback_persistent_auto_increment_high_water_reconciliation(
    const struct mylite_db *database
) {
    (void)execute_sqlite_control_sql(database, "ROLLBACK");
}

static int apply_persistent_auto_increment_high_waters(
    struct mylite_db *database,
    const struct mylite_session_auto_increment_high_water *high_waters,
    size_t high_water_count
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_sqlite_statement(
        database,
        "UPDATE _mylite_catalog_tables "
        "SET auto_increment_next = ?1 "
        "WHERE table_id = ?2 AND auto_increment_next < ?1",
        &statement
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < high_water_count; ++index) {
        rc = apply_persistent_auto_increment_high_water(statement, &high_waters[index]);
    }

    rc = finalize_sqlite_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_invalidate_descriptor_cache(database);
    }
    return rc;
}

static int apply_persistent_auto_increment_high_water(
    sqlite3_stmt *statement,
    const struct mylite_session_auto_increment_high_water *high_water
) {
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    if (high_water->table_id <= 0 || high_water->next_value <= 0) {
        return MYLITE_OK;
    }

    sqlite_rc = sqlite3_reset(statement);
    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_clear_bindings(statement);
    }
    rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_bind_int64(statement, 1, (sqlite3_int64)high_water->next_value);
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_bind_int64(statement, 2, (sqlite3_int64)high_water->table_id);
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    return rc;
}

static void clear_persistent_auto_increment_high_waters(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    database->session.auto_increment_high_water_count = 0U;
}

static int reconcile_temporary_physical_tables_after_user_rollback(struct mylite_db *database) {
    bool schema_changed = false;
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK && table_index < database->session.temporary_catalog.table_count;
         ++table_index) {
        const struct mylite_temporary_catalog_table *table =
            &database->session.temporary_catalog.tables[table_index];
        bool exists = false;

        rc = temporary_physical_table_exists(database, table->table.physical_name, &exists);
        if (rc == MYLITE_OK && !exists) {
            rc = rebuild_temporary_physical_table(database, table);
            if (rc == MYLITE_OK) {
                schema_changed = true;
            }
        }
    }

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to reconcile temporary tables");
        return rc;
    }
    if (schema_changed) {
        ++database->session.sqlite_schema_generation;
    }
    return MYLITE_OK;
}

static int temporary_physical_table_exists(
    struct mylite_db *database,
    const char *physical_name,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = prepare_sqlite_statement(
        database,
        "SELECT 1 FROM temp.sqlite_master WHERE type = 'table' AND name = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_bind_text(statement, 1, physical_name, -1, SQLITE_TRANSIENT);
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    return finalize_sqlite_statement(statement, rc);
}

static int rebuild_temporary_physical_table(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table
) {
    struct planned_create_table plan = {0};
    int rc = rebuild_temporary_create_table_plan(database, table, &plan);

    if (rc == MYLITE_OK) {
        rc = execute_physical_create_table(database, &plan, table->table.physical_name, true);
    }
    planned_create_table_deinit(&plan);
    return rc;
}

static int rebuild_temporary_create_table_plan(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    struct planned_create_table *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_table){
        .auto_increment_next =
            table->table.auto_increment_next > 0 ? table->table.auto_increment_next : 1,
        .key_block_size = table->table.key_block_size,
        .pack_keys = table->table.pack_keys,
        .checksum = table->table.checksum,
        .stats_persistent = table->table.stats_persistent,
        .stats_auto_recalc = table->table.stats_auto_recalc,
        .stats_sample_pages = table->table.stats_sample_pages,
        .min_rows = table->table.min_rows,
        .max_rows = table->table.max_rows,
        .avg_row_length = table->table.avg_row_length,
        .delay_key_write = table->table.delay_key_write,
    };
    snprintf(
        out_plan->target.schema.name,
        sizeof(out_plan->target.schema.name),
        "%s",
        table->schema_name
    );
    out_plan->target.schema.schema_id = table->table.schema_id;
    snprintf(
        out_plan->target.table_name,
        sizeof(out_plan->target.table_name),
        "%s",
        table->table.name
    );
    snprintf(
        out_plan->default_charset,
        sizeof(out_plan->default_charset),
        "%s",
        table->table.default_charset
    );
    snprintf(
        out_plan->default_collation,
        sizeof(out_plan->default_collation),
        "%s",
        table->table.default_collation
    );
    snprintf(out_plan->comment, sizeof(out_plan->comment), "%s", table->table.comment);
    snprintf(
        out_plan->row_format_option,
        sizeof(out_plan->row_format_option),
        "%s",
        table->table.row_format_option
    );

    rc = rebuild_temporary_create_table_columns(database, table, out_plan);
    if (rc == MYLITE_OK) {
        rc = rebuild_temporary_create_table_indexes(database, table, out_plan);
    }
    return rc;
}

static int rebuild_temporary_create_table_columns(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    struct planned_create_table *out_plan
) {
    if (table->column_count > SIZE_MAX / sizeof(*out_plan->columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->columns = calloc(table->column_count, sizeof(*out_plan->columns));
    if (out_plan->columns == NULL && table->column_count != 0U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->column_count = table->column_count;

    for (size_t column_index = 0U; column_index < table->column_count; ++column_index) {
        planned_column_from_catalog_descriptor(
            &table->columns[column_index],
            NULL,
            &out_plan->columns[column_index]
        );
    }
    return MYLITE_OK;
}

static int rebuild_temporary_create_table_indexes(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    struct planned_create_table *out_plan
) {
    int rc = MYLITE_OK;

    for (size_t index_index = 0U; rc == MYLITE_OK && index_index < table->index_count;
         ++index_index) {
        const struct mylite_catalog_index_descriptor *index = &table->indexes[index_index];

        if (index->kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            rc = rebuild_temporary_create_table_primary_index(database, table, index, out_plan);
        } else if (index->kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
            rc = rebuild_temporary_create_table_secondary_index(database, table, index, out_plan);
        } else {
            rc = MYLITE_ERROR;
        }
    }
    return rc;
}

static int rebuild_temporary_create_table_primary_index(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    const struct mylite_catalog_index_descriptor *index,
    struct planned_create_table *out_plan
) {
    size_t part_count = temporary_index_part_count(table, index->index_id);
    int rc = reserve_planned_primary_key_parts(database, out_plan, part_count);

    if (part_count == 0U) {
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_plan->has_primary_key = true;
    out_plan->primary_key_index_id = index->index_id;
    out_plan->primary_key_show_create_explicit_btree = index->show_create_explicit_btree;
    snprintf(
        out_plan->primary_key_physical_name,
        sizeof(out_plan->primary_key_physical_name),
        "%s",
        index->physical_name
    );
    snprintf(
        out_plan->primary_key_comment,
        sizeof(out_plan->primary_key_comment),
        "%s",
        index->comment
    );

    for (size_t part_index = 0U; part_index < part_count; ++part_index) {
        const struct mylite_catalog_index_column_descriptor *part =
            temporary_index_part_by_ordinal(table, index->index_id, (int64_t)part_index + 1);
        size_t column_index = 0U;

        if (part == NULL || !temporary_column_index_by_id(table, part->column_id, &column_index)) {
            return MYLITE_ERROR;
        }
        out_plan->primary_key_parts[out_plan->primary_key_part_count] =
            (struct planned_primary_key_part){
                .column_index = column_index,
                .sort_direction = part->sort_direction,
            };
        ++out_plan->primary_key_part_count;
        out_plan->columns[column_index].is_primary_key = true;
        out_plan->columns[column_index].is_nullable = false;
    }
    return MYLITE_OK;
}

static int rebuild_temporary_create_table_secondary_index(
    struct mylite_db *database,
    const struct mylite_temporary_catalog_table *table,
    const struct mylite_catalog_index_descriptor *index,
    struct planned_create_table *out_plan
) {
    struct planned_secondary_index *planned = NULL;
    size_t part_count = temporary_index_part_count(table, index->index_id);
    int rc = MYLITE_OK;

    if (part_count == 0U) {
        return MYLITE_ERROR;
    }
    rc =
        reserve_planned_secondary_indexes(database, out_plan, out_plan->secondary_index_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }
    planned = &out_plan->secondary_indexes[out_plan->secondary_index_count];
    *planned = (struct planned_secondary_index){
        .index_id = index->index_id,
        .kind = index->kind,
        .is_unique = index->is_unique,
        .is_visible = index->is_visible,
        .show_create_explicit_btree = index->show_create_explicit_btree,
    };
    snprintf(planned->name, sizeof(planned->name), "%s", index->name);
    snprintf(planned->physical_name, sizeof(planned->physical_name), "%s", index->physical_name);
    snprintf(planned->comment, sizeof(planned->comment), "%s", index->comment);

    rc = reserve_planned_secondary_index_parts(database, planned, part_count);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        const struct mylite_catalog_index_column_descriptor *part =
            temporary_index_part_by_ordinal(table, index->index_id, (int64_t)part_index + 1);
        size_t column_index = 0U;

        if (part == NULL || !temporary_column_index_by_id(table, part->column_id, &column_index)) {
            rc = MYLITE_ERROR;
            break;
        }
        planned->parts[planned->part_count] = (struct planned_secondary_index_part){
            .column_index = column_index,
            .has_prefix_length = part->has_prefix_length,
            .prefix_length = part->prefix_length,
            .sort_direction = part->sort_direction,
        };
        ++planned->part_count;
    }
    if (rc != MYLITE_OK) {
        free(planned->parts);
        *planned = (struct planned_secondary_index){0};
        return rc;
    }
    ++out_plan->secondary_index_count;
    return MYLITE_OK;
}

static size_t temporary_index_part_count(
    const struct mylite_temporary_catalog_table *table,
    int64_t index_id
) {
    size_t count = 0U;

    for (size_t part_index = 0U; part_index < table->index_column_count; ++part_index) {
        if (table->index_columns[part_index].index_id == index_id) {
            ++count;
        }
    }
    return count;
}

static const struct mylite_catalog_index_column_descriptor *temporary_index_part_by_ordinal(
    const struct mylite_temporary_catalog_table *table,
    int64_t index_id,
    int64_t ordinal_position
) {
    for (size_t part_index = 0U; part_index < table->index_column_count; ++part_index) {
        const struct mylite_catalog_index_column_descriptor *part =
            &table->index_columns[part_index];

        if (part->index_id == index_id && part->ordinal_position == ordinal_position) {
            return part;
        }
    }
    return NULL;
}

static bool temporary_column_index_by_id(
    const struct mylite_temporary_catalog_table *table,
    int64_t column_id,
    size_t *out_column_index
) {
    for (size_t column_index = 0U; column_index < table->column_count; ++column_index) {
        if (table->columns[column_index].column_id == column_id) {
            *out_column_index = column_index;
            return true;
        }
    }
    return false;
}

static int execute_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char savepoint_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_savepoint_name(
        database,
        child_at(statement, 0U),
        savepoint_name,
        sizeof(savepoint_name)
    );
    if (rc == MYLITE_OK && database->session.user_transaction_active) {
        rc = create_or_replace_user_savepoint(database, savepoint_name);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_rollback_to_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char savepoint_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_savepoint_name(
        database,
        child_at(statement, 0U),
        savepoint_name,
        sizeof(savepoint_name)
    );
    if (rc == MYLITE_OK) {
        rc = rollback_to_user_savepoint(database, savepoint_name);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_release_savepoint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char savepoint_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_savepoint_name(
        database,
        child_at(statement, 0U),
        savepoint_name,
        sizeof(savepoint_name)
    );
    if (rc == MYLITE_OK) {
        rc = release_user_savepoint(database, savepoint_name);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}
