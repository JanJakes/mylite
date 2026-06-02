static int begin_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    int rc = MYLITE_OK;

    transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
    transaction->active = false;
    if (database->session.user_transaction_active) {
        rc = normalize_sqlite_control_rc(
            database,
            execute_sqlite_control_sql(database, "SAVEPOINT _mylite_statement")
        );
        if (rc == MYLITE_OK) {
            transaction->kind = MYLITE_STATEMENT_TRANSACTION_SAVEPOINT;
            transaction->active = true;
        }
        return rc;
    }

    rc = normalize_sqlite_control_rc(
        database,
        execute_sqlite_control_sql(database, "BEGIN IMMEDIATE")
    );
    if (rc == MYLITE_OK) {
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_DIRECT;
        transaction->active = true;
    }
    return rc;
}

static int commit_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    int rc = MYLITE_OK;

    if (!transaction->active) {
        return MYLITE_OK;
    }

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        rc = normalize_sqlite_control_rc(
            database,
            execute_sqlite_control_sql(database, "RELEASE SAVEPOINT _mylite_statement")
        );
    } else {
        rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, "COMMIT"));
    }
    if (rc == MYLITE_OK) {
        transaction->active = false;
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
    }
    return rc;
}

static void rollback_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    if (!transaction->active) {
        return;
    }

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK TO SAVEPOINT _mylite_statement");
        (void)execute_sqlite_control_sql(database, "RELEASE SAVEPOINT _mylite_statement");
    } else {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    transaction->active = false;
    transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
}

static int normalize_sqlite_control_rc(struct mylite_db *database, int rc) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    set_physical_sqlite_error(database);
    return MYLITE_ERROR;
}
