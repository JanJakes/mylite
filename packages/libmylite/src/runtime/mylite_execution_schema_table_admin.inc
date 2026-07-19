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
        mylite_catalog_invalidate_descriptor_cache(database);
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
        rc = mylite_catalog_rollback_mutation(database, &mutation, rc);
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
