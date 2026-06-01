static int execute_show_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_show_create_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_show_create_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_show_create_table_from_plan(database, &plan, result);
    }
    planned_show_create_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_view_descriptor view = {0};
    mylite_result *result = NULL;
    bool missing_schema = false;
    const struct mylite_execution_catalog_builtin_sys_view *selected_builtin_sys_view = NULL;
    const struct mylite_execution_catalog_builtin_sys_view *builtin_sys_view = NULL;
    int rc = show_create_statement_targets_selected_builtin_sys_view(
        database,
        statement,
        &selected_builtin_sys_view
    );

    if (rc == MYLITE_OK && selected_builtin_sys_view != NULL) {
        return execute_show_create_builtin_sys_view_statement(
            database,
            selected_builtin_sys_view,
            false,
            out_result
        );
    }

    if (rc == MYLITE_OK) {
        rc = resolve_table_name_allow_missing_schema(
            database,
            child_at(statement, 0U),
            &target,
            &missing_schema
        );
    }

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(target.table_name)) {
        set_reserved_name_error(database, "table", target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        builtin_sys_view = show_create_target_builtin_sys_view(&target);
    }
    if (rc == MYLITE_OK && builtin_sys_view != NULL) {
        return execute_show_create_builtin_sys_view_statement(
            database,
            builtin_sys_view,
            show_create_target_was_schema_qualified(statement),
            out_result
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_persistent_metadata_table_reference(database, &target, missing_schema, &table);
    }
    if (rc == MYLITE_OK && table.kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
        set_not_view_error(database, target.schema.name, target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_view_by_table_id(database, table.table_id, &view);
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_view_result(database, result, table.name, &view);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_builtin_sys_view_statement(
    struct mylite_db *database,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_show_create_builtin_sys_view_result(database, result, view, schema_qualified);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_database_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_create_database_result_column_count] = {
        "Database",
        "Create Database",
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *create_sql = NULL;
    const char *values[show_create_database_result_column_count] = {NULL, NULL};
    mylite_result *result = NULL;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_schema_name(database, schema_name, &schema);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_database_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_create_database_sql(&schema, &create_sql);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        values[0] = schema.name;
        values[1] = create_sql;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free(create_sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_engines_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_engines_result_column_count] = {
        "Engine",
        "Support",
        "Comment",
        "Transactions",
        "XA",
        "Savepoints",
    };
    static const char *const values[show_engines_result_column_count] = {
        "InnoDB",
        "DEFAULT",
        "Supports transactions, row-level locking, and foreign keys",
        "YES",
        "YES",
        "YES",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_engines_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_engine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_engine_status_result_column_count] = {
        "Type",
        "Name",
        "Status",
    };
    static const char *const values[show_engine_status_result_column_count] = {
        "InnoDB",
        "",
        "MyLite embedded InnoDB-compatible storage engine is active",
    };
    mylite_result *result = NULL;
    int rc = validate_show_engine_name(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_engine_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int validate_show_engine_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    char engine_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(statement, 0U),
        engine_name,
        sizeof(engine_name),
        (struct table_option_name_policy){
            .identifier_kind = "storage engine",
            .nul_message = "storage engine names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(engine_name, "InnoDB")) {
        set_unknown_storage_engine_error(database, engine_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_show_plugins_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_plugins_result_column_count] = {
        "Name",
        "Status",
        "Type",
        "Library",
        "License",
    };
    static const char *const values[show_plugins_result_column_count] = {
        "InnoDB",
        "ACTIVE",
        "STORAGE ENGINE",
        NULL,
        "GPL",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_plugins_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_databases_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *filter_node = child_at(statement, 0U);
    const struct mylite_sql_ast_node *like_node = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_databases_context context = {0};
    char *column_name = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        like_node = filter_node;
    } else if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else if (filter_node != NULL) {
        set_runtime_error(database, "invalid SHOW DATABASES filter");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_databases_column_name(&filter, &column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_databases_where_clause(database, where_clause);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        rc = append_builtin_show_database_names(&context);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(database, collect_show_database, &context);
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW DATABASES result");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_sorted_show_database_rows(database, result, &filter, where_clause, &context);
    }
    if (rc != MYLITE_OK) {
        free(context.names);
        mylite_result_free(result);
        free(column_name);
        show_like_filter_deinit(&filter);
        return rc;
    }

    free(context.names);
    free(column_name);
    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int plan_show_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_show_create_table *out_plan
) {
    bool missing_schema = false;
    const struct mylite_execution_catalog_builtin_sys_view *selected_builtin_sys_view = NULL;
    const struct mylite_execution_catalog_builtin_sys_view *builtin_sys_view = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_show_create_table){0};
    rc = show_create_statement_targets_selected_builtin_sys_view(
        database,
        statement,
        &selected_builtin_sys_view
    );
    if (rc == MYLITE_OK && selected_builtin_sys_view != NULL) {
        populate_builtin_sys_view_target(&out_plan->target, selected_builtin_sys_view);
        out_plan->is_view = true;
        out_plan->builtin_sys_view = selected_builtin_sys_view;
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_table_name_allow_missing_schema(
            database,
            child_at(statement, 0U),
            &out_plan->target,
            &missing_schema
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->target_was_schema_qualified = show_create_target_was_schema_qualified(statement);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        builtin_sys_view = show_create_target_builtin_sys_view(&out_plan->target);
    }
    if (rc == MYLITE_OK && builtin_sys_view != NULL) {
        out_plan->is_view = true;
        out_plan->builtin_sys_view = builtin_sys_view;
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_metadata_table_reference(
            database,
            &out_plan->target,
            missing_schema,
            &out_plan->table
        );
    }
    if (rc == MYLITE_OK && out_plan->table.kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        out_plan->is_view = true;
        rc = mylite_catalog_read_view_by_table_id(
            database,
            out_plan->table.table_id,
            &out_plan->view
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read view descriptor");
        }
    }
    if (rc == MYLITE_OK && out_plan->is_view) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_unsupported_error(database, "SHOW CREATE TABLE supports only tables and views");
        rc = MYLITE_ERROR;
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
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->indexes,
            &out_plan->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->foreign_keys,
            &out_plan->foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_check_constraint_infos(
            database,
            out_plan->table.table_id,
            &out_plan->check_constraints,
            &out_plan->check_constraint_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_show_create_table_deinit(out_plan);
    }

    return rc;
}

static void planned_show_create_table_deinit(struct planned_show_create_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    loaded_index_infos_deinit(&plan->indexes, &plan->index_count);
    loaded_foreign_key_infos_deinit(&plan->foreign_keys, &plan->foreign_key_count);
    loaded_check_constraint_infos_deinit(&plan->check_constraints, &plan->check_constraint_count);
    *plan = (struct planned_show_create_table){0};
}

static int execute_show_create_table_from_plan(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    mylite_result *result
) {
    static const char *const result_columns[show_create_table_result_column_count] = {
        "Table",
        "Create Table",
    };
    char *create_sql = NULL;
    const char *values[show_create_table_result_column_count] = {NULL, NULL};
    int rc = MYLITE_OK;

    if (plan->builtin_sys_view != NULL) {
        return append_show_create_builtin_sys_view_result(
            database,
            result,
            plan->builtin_sys_view,
            plan->target_was_schema_qualified
        );
    }
    if (plan->is_view) {
        return append_show_create_view_result(database, result, plan->table.name, &plan->view);
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_table_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_create_table_sql(database, plan, &create_sql);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        values[0] = plan->table.name;
        values[1] = create_sql;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(create_sql);
    return rc;
}

static int append_show_create_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const struct mylite_catalog_view_descriptor *view
) {
    return append_show_create_view_text_result(
        database,
        result,
        view_name,
        view->show_create_sql,
        view->character_set_client,
        view->collation_connection
    );
}

static int append_show_create_builtin_sys_view_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_execution_catalog_builtin_sys_view *view,
    bool schema_qualified
) {
    const char *show_create_sql =
        schema_qualified ? view->show_create_qualified_view_sql : view->show_create_view_sql;

    return append_show_create_view_text_result(
        database,
        result,
        view->name,
        show_create_sql,
        "utf8mb4",
        "utf8mb4_0900_ai_ci"
    );
}

static int append_show_create_view_text_result(
    struct mylite_db *database,
    mylite_result *result,
    const char *view_name,
    const char *show_create_sql,
    const char *character_set_client,
    const char *collation_connection
) {
    static const char *const result_columns[show_create_view_result_column_count] = {
        "View",
        "Create View",
        "character_set_client",
        "collation_connection",
    };
    const char *values[show_create_view_result_column_count] = {
        view_name,
        show_create_sql,
        character_set_client,
        collation_connection,
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_view_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static const struct mylite_execution_catalog_builtin_sys_view *find_builtin_sys_view_definition(
    const char *view_name
) {
    return mylite_execution_catalog_builtin_sys_view_definition_by_name(view_name);
}

static const struct mylite_execution_catalog_builtin_sys_view *show_create_target_builtin_sys_view(
    const struct table_name_resolution *target
) {
    if (target == NULL || strcmp(target->schema.name, "sys") != 0) {
        return NULL;
    }
    return find_builtin_sys_view_definition(target->table_name);
}

static bool show_create_target_was_schema_qualified(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *target = statement == NULL ? NULL : child_at(statement, 0U);

    return target != NULL && target->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER;
}

static int show_create_statement_targets_selected_builtin_sys_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct mylite_execution_catalog_builtin_sys_view **out_view
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_view == NULL) {
        return MYLITE_MISUSE;
    }
    *out_view = NULL;
    if (database == NULL || !database->session.has_selected_schema ||
        strcmp(database->session.selected_schema, "sys") != 0) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        statement == NULL ? NULL : child_at(statement, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        *out_view = find_builtin_sys_view_definition(parts[0]);
    }
    return MYLITE_OK;
}

static void populate_builtin_sys_view_target(
    struct table_name_resolution *target,
    const struct mylite_execution_catalog_builtin_sys_view *view
) {
    if (target == NULL || view == NULL) {
        return;
    }

    *target = (struct table_name_resolution){0};
    (void)snprintf(target->schema.name, sizeof(target->schema.name), "%s", "sys");
    (void)snprintf(target->table_name, sizeof(target->table_name), "%s", view->name);
}

static int build_show_create_table_sql(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(
        &string,
        plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY ? "CREATE TEMPORARY TABLE "
                                                                : "CREATE TABLE "
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, plan->table.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (\n");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = append_show_create_table_column_definition(
            database,
            &string,
            &plan->table,
            &plan->columns[column_index],
            (plan->index_count == 0U && plan->foreign_key_count == 0U &&
             plan->check_constraint_count == 0U && column_index + 1U == plan->column_count) != 0
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_indexes(database, &string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_keys(database, &string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_check_constraints(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_table_options(database, &string, plan);
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

static int append_show_create_table_column_definition(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
) {
    const char *type_text = NULL;
    char type_text_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    bool is_national = column_descriptor_is_national_char_or_varchar(column);
    int rc = show_create_table_type_text(
        database,
        column->logical_type,
        type_text_storage,
        sizeof(type_text_storage),
        &type_text
    );

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "  ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, type_text);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_column_charset_collation(string, table, column, is_national);
    }
    if (rc == MYLITE_OK && column->is_generated) {
        rc = mylite_dynamic_string_append(string, " GENERATED ALWAYS AS (");
    }
    if (rc == MYLITE_OK && column->is_generated) {
        rc = mylite_dynamic_string_append(string, column->generation_expression);
    }
    if (rc == MYLITE_OK && column->is_generated) {
        rc = mylite_dynamic_string_append(
            string,
            column->generated_kind == MYLITE_CATALOG_GENERATED_COLUMN_STORED ? ") STORED"
                                                                             : ") VIRTUAL"
        );
    }
    if (rc == MYLITE_OK && !column->is_nullable) {
        rc = mylite_dynamic_string_append(string, " NOT NULL");
    }
    if (rc == MYLITE_OK && !column->is_generated && column->is_nullable &&
        column_descriptor_is_timestamp(column)) {
        rc = mylite_dynamic_string_append(string, " NULL");
    }
    if (rc == MYLITE_OK && !column->is_generated) {
        rc = append_show_create_table_column_default(database, string, column);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_column_suffix(string, column, is_last_column);
    }

    return rc;
}

static int append_show_create_table_column_charset_collation(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    bool is_national
) {
    const bool has_explicit_attribute =
        (column->character_set_name[0] != '\0' || column->collation_name[0] != '\0') != 0;
    const char *effective_collation = NULL;
    int rc = MYLITE_OK;

    if (is_national) {
        rc = mylite_dynamic_string_append(string, " CHARACTER SET ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, national_character_set_name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " COLLATE ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, national_collation_name);
        }
        return rc;
    }
    if (!column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }

    effective_collation = column_effective_collation_name(table, column);
    if (has_explicit_attribute) {
        rc = mylite_dynamic_string_append(string, " CHARACTER SET ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(
                string,
                column_effective_character_set_name(table, column)
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " COLLATE ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, effective_collation);
        }
        return rc;
    }
    if (text_equals_ascii_case_insensitive(
            effective_collation,
            MYLITE_CATALOG_DEFAULT_TABLE_COLLATION
        )) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " COLLATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, effective_collation);
    }
    return rc;
}

static int append_show_create_table_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    char default_text[show_create_integer_default_text_capacity];
    struct binary_string_type_info binary_info_storage = {0};
    const struct binary_string_type_info *binary_info =
        binary_string_type_info_for_logical_type(column->logical_type, &binary_info_storage);
    bool binary_blob_family = (column_descriptor_is_binary_string_family(column) &&
                               binary_info != NULL && binary_info->blob_family) != 0;
    int written = 0;

    if (column->is_auto_increment) {
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE && column->is_nullable &&
        !column_descriptor_is_text_family(column) && !binary_blob_family) {
        return mylite_dynamic_string_append(string, " DEFAULT NULL");
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return mylite_dynamic_string_append(string, " DEFAULT CURRENT_TIMESTAMP");
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return mylite_dynamic_string_append(string, " DEFAULT (curdate())");
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return mylite_dynamic_string_append(string, " DEFAULT (curtime())");
    }
    if (column_default_kind_is_expression(column->default_kind)) {
        return append_show_create_table_expression_default(string, column);
    }
    if (column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        return append_show_create_table_non_integer_column_default(database, string, column);
    }

    written = snprintf(
        default_text,
        sizeof(default_text),
        " DEFAULT '%" PRId64 "'",
        column->default_integer
    );
    if (written < 0 || (size_t)written >= sizeof(default_text)) {
        set_runtime_error(database, "failed to format column default");
        return MYLITE_ERROR;
    }

    return mylite_dynamic_string_append(string, default_text);
}

static int append_show_create_table_non_integer_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_bit(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        int rc = mylite_dynamic_string_append(string, " DEFAULT ");

        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, column->default_text);
        }
        return rc;
    }
    if (column_descriptor_is_text_family(column) &&
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_show_create_table_text_expression_default(string, column);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        if (column_descriptor_is_binary_blob_family(column)) {
            return append_show_create_table_binary_blob_expression_default(
                database,
                string,
                column
            );
        }
        return append_show_create_table_binary_default(database, string, column);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
        column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return append_mysql_quoted_default_text(string, column->default_text);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return append_show_create_table_text_expression_default(string, column);
    }
    return MYLITE_OK;
}

static int append_show_create_table_binary_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " DEFAULT ");
    }
    if (rc == MYLITE_OK) {
        if (binary_default_bytes_use_hex_literal(bytes, byte_count)) {
            rc = append_binary_default_hex_literal(string, bytes, byte_count);
        } else {
            rc = append_mysql_quoted_binary_default_text(string, bytes, byte_count);
        }
    }
    free(bytes);
    return rc;
}

static int append_show_create_table_binary_blob_expression_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = decode_binary_default_text(database, column->default_text, &bytes, &byte_count);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " DEFAULT (");
    }
    if (rc == MYLITE_OK) {
        if (byte_count == 0U) {
            rc = mylite_dynamic_string_append(string, "X''");
        } else {
            rc = append_binary_default_hex_literal(string, bytes, byte_count);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    free(bytes);
    return rc;
}

static bool binary_default_bytes_use_hex_literal(const char *bytes, size_t byte_count) {
    for (size_t index = 0U; index < byte_count; ++index) {
        unsigned char byte = (unsigned char)bytes[index];

        if (byte == '\0' ||
            (byte >= ascii_printable_min_byte && byte <= ascii_printable_max_byte)) {
            continue;
        }
        return true;
    }
    return false;
}

static int append_binary_default_hex_literal(
    struct mylite_dynamic_string *string,
    const char *bytes,
    size_t byte_count
) {
    static const char hex_digits[] = "0123456789ABCDEF";
    int rc = mylite_dynamic_string_append(string, "0x");

    for (size_t byte_index = 0U; rc == MYLITE_OK && byte_index < byte_count; ++byte_index) {
        unsigned char byte = (unsigned char)bytes[byte_index];

        rc = mylite_dynamic_string_append_char(string, hex_digits[byte >> byte_high_nibble_shift]);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, hex_digits[byte & byte_low_nibble_mask]);
        }
    }
    return rc;
}

