#include "mylite_execution_statement_transaction.h"

#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_mysql_error_codes.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

struct transaction_failure_context {
    const char *primary_sqlite_error;
    const char *failure_kind;
    const char *operation;
    const char *failure_error;
    int emergency_rc;
};

static void mark_transaction_state_uncertain(
    struct mylite_db *database,
    const struct transaction_failure_context *failure
);

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
            mylite_execution_execute_cached_sqlite_control_sql(
                database,
                "SAVEPOINT _mylite_statement"
            )
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
                mylite_execution_execute_cached_sqlite_control_sql(
                    database,
                    "SAVEPOINT _mylite_statement"
                )
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
        mylite_execution_execute_cached_sqlite_control_sql(database, "BEGIN IMMEDIATE")
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
    if (database->session.user_transaction_active ||
        sqlite3_get_autocommit(database->sqlite) == 0) {
        return mylite_catalog_synchronize_snapshot(database);
    }
    if (!database->session.autocommit_enabled) {
        rc = mylite_execution_begin_autocommit_disabled_transaction(database);
        return rc == MYLITE_OK ? mylite_catalog_synchronize_snapshot(database) : rc;
    }

    rc = mylite_execution_normalize_sqlite_control_rc(
        database,
        mylite_execution_execute_cached_sqlite_control_sql(database, "BEGIN")
    );
    if (rc == MYLITE_OK) {
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_DIRECT;
        transaction->active = true;
        rc = mylite_catalog_synchronize_snapshot(database);
    }
    return rc;
}

