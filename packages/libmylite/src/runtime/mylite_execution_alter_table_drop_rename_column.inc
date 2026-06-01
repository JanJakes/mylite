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