static int append_mysql_quoted_binary_default_text(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
) {
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        switch (text[index]) {
        case '\0':
            rc = mylite_dynamic_string_append(string, "\\0");
            break;
        case '\'':
            rc = mylite_dynamic_string_append(string, "''");
            break;
        case '\\':
            rc = mylite_dynamic_string_append(string, "\\\\");
            break;
        case '\n':
            rc = mylite_dynamic_string_append(string, "\\n");
            break;
        case '\r':
            rc = mylite_dynamic_string_append(string, "\\r");
            break;
        case '\t':
            rc = mylite_dynamic_string_append(string, "\\t");
            break;
        default:
            rc = mylite_dynamic_string_append_char(string, text[index]);
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static int append_show_create_table_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT (");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, column->default_text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_show_create_table_text_expression_default(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    int rc = mylite_dynamic_string_append(string, " DEFAULT (");

    if (rc == MYLITE_OK) {
        rc = append_mysql_utf8mb4_expression_text(string, column->default_text);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_show_create_table_column_suffix(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
) {
    int rc = MYLITE_OK;

    if (column->is_auto_increment) {
        rc = mylite_dynamic_string_append(string, " AUTO_INCREMENT");
    }
    if (rc == MYLITE_OK && column->on_update_current_timestamp) {
        rc = mylite_dynamic_string_append(string, " ON UPDATE CURRENT_TIMESTAMP");
    }
    if (rc == MYLITE_OK && column->comment[0] != '\0') {
        rc = mylite_dynamic_string_append(string, " COMMENT ");
        if (rc == MYLITE_OK) {
            rc = append_mysql_quoted_text(string, column->comment);
        }
    }
    if (rc == MYLITE_OK && !column->is_visible) {
        rc = mylite_dynamic_string_append(string, " /*!80023 INVISIBLE */");
    }
    if (rc == MYLITE_OK && !is_last_column) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\n');
    }

    return rc;
}

static int append_show_create_table_indexes(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    size_t appended = 0U;
    int rc = MYLITE_OK;

    for (int group = index_display_group_primary;
         rc == MYLITE_OK && group <= index_display_group_fulltext;
         ++group) {
        for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
            if (index_display_group(&plan->indexes[index]) != group) {
                continue;
            }

            ++appended;
            const bool is_last_index = appended == plan->index_count;
            const bool has_no_foreign_keys = plan->foreign_key_count == 0U;
            const bool has_no_check_constraints = plan->check_constraint_count == 0U;
            bool is_last_definition = false;

            if (is_last_index && has_no_foreign_keys && has_no_check_constraints) {
                is_last_definition = true;
            }

            rc = append_show_create_table_index(
                database,
                string,
                &plan->indexes[index],
                is_last_definition
            );
        }
    }

    return rc;
}

static int append_show_create_table_foreign_keys(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    (void)database;
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->foreign_key_count; ++index) {
        bool is_last_constraint = false;

        if (index + 1U == plan->foreign_key_count && plan->check_constraint_count == 0U) {
            is_last_constraint = true;
        }
        rc = append_show_create_table_foreign_key(
            string,
            &plan->foreign_keys[index],
            is_last_constraint
        );
    }

    return rc;
}

static int append_show_create_table_check_constraints(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->check_constraint_count; ++index) {
        const struct mylite_catalog_check_constraint_descriptor *check_constraint =
            &plan->check_constraints[index].check_constraint;

        rc = mylite_dynamic_string_append(string, "  CONSTRAINT ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(
                string,
                check_constraint->name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " CHECK (");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, check_constraint->check_clause);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
        if (rc == MYLITE_OK && !check_constraint->is_enforced) {
            rc = mylite_dynamic_string_append(string, " /*!80016 NOT ENFORCED */");
        }
        if (rc == MYLITE_OK && index + 1U < plan->check_constraint_count) {
            rc = mylite_dynamic_string_append_char(string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '\n');
        }
    }

    return rc;
}