int mylite_execution_begin_autocommit_disabled_transaction(struct mylite_db *database) {
    bool read_only = false;
    int rc = MYLITE_OK;

    if (database->session.user_transaction_active || database->session.autocommit_enabled) {
        return MYLITE_OK;
    }

    read_only =
        database->session.has_next_transaction_access_mode
            ? database->session.next_transaction_access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY
            : database->session.session_transaction_access_mode ==
                  MYLITE_TRANSACTION_ACCESS_READ_ONLY;
    rc = mylite_execution_normalize_sqlite_control_rc(
        database,
        mylite_execution_execute_cached_sqlite_control_sql(
            database,
            read_only ? "BEGIN" : "BEGIN IMMEDIATE"
        )
    );
    if (rc == MYLITE_OK) {
        database->session.user_transaction_active = true;
        database->session.active_transaction_isolation =
            database->session.has_next_transaction_isolation
                ? database->session.next_transaction_isolation
                : database->session.session_transaction_isolation;
        database->session.active_transaction_read_only = read_only;
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
    char commit_error[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *failed_operation = "COMMIT";
    int emergency_rc = MYLITE_OK;
    int rc = MYLITE_OK;

    if (!transaction->active) {
        return MYLITE_OK;
    }

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        failed_operation = "RELEASE SAVEPOINT";
        rc = mylite_execution_normalize_sqlite_control_rc(
            database,
            mylite_execution_execute_cached_sqlite_control_sql(
                database,
                "RELEASE SAVEPOINT _mylite_statement"
            )
        );
    } else {
        rc = mylite_execution_normalize_sqlite_control_rc(
            database,
            mylite_execution_execute_cached_sqlite_control_sql(database, "COMMIT")
        );
    }
    if (rc == MYLITE_OK) {
        transaction->active = false;
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
        return MYLITE_OK;
    }

    (void)snprintf(commit_error, sizeof(commit_error), "%s", sqlite3_errmsg(database->sqlite));
    if (sqlite3_get_autocommit(database->sqlite) == 0) {
        emergency_rc = mylite_execution_execute_cached_sqlite_control_sql(database, "ROLLBACK");
    }
    if (sqlite3_get_autocommit(database->sqlite) != 0) {
        transaction->active = false;
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
        database->session.user_transaction_active = false;
        database->session.active_transaction_read_only = false;
        database->session.active_transaction_isolation =
            MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    }
    mylite_catalog_invalidate_descriptor_cache(database);
    mark_transaction_state_uncertain(
        database,
        &(const struct transaction_failure_context){
            .primary_sqlite_error = commit_error,
            .failure_kind = "completion",
            .operation = failed_operation,
            .failure_error = commit_error,
            .emergency_rc = emergency_rc,
        }
    );
    return rc;
}

int mylite_execution_rollback_statement_transaction(
    struct mylite_db *database,
    struct mylite_statement_transaction *transaction,
    int primary_rc
) {
    char cleanup_error[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char primary_sqlite_error[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *failed_operation = "ROLLBACK";
    int cleanup_rc = MYLITE_OK;
    int emergency_rc = MYLITE_OK;

    if (!transaction->active) {
        return primary_rc;
    }
    (void)snprintf(
        primary_sqlite_error,
        sizeof(primary_sqlite_error),
        "%s",
        sqlite3_errmsg(database->sqlite)
    );

    if (transaction->kind == MYLITE_STATEMENT_TRANSACTION_SAVEPOINT) {
        failed_operation = "ROLLBACK TO SAVEPOINT";
        cleanup_rc = mylite_execution_execute_cached_sqlite_control_sql(
            database,
            "ROLLBACK TO SAVEPOINT _mylite_statement"
        );
        if (cleanup_rc == MYLITE_OK) {
            failed_operation = "RELEASE SAVEPOINT";
            cleanup_rc = mylite_execution_execute_cached_sqlite_control_sql(
                database,
                "RELEASE SAVEPOINT _mylite_statement"
            );
        }
    } else {
        cleanup_rc = mylite_execution_execute_cached_sqlite_control_sql(database, "ROLLBACK");
    }
    mylite_catalog_invalidate_descriptor_cache(database);
    if (cleanup_rc == MYLITE_OK) {
        transaction->active = false;
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
        return primary_rc;
    }

    (void)snprintf(cleanup_error, sizeof(cleanup_error), "%s", sqlite3_errmsg(database->sqlite));
    if (sqlite3_get_autocommit(database->sqlite) == 0) {
        emergency_rc = mylite_execution_execute_cached_sqlite_control_sql(database, "ROLLBACK");
    }
    if (sqlite3_get_autocommit(database->sqlite) != 0) {
        transaction->active = false;
        transaction->kind = MYLITE_STATEMENT_TRANSACTION_NONE;
        database->session.user_transaction_active = false;
        database->session.active_transaction_read_only = false;
        database->session.active_transaction_isolation =
            MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    }
    mark_transaction_state_uncertain(
        database,
        &(const struct transaction_failure_context){
            .primary_sqlite_error = primary_sqlite_error,
            .failure_kind = "cleanup",
            .operation = failed_operation,
            .failure_error = cleanup_error,
            .emergency_rc = emergency_rc,
        }
    );
    return primary_rc == MYLITE_OK ? cleanup_rc : primary_rc;
}

static void mark_transaction_state_uncertain(
    struct mylite_db *database,
    const struct transaction_failure_context *failure
) {
    struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    struct mylite_diagnostic_record primary = diagnostics->condition;
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (failure == NULL) {
        return;
    }
    database->transaction_state_uncertain = true;
    if (primary.code == MYLITE_OK) {
        primary.code = mysql_error_unknown;
        (void)snprintf(primary.sqlstate, sizeof(primary.sqlstate), "%s", "HY000");
        (void)snprintf(
            primary.message,
            sizeof(primary.message),
            "%s",
            failure->primary_sqlite_error[0] == '\0' ? "statement execution failed"
                                                     : failure->primary_sqlite_error
        );
    }
    if (failure->emergency_rc == MYLITE_OK) {
        (void)snprintf(
            message,
            sizeof(message),
            "%.112s; transaction %s failed during %s: %.72s",
            primary.message,
            failure->failure_kind,
            failure->operation,
            failure->failure_error
        );
    } else {
        (void)snprintf(
            message,
            sizeof(message),
            "%.88s; transaction %s failed during %s: %.56s; emergency rollback failed",
            primary.message,
            failure->failure_kind,
            failure->operation,
            failure->failure_error
        );
    }
    mylite_diagnostics_set_error(diagnostics, primary.code, primary.sqlstate, message);
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
