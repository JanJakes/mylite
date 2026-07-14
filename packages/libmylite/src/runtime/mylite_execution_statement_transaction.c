#include "mylite_execution_statement_transaction.h"

#include "mylite_connection.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_sqlite_internal.h"

#include <mylite/mylite.h>

int mylite_execution_begin_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    int rc = MYLITE_OK;

    transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
    transaction->active = false;
    if (database->session.user_transaction_active) {
        rc = mylite_execution_normalize_sqlite_control_rc(
            database,
            mylite_execution_execute_sqlite_control_sql(database, "SAVEPOINT _mylite_statement")
        );
        if (rc == MYLITE_OK) {
            transaction->kind = MYLITE_STATEMENT_TRANSACTION_SAVEPOINT;
            transaction->active = true;
        }
        return rc;
    }
    if (!database->session.autocommit_enabled) {
        rc = mylite_execution_begin_autocommit_disabled_transaction(database);
        if (rc == MYLITE_OK) {
            rc = mylite_execution_normalize_sqlite_control_rc(
                database,
                mylite_execution_execute_sqlite_control_sql(database, "SAVEPOINT _mylite_statement")
            );
        }
        if (rc == MYLITE_OK) {
            transaction->kind = MYLITE_STATEMENT_TRANSACTION_SAVEPOINT;
            transaction->active = true;
        }
        return rc;
    }

    rc = mylite_execution_normalize_sqlite_control_rc(
        database,
        mylite_execution_execute_sqlite_control_sql(database, "BEGIN IMMEDIATE")
    );
    if (rc == MYLITE_OK) {
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_DIRECT;
        transaction->active = true;
    }
    return rc;
}

int mylite_execution_begin_read_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    int rc = MYLITE_OK;

    transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
    transaction->active = false;
    if (database->session.user_transaction_active || sqlite3_get_autocommit(database->sqlite) == 0) {
        return MYLITE_OK;
    }
    if (!database->session.autocommit_enabled) {
        return mylite_execution_begin_autocommit_disabled_transaction(database);
    }

    rc = mylite_execution_normalize_sqlite_control_rc(
        database,
        mylite_execution_execute_sqlite_control_sql(database, "BEGIN")
    );
    if (rc == MYLITE_OK) {
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_DIRECT;
        transaction->active = true;
    }
    return rc;
}

int mylite_execution_begin_autocommit_disabled_transaction(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (database->session.user_transaction_active || database->session.autocommit_enabled) {
        return MYLITE_OK;
    }

    rc = mylite_execution_normalize_sqlite_control_rc(
        database,
        mylite_execution_execute_sqlite_control_sql(database, "BEGIN IMMEDIATE")
    );
    if (rc == MYLITE_OK) {
        database->session.user_transaction_active = true;
        database->session.active_transaction_isolation =
            database->session.has_next_transaction_isolation
                ? database->session.next_transaction_isolation
                : database->session.session_transaction_isolation;
        database->session.active_transaction_read_only =
            database->session.has_next_transaction_access_mode
                ? database->session.next_transaction_access_mode ==
                      MYLITE_TRANSACTION_ACCESS_READ_ONLY
                : database->session.session_transaction_access_mode ==
                      MYLITE_TRANSACTION_ACCESS_READ_ONLY;
        database->session.has_next_transaction_isolation = false;
        database->session.has_next_transaction_access_mode = false;
        database->session.next_transaction_isolation_from_system_variable = false;
        database->session.next_transaction_access_mode_from_system_variable = false;
    }
    return rc;
}

int mylite_execution_commit_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    int rc = MYLITE_OK;

    if (!transaction->active) {
        return MYLITE_OK;
    }

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        rc = mylite_execution_normalize_sqlite_control_rc(
            database,
            mylite_execution_execute_sqlite_control_sql(
                database,
                "RELEASE SAVEPOINT _mylite_statement"
            )
        );
    } else {
        rc = mylite_execution_normalize_sqlite_control_rc(
            database,
            mylite_execution_execute_sqlite_control_sql(database, "COMMIT")
        );
    }
    if (rc == MYLITE_OK) {
        transaction->active = false;
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
    }
    return rc;
}

void mylite_execution_rollback_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction
) {
    if (!transaction->active) {
        return;
    }

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        (void)mylite_execution_execute_sqlite_control_sql(
            database,
            "ROLLBACK TO SAVEPOINT _mylite_statement"
        );
        (void)mylite_execution_execute_sqlite_control_sql(
            database,
            "RELEASE SAVEPOINT _mylite_statement"
        );
    } else {
        (void)mylite_execution_execute_sqlite_control_sql(database, "ROLLBACK");
    }
    transaction->active = false;
    transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
}

int mylite_execution_normalize_sqlite_control_rc(struct mylite_db *database, int rc) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    mylite_execution_diagnostics_set_physical_sqlite_error(database);
    return MYLITE_ERROR;
}