static int append_show_create_table_table_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = mylite_dynamic_string_append(string, ") ENGINE=InnoDB");

    if (rc == MYLITE_OK) {
        rc = append_show_create_table_auto_increment_option(database, string, plan);
    }
    if (rc == MYLITE_OK && charset_name_is_binary(plan->table.default_charset) &&
        collation_name_is_binary(plan->table.default_collation)) {
        rc = mylite_dynamic_string_append(string, " DEFAULT CHARSET=binary");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " DEFAULT CHARSET=");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, plan->table.default_charset);
        }
        if (rc == MYLITE_OK && show_create_table_should_append_default_collation(plan)) {
            rc = mylite_dynamic_string_append(string, " COLLATE=");
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append(string, plan->table.default_collation);
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_comment_option(string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_storage_statistics_options(database, string, plan);
    }

    return rc;
}

static bool show_create_table_should_append_default_collation(
    const struct planned_show_create_table *plan
) {
    if (plan == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(plan->table.default_charset, "ascii") &&
        text_equals_ascii_case_insensitive(plan->table.default_collation, "ascii_general_ci")) {
        return false;
    }
    return true;
}

static int append_show_create_table_auto_increment_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    char auto_increment_text[integer_text_capacity + sizeof(" AUTO_INCREMENT=")];
    int written = 0;

    if (!show_create_table_has_auto_increment(plan) || plan->table.auto_increment_next <= 1) {
        return MYLITE_OK;
    }

    written = snprintf(
        auto_increment_text,
        sizeof(auto_increment_text),
        " AUTO_INCREMENT=%" PRId64,
        plan->table.auto_increment_next
    );
    if (written < 0 || (size_t)written >= sizeof(auto_increment_text)) {
        set_runtime_error(database, "failed to format auto-increment table option");
        return MYLITE_ERROR;
    }

    return mylite_dynamic_string_append(string, auto_increment_text);
}

