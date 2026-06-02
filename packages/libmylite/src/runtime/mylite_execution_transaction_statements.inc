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
        rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
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
        rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
        if (rc == MYLITE_OK) {
            database->session.user_transaction_active = false;
            clear_active_transaction_characteristics(database);
            clear_user_savepoints(database);
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
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
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
