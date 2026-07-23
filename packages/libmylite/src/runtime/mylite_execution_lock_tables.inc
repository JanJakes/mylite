static int execute_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_lock_tables plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = commit_active_user_transaction_for_ddl(database);
    if (rc == MYLITE_OK) {
        clear_session_table_locks(database);
        rc = plan_lock_tables_statement(database, statement, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_lock_tables_plan(database, &plan);
    }
    planned_lock_tables_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_unlock_tables_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (database->session.table_lock_count != 0U && database->session.user_transaction_active) {
        rc = commit_active_user_transaction_for_ddl(database);
    }
    if (rc == MYLITE_OK) {
        clear_session_table_locks(database);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int plan_lock_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_lock_tables *out_plan
) {
    const struct mylite_sql_ast_node *target_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *target_node = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_lock_tables){0};
    if (target_list == NULL || target_list->kind != MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->lock_count = mylite_sql_ast_node_child_count(target_list);
    if (out_plan->lock_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->lock_count > SIZE_MAX / sizeof(*out_plan->locks)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->locks = calloc(out_plan->lock_count, sizeof(*out_plan->locks));
    if (out_plan->locks == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    target_node = child_at(target_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < out_plan->lock_count;
         ++target_index) {
        rc = plan_lock_table_target(database, target_node, out_plan, target_index);
        if (target_node != NULL) {
            target_node = target_node->next_sibling;
        }
    }
    if (rc != MYLITE_OK) {
        planned_lock_tables_deinit(out_plan);
    }
    return rc;
}

static int plan_lock_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_lock_tables *plan,
    size_t target_index
) {
    struct mylite_session_table_lock *lock = &plan->locks[target_index];
    struct table_name_resolution resolution = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const size_t child_count = mylite_sql_ast_node_child_count(target_node);
    const struct mylite_sql_ast_node *table_node = child_at(target_node, 0U);
    const struct mylite_sql_ast_node *alias_node = NULL;
    const struct mylite_sql_ast_node *mode_node = NULL;
    int rc = MYLITE_OK;

    if (target_node == NULL || target_node->kind != MYLITE_SQL_AST_LOCK_TABLE_TARGET ||
        child_count < 2U || child_count > 3U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    alias_node = child_count == 3U ? child_at(target_node, 1U) : NULL;
    mode_node = child_at(target_node, child_count - 1U);

    rc =
        mylite_execution_transaction_resolve_lock_target(database, table_node, &resolution, &table);
    if (rc != MYLITE_OK) {
        return rc;
    }

    memcpy(lock->schema_name, resolution.schema.name, sizeof(lock->schema_name));
    memcpy(lock->table_name, resolution.table_name, sizeof(lock->table_name));
    lock->is_temporary = table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY;
    rc = copy_lock_table_alias(database, alias_node, lock);
    if (rc == MYLITE_OK) {
        rc = set_lock_table_mode(database, mode_node, lock);
    }
    if (rc == MYLITE_OK) {
        rc = check_lock_table_duplicate_targets(database, plan, target_index);
    }
    return rc;
}

static int copy_lock_table_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias_node,
    struct mylite_session_table_lock *out_lock
) {
    int rc = MYLITE_OK;

    if (alias_node == NULL) {
        memcpy(out_lock->effective_name, out_lock->table_name, sizeof(out_lock->effective_name));
        return MYLITE_OK;
    }

    rc = copy_identifier_text(alias_node, out_lock->alias, sizeof(out_lock->alias), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_lock->has_alias = true;
    memcpy(out_lock->effective_name, out_lock->alias, sizeof(out_lock->effective_name));
    return MYLITE_OK;
}

static int set_lock_table_mode(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *mode_node,
    struct mylite_session_table_lock *out_lock
) {
    if (mode_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (mode_node->kind) {
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCK:
        out_lock->mode = MYLITE_SESSION_TABLE_LOCK_READ;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LOCK_TABLE_READ_LOCAL_LOCK:
        out_lock->mode = MYLITE_SESSION_TABLE_LOCK_READ_LOCAL;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LOCK_TABLE_WRITE_LOCK:
        out_lock->mode = MYLITE_SESSION_TABLE_LOCK_WRITE;
        return MYLITE_OK;
    default:
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
}

static int check_lock_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_lock_tables *plan,
    size_t target_index
) {
    const struct mylite_session_table_lock *target = &plan->locks[target_index];

    for (size_t previous_index = 0U; previous_index < target_index; ++previous_index) {
        const struct mylite_session_table_lock *previous = &plan->locks[previous_index];

        if (strcmp(previous->effective_name, target->effective_name) == 0) {
            set_duplicate_table_alias_error(database, target->effective_name);
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int apply_lock_tables_plan(struct mylite_db *database, struct planned_lock_tables *plan) {
    free(database->session.table_locks);
    database->session.table_locks = plan->locks;
    database->session.table_lock_count = plan->lock_count;
    database->session.table_lock_capacity = plan->lock_count;
    plan->locks = NULL;
    plan->lock_count = 0U;
    return MYLITE_OK;
}

static void planned_lock_tables_deinit(struct planned_lock_tables *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->locks);
    plan->locks = NULL;
    plan->lock_count = 0U;
}

static void clear_session_table_locks(struct mylite_db *database) {
    if (database == NULL || database->session.table_locks == NULL) {
        return;
    }

    memset(
        database->session.table_locks,
        0,
        database->session.table_lock_count * sizeof(*database->session.table_locks)
    );
    database->session.table_lock_count = 0U;
}