static bool show_create_table_has_auto_increment(const struct planned_show_create_table *plan) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (column_descriptor_is_auto_increment(&plan->columns[column_index])) {
            return true;
        }
    }

    return false;
}

static int append_show_create_table_comment_option(
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL || plan->table.comment[0] == '\0') {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " COMMENT=");
    if (rc == MYLITE_OK) {
        rc = append_mysql_quoted_text(string, plan->table.comment);
    }

    return rc;
}

static int append_show_create_table_storage_statistics_options(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_show_create_table *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_OK;
    }
    if (plan->table.pack_keys != -1) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " PACK_KEYS=",
            plan->table.pack_keys
        );
    }
    if (rc == MYLITE_OK && plan->table.stats_persistent != -1) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " STATS_PERSISTENT=",
            plan->table.stats_persistent
        );
    }
    if (rc == MYLITE_OK && plan->table.stats_auto_recalc != -1) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " STATS_AUTO_RECALC=",
            plan->table.stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK && plan->table.stats_sample_pages > 0) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " STATS_SAMPLE_PAGES=",
            plan->table.stats_sample_pages
        );
    }
    if (rc == MYLITE_OK && plan->table.checksum != 0) {
        rc = mylite_dynamic_string_append(string, " CHECKSUM=1");
    }
    if (rc == MYLITE_OK && plan->table.row_format_option[0] != '\0') {
        rc = mylite_dynamic_string_append(string, " ROW_FORMAT=");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, plan->table.row_format_option);
        }
    }
    if (rc == MYLITE_OK && plan->table.key_block_size > 0) {
        rc = append_show_create_table_integer_option(
            database,
            string,
            " KEY_BLOCK_SIZE=",
            plan->table.key_block_size
        );
    }

    return rc;
}

static int append_show_create_table_integer_option(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const char *prefix,
    int64_t value
) {
    char text[integer_text_capacity + sizeof(" STATS_SAMPLE_PAGES=")];
    int written = snprintf(text, sizeof(text), "%s%" PRId64, prefix, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        set_runtime_error(database, "failed to format table option value");
        return MYLITE_ERROR;
    }
    return mylite_dynamic_string_append(string, text);
}

static int append_show_create_table_index(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index,
    bool is_last_index
) {
    int rc = MYLITE_OK;

    if (index == NULL) {
        set_runtime_error(database, "invalid index descriptor");
        return MYLITE_ERROR;
    }

    rc = append_show_create_table_index_header(database, string, index);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        rc = append_show_create_table_index_part(
            string,
            &index->parts[part_index],
            part_index != 0U
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_index_options(string, index);
    }
    if (rc == MYLITE_OK && !is_last_index) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\n');
    }

    return rc;
}

static int append_show_create_table_index_header(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index
) {
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        return mylite_dynamic_string_append(string, "  PRIMARY KEY (");
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        const char *prefix = "  KEY ";
        if (index->index.is_unique) {
            prefix = "  UNIQUE KEY ";
        }
        rc = mylite_dynamic_string_append(string, prefix);
    } else if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        rc = mylite_dynamic_string_append(string, "  FULLTEXT KEY ");
    } else if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        rc = mylite_dynamic_string_append(string, "  SPATIAL KEY ");
    } else {
        set_runtime_error(database, "invalid index descriptor");
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, index->index.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }

    return rc;
}

static int append_show_create_table_index_options(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index
) {
    int rc = MYLITE_OK;

    if (index->index.show_create_explicit_btree) {
        rc = mylite_dynamic_string_append(string, " USING BTREE");
    }
    if (rc == MYLITE_OK && index->index.comment[0] != '\0') {
        rc = mylite_dynamic_string_append(string, " COMMENT ");
        if (rc == MYLITE_OK) {
            rc = append_mysql_quoted_text(string, index->index.comment);
        }
    }
    if (rc == MYLITE_OK && index->index.kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY &&
        !index->index.is_visible) {
        rc = mylite_dynamic_string_append(string, " /*!80000 INVISIBLE */");
    }

    return rc;
}

static int append_show_create_table_foreign_key(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool is_last_foreign_key
) {
    int rc = MYLITE_OK;

    if (foreign_key == NULL || foreign_key->part_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(string, "  CONSTRAINT ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            foreign_key->foreign_key.name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FOREIGN KEY (");
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_parts(string, foreign_key, true);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") REFERENCES ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            foreign_key->parent_table.name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_parts(string, foreign_key, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_rule(
            string,
            "DELETE",
            foreign_key->foreign_key.delete_rule
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_foreign_key_rule(
            string,
            "UPDATE",
            foreign_key->foreign_key.update_rule
        );
    }
    if (rc == MYLITE_OK && !is_last_foreign_key) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\n');
    }

    return rc;
}

static int append_show_create_table_foreign_key_parts(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool child_columns
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const char *column_name = foreign_key->parts[part_index].parent_column.name;

        if (child_columns) {
            column_name = foreign_key->parts[part_index].child_column.name;
        }

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, column_name);
        }
    }
    return rc;
}

static int append_show_create_table_foreign_key_rule(
    struct mylite_dynamic_string *string,
    const char *timing,
    const char *rule
) {
    int rc = MYLITE_OK;

    if (strcmp(rule, "NO ACTION") == 0) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " ON ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, timing);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, rule);
    }
    return rc;
}

static int append_show_create_table_index_part(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    bool needs_comma
) {
    int rc = MYLITE_OK;

    if (needs_comma) {
        rc = mylite_dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, part->column.name);
    }
    if (rc == MYLITE_OK && part->index_column.has_prefix_length) {
        char prefix_text[integer_text_capacity];
        int written = snprintf(
            prefix_text,
            sizeof(prefix_text),
            "%" PRId64,
            part->index_column.prefix_length
        );

        if (written < 0 || (size_t)written >= sizeof(prefix_text)) {
            return MYLITE_NOMEM;
        }
        rc = mylite_dynamic_string_append_char(string, '(');
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, prefix_text);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, ')');
        }
    }
    if (rc == MYLITE_OK &&
        part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        rc = mylite_dynamic_string_append(string, " DESC");
    }

    return rc;
}

static int index_display_group(const struct loaded_index_info *index) {
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        return index_display_group_primary;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY && index->index.is_unique &&
        index->part_count > 0U && !index->parts[0].column.is_nullable) {
        return index_display_group_not_null_unique;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY && index->index.is_unique) {
        return index_display_group_nullable_unique;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        return index_display_group_secondary;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return index_display_group_fulltext;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return index_display_group_fulltext;
    }

    return index_display_group_unknown;
}

static int show_create_table_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text
) {
    return show_descriptor_type_text(
        database,
        logical_type,
        buffer,
        buffer_size,
        "SHOW CREATE TABLE supports only integer, BIT, string, binary string, decimal, "
        "approximate numeric, YEAR, DATE, TIME, DATETIME, and TIMESTAMP column descriptors",
        out_type_text
    );
}

static int show_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    const char *integer_text = NULL;
    bool matched = false;
    int rc = MYLITE_OK;

    if (logical_type == NULL || buffer == NULL || unsupported_message == NULL ||
        out_type_text == NULL) {
        set_runtime_error(database, "invalid column descriptor");
        return MYLITE_ERROR;
    }
    if (logical_type_is_char(logical_type)) {
        return format_char_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (logical_type_is_varchar(logical_type)) {
        return format_varchar_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (format_text_family_type_text(logical_type, out_type_text) == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "JSON") == 0) {
        *out_type_text = "json";
        return MYLITE_OK;
    }
    if (spatial_logical_type_display_text(logical_type, out_type_text)) {
        return MYLITE_OK;
    }
    if (strncmp(logical_type, "ENUM(", enum_logical_prefix_length) == 0) {
        return format_enum_descriptor_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (strncmp(logical_type, "SET(", set_logical_prefix_length) == 0) {
        return format_set_descriptor_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        );
    }
    if (format_binary_string_type_text(
            database,
            logical_type,
            buffer,
            buffer_size,
            unsupported_message,
            out_type_text
        ) == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (strncmp(logical_type, "BIT(", bit_logical_prefix_length) == 0) {
        uint64_t width = 0U;
        int written = 0;

        rc = bit_width_for_logical_type(logical_type, &width);
        if (rc != MYLITE_OK) {
            set_unsupported_error(database, unsupported_message);
            return MYLITE_ERROR;
        }
        written = snprintf(buffer, buffer_size, "bit(%" PRIu64 ")", width);
        if (written < 0 || (size_t)written >= buffer_size) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        *out_type_text = buffer;
        return MYLITE_OK;
    }
    rc = show_descriptor_numeric_type_text(
        database,
        logical_type,
        buffer,
        buffer_size,
        out_type_text,
        &matched
    );
    if (rc != MYLITE_OK || matched) {
        return rc;
    }
    if (show_descriptor_temporal_type_text(logical_type, out_type_text)) {
        return MYLITE_OK;
    }

    integer_text = integer_descriptor_display_text(logical_type);
    if (integer_text != NULL) {
        *out_type_text = integer_text;
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static bool show_descriptor_temporal_type_text(
    const char *logical_type,
    const char **out_type_text
) {
    static const struct {
        const char *logical_type;
        const char *display_type;
    } temporal_types[] = {
        {"DATE", "date"},
        {"TIME", "time"},
        {"DATETIME", "datetime"},
        {"TIMESTAMP", "timestamp"},
        {"YEAR", "year"},
    };

    for (size_t index = 0U; index < sizeof(temporal_types) / sizeof(temporal_types[0]); ++index) {
        if (strcmp(logical_type, temporal_types[index].logical_type) == 0) {
            *out_type_text = temporal_types[index].display_type;
            return true;
        }
    }

    return false;
}

static bool spatial_logical_type_display_text(
    const char *logical_type,
    const char **out_type_text
) {
    static const struct {
        const char *logical_type;
        const char *display_type;
    } spatial_types[] = {
        {"geometry", "geometry"},
        {"point", "point"},
        {"linestring", "linestring"},
        {"polygon", "polygon"},
        {"multipoint", "multipoint"},
        {"multilinestring", "multilinestring"},
        {"multipolygon", "multipolygon"},
        {"geomcollection", "geomcollection"},
    };

    if (logical_type == NULL || out_type_text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(spatial_types) / sizeof(spatial_types[0]); ++index) {
        if (strcmp(logical_type, spatial_types[index].logical_type) == 0) {
            *out_type_text = spatial_types[index].display_type;
            return true;
        }
    }
    return false;
}

static int show_descriptor_numeric_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text,
    bool *out_matched
) {
    struct decimal_type_info decimal_info = {0};
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };

    if (out_matched == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = false;

    if (decimal_type_info_for_logical_type(logical_type, &decimal_info) == MYLITE_OK) {
        *out_matched = true;
        return format_decimal_descriptor_type_text(
            database,
            buffer,
            buffer_size,
            &decimal_info,
            out_type_text
        );
    }
    if (approximate_type_info_for_logical_type(logical_type, &approximate_info) == MYLITE_OK) {
        *out_matched = true;
        return format_approximate_descriptor_type_text(
            database,
            buffer,
            buffer_size,
            &approximate_info,
            out_type_text
        );
    }

    return MYLITE_OK;
}

static int format_decimal_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct decimal_type_info *info,
    const char **out_type_text
) {
    int written = snprintf(
        buffer,
        buffer_size,
        "decimal(%" PRIu64 ",%" PRIu64 ")%s",
        info->precision,
        info->scale,
        info->is_unsigned ? " unsigned" : ""
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_approximate_descriptor_type_text(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const struct approximate_type_info *info,
    const char **out_type_text
) {
    const char *base_type = info->type_class == APPROXIMATE_TYPE_FLOAT ? "float" : "double";
    int written = 0;

    if (!info->is_unsigned) {
        *out_type_text = base_type;
        return MYLITE_OK;
    }

    written = snprintf(buffer, buffer_size, "%s unsigned", base_type);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_varchar_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    size_t length = 0U;
    int written = 0;
    int rc = parse_varchar_descriptor_length(database, logical_type, unsupported_message, &length);

    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(buffer, buffer_size, "varchar(%zu)", length);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_char_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    size_t length = 0U;
    int written = 0;
    int rc = parse_char_descriptor_length(database, logical_type, unsupported_message, &length);

    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(buffer, buffer_size, "char(%zu)", length);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_enum_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    struct enum_type_info info = {0};
    int rc = enum_type_info_for_logical_type(database, logical_type, unsupported_message, &info);
    int written = 0;

    if (rc != MYLITE_OK) {
        return rc;
    }
    written =
        snprintf(buffer, buffer_size, "enum%s", logical_type + enum_logical_prefix_length - 1U);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_set_descriptor_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    struct set_type_info info = {0};
    int rc = set_type_info_for_logical_type(database, logical_type, unsupported_message, &info);
    int written = 0;

    if (rc != MYLITE_OK) {
        return rc;
    }
    written = snprintf(buffer, buffer_size, "set%s", logical_type + set_logical_prefix_length - 1U);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static int format_text_family_type_text(const char *logical_type, const char **out_type_text) {
    const struct text_family_type_info *info = text_family_type_info_for_logical_type(logical_type);

    if (info == NULL || out_type_text == NULL) {
        return MYLITE_ERROR;
    }
    *out_type_text = info->display_type;
    return MYLITE_OK;
}

static int format_binary_string_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char *unsupported_message,
    const char **out_type_text
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info =
        binary_string_type_info_for_logical_type(logical_type, &storage);
    size_t length = 0U;
    int written = 0;

    if (info == NULL || out_type_text == NULL) {
        return MYLITE_ERROR;
    }
    if (strncmp(logical_type, "BINARY(", binary_logical_prefix_length) == 0) {
        int rc =
            parse_binary_descriptor_length(database, logical_type, unsupported_message, &length);

        if (rc != MYLITE_OK) {
            return rc;
        }
        written = snprintf(buffer, buffer_size, "binary(%zu)", length);
    } else if (strncmp(logical_type, "VARBINARY(", varbinary_logical_prefix_length) == 0) {
        int rc =
            parse_varbinary_descriptor_length(database, logical_type, unsupported_message, &length);

        if (rc != MYLITE_OK) {
            return rc;
        }
        written = snprintf(buffer, buffer_size, "varbinary(%zu)", length);
    } else {
        *out_type_text = info->display_type;
        return info->display_type == NULL ? MYLITE_ERROR : MYLITE_OK;
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format column type");
        return MYLITE_ERROR;
    }
    *out_type_text = buffer;
    return MYLITE_OK;
}

static const char *integer_descriptor_display_text(const char *logical_type) {
    static const struct {
        const char *logical_type;
        const char *display_type;
    } renderings[] = {
        {"INT", "int"},
        {"INTEGER", "int"},
        {"TINYINT", "tinyint"},
        {"TINYINT(1)", "tinyint(1)"},
        {"SMALLINT", "smallint"},
        {"MEDIUMINT", "mediumint"},
        {"INT UNSIGNED", "int unsigned"},
        {"INTEGER UNSIGNED", "int unsigned"},
        {"TINYINT UNSIGNED", "tinyint unsigned"},
        {"SMALLINT UNSIGNED", "smallint unsigned"},
        {"MEDIUMINT UNSIGNED", "mediumint unsigned"},
        {"BIGINT", "bigint"},
        {"BIGINT UNSIGNED", "bigint unsigned"},
    };

    if (logical_type == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < sizeof(renderings) / sizeof(renderings[0]); ++index) {
        if (strcmp(logical_type, renderings[index].logical_type) == 0) {
            return renderings[index].display_type;
        }
    }

    return NULL;
}

static int build_show_create_database_sql(
    const struct mylite_catalog_schema_descriptor *schema,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE DATABASE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, schema->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " /*!40100 DEFAULT CHARACTER SET ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, schema->default_charset);
    }
    if (rc == MYLITE_OK && !charset_name_is_binary(schema->default_charset)) {
        rc = mylite_dynamic_string_append(&string, " COLLATE ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, schema->default_collation);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " */ /*!80016 DEFAULT ENCRYPTION='N' */");
    }
    if (rc == MYLITE_OK) {
        *out_sql = string.text;
        return MYLITE_OK;
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}
