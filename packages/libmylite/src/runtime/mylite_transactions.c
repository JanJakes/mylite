#include "mylite_transactions.h"

#include "mylite_diagnostics.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sqlite3.h"

static int execute_start_transaction_statement(mylite_stmt *stmt);
static int execute_begin_transaction_statement(mylite_stmt *stmt);
static int execute_commit_statement(mylite_stmt *stmt);
static int execute_rollback_statement(mylite_stmt *stmt);
static int finish_transaction_completion(mylite_stmt *stmt,
                                         enum mylite_transaction_access_mode chain_access_mode,
                                         bool chain_consistent_snapshot);

int mylite_transaction_begin_explicit(mylite_db *database,
                                      enum mylite_transaction_access_mode access_mode,
                                      bool consistent_snapshot)
{
    int rc = sqlite3_exec(database->sqlite, "BEGIN DEFERRED", NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    database->transaction_active = true;
    database->transaction_access_mode = access_mode;
    database->transaction_consistent_snapshot = consistent_snapshot;
    return MYLITE_OK;
}

int mylite_transaction_commit_explicit(mylite_db *database)
{
    int status = mylite_transaction_commit_storage(database);

    if (status != MYLITE_OK) {
        return status;
    }

    database->transaction_active = false;
    database->transaction_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    database->transaction_consistent_snapshot = false;
    mylite_transaction_clear_user_savepoints(database);
    mylite_transaction_clear_pending_auto_increments(database);
    return MYLITE_OK;
}

int mylite_transaction_rollback_explicit(mylite_db *database)
{
    int status = MYLITE_OK;
    int rc = sqlite3_exec(database->sqlite, "ROLLBACK", NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }

    database->transaction_active = false;
    database->transaction_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    database->transaction_consistent_snapshot = false;
    status = mylite_transaction_reapply_pending_auto_increments(database);
    mylite_transaction_clear_user_savepoints(database);
    mylite_transaction_clear_pending_auto_increments(database);
    return status;
}

int mylite_transaction_copy_statement(const struct mylite_sql_ast_node *statement,
                                      mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *characteristics = NULL;
    const struct mylite_sql_ast_node *completion = NULL;

    stmt->transaction.access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    stmt->transaction.completion_chain = MYLITE_TRANSACTION_COMPLETION_CHAIN_DEFAULT;
    stmt->transaction.completion_release = MYLITE_TRANSACTION_COMPLETION_RELEASE_DEFAULT;

    if (statement->kind == MYLITE_SQL_AST_START_TRANSACTION_STATEMENT) {
        characteristics = mylite_ast_child_at(statement, 0U);
        for (const struct mylite_sql_ast_node *item =
                 characteristics == NULL ? NULL : characteristics->first_child;
             item != NULL; item = item->next_sibling) {
            if (item->transaction_access_mode == MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE) {
                stmt->transaction.has_access_mode = true;
                stmt->transaction.access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
            } else if (item->transaction_access_mode ==
                       MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY) {
                stmt->transaction.has_access_mode = true;
                stmt->transaction.access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
            }
            if (item->transaction_consistent_snapshot) {
                stmt->transaction.consistent_snapshot = true;
            }
        }
        return MYLITE_OK;
    }

    if (statement->kind == MYLITE_SQL_AST_COMMIT_STATEMENT ||
        statement->kind == MYLITE_SQL_AST_ROLLBACK_STATEMENT) {
        completion = mylite_ast_child_at(statement, 0U);
        if (completion != NULL) {
            switch (completion->transaction_chain) {
            case MYLITE_SQL_AST_TRANSACTION_CHAIN_YES:
                stmt->transaction.completion_chain = MYLITE_TRANSACTION_COMPLETION_CHAIN_YES;
                break;
            case MYLITE_SQL_AST_TRANSACTION_CHAIN_NO:
                stmt->transaction.completion_chain = MYLITE_TRANSACTION_COMPLETION_CHAIN_NO;
                break;
            case MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT:
                break;
            }
            switch (completion->transaction_release) {
            case MYLITE_SQL_AST_TRANSACTION_RELEASE_YES:
                stmt->transaction.completion_release = MYLITE_TRANSACTION_COMPLETION_RELEASE_YES;
                break;
            case MYLITE_SQL_AST_TRANSACTION_RELEASE_NO:
                stmt->transaction.completion_release = MYLITE_TRANSACTION_COMPLETION_RELEASE_NO;
                break;
            case MYLITE_SQL_AST_TRANSACTION_RELEASE_DEFAULT:
                break;
            }
        }
    }

    return MYLITE_OK;
}

int mylite_transaction_execute_statement(mylite_stmt *stmt)
{
    switch (stmt->kind) {
    case MYLITE_STMT_START_TRANSACTION:
        return execute_start_transaction_statement(stmt);
    case MYLITE_STMT_BEGIN_TRANSACTION:
        return execute_begin_transaction_statement(stmt);
    case MYLITE_STMT_COMMIT:
        return execute_commit_statement(stmt);
    case MYLITE_STMT_ROLLBACK:
        return execute_rollback_statement(stmt);
    case MYLITE_STMT_SAVEPOINT:
        return mylite_transaction_execute_savepoint_statement(stmt);
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
        return mylite_transaction_execute_rollback_to_savepoint_statement(stmt);
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        return mylite_transaction_execute_release_savepoint_statement(stmt);
    case MYLITE_STMT_SQLITE:
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_SET_SYSTEM_VARIABLE:
    case MYLITE_STMT_SET_USER_VARIABLE:
    case MYLITE_STMT_PREPARE_STATEMENT:
    case MYLITE_STMT_EXECUTE_PREPARED:
    case MYLITE_STMT_DEALLOCATE_PREPARE:
    case MYLITE_STMT_CALL_PLACEHOLDER:
    case MYLITE_STMT_CREATE_PROCEDURE_PLACEHOLDER:
    case MYLITE_STMT_CREATE_FUNCTION_PLACEHOLDER:
    case MYLITE_STMT_CREATE_TRIGGER_PLACEHOLDER:
    case MYLITE_STMT_CREATE_EVENT_PLACEHOLDER:
    case MYLITE_STMT_DROP_PROCEDURE_PLACEHOLDER:
    case MYLITE_STMT_DROP_FUNCTION_PLACEHOLDER:
    case MYLITE_STMT_DROP_TRIGGER_PLACEHOLDER:
    case MYLITE_STMT_DROP_EVENT_PLACEHOLDER:
    case MYLITE_STMT_SIGNAL_PLACEHOLDER:
    case MYLITE_STMT_CREATE_TABLE:
    case MYLITE_STMT_DROP_TABLE:
    case MYLITE_STMT_INSERT_VALUES:
    case MYLITE_STMT_INSERT_SET:
    case MYLITE_STMT_REPLACE_VALUES:
    case MYLITE_STMT_REPLACE_SET:
    case MYLITE_STMT_SCALAR_SELECT:
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_UNION_QUERY:
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
    case MYLITE_STMT_RENAME_TABLE:
    case MYLITE_STMT_TRUNCATE_TABLE:
    case MYLITE_STMT_CREATE_INDEX:
    case MYLITE_STMT_DROP_INDEX:
    case MYLITE_STMT_ALTER_TABLE:
        break;
    }

    return MYLITE_MISUSE;
}

static int execute_start_transaction_statement(mylite_stmt *stmt)
{
    enum mylite_transaction_access_mode access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    int status = MYLITE_OK;

    if (stmt->transaction.has_access_mode) {
        access_mode = stmt->transaction.access_mode;
    }
    if (stmt->database->transaction_active) {
        status = mylite_transaction_commit_explicit(stmt->database);
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
            return status;
        }
    }

    status = mylite_transaction_begin_explicit(stmt->database, access_mode,
                                               stmt->transaction.consistent_snapshot);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
        return status;
    }

    stmt->affected_rows = 0;
    return MYLITE_OK;
}

static int execute_begin_transaction_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (stmt->database->transaction_active) {
        status = mylite_transaction_commit_explicit(stmt->database);
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
            return status;
        }
    }

    status = mylite_transaction_begin_explicit(stmt->database, MYLITE_TRANSACTION_ACCESS_READ_WRITE,
                                               false);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
        return status;
    }

    stmt->affected_rows = 0;
    return MYLITE_OK;
}

static int execute_commit_statement(mylite_stmt *stmt)
{
    enum mylite_transaction_access_mode chain_access_mode = stmt->database->transaction_access_mode;
    bool chain_consistent_snapshot = stmt->database->transaction_consistent_snapshot;
    int status = MYLITE_OK;

    if (stmt->database->transaction_active) {
        status = mylite_transaction_commit_explicit(stmt->database);
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
            return status;
        }
    }

    status = finish_transaction_completion(stmt, chain_access_mode, chain_consistent_snapshot);
    stmt->affected_rows = status == MYLITE_OK ? 0 : -1;
    return status;
}

static int execute_rollback_statement(mylite_stmt *stmt)
{
    enum mylite_transaction_access_mode chain_access_mode = stmt->database->transaction_access_mode;
    bool chain_consistent_snapshot = stmt->database->transaction_consistent_snapshot;
    int status = MYLITE_OK;

    if (stmt->database->transaction_active) {
        status = mylite_transaction_rollback_explicit(stmt->database);
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
            return status;
        }
    }

    status = finish_transaction_completion(stmt, chain_access_mode, chain_consistent_snapshot);
    stmt->affected_rows = status == MYLITE_OK ? 0 : -1;
    return status;
}

static int finish_transaction_completion(mylite_stmt *stmt,
                                         enum mylite_transaction_access_mode chain_access_mode,
                                         bool chain_consistent_snapshot)
{
    int status = MYLITE_OK;

    if (stmt->transaction.completion_chain == MYLITE_TRANSACTION_COMPLETION_CHAIN_YES) {
        status = mylite_transaction_begin_explicit(stmt->database, chain_access_mode,
                                                   chain_consistent_snapshot);
        if (status != MYLITE_OK) {
            return status;
        }
    }

    if (stmt->transaction.completion_release == MYLITE_TRANSACTION_COMPLETION_RELEASE_YES) {
        stmt->database->transaction_released = true;
    }
    return MYLITE_OK;
}

int mylite_transaction_begin_statement_atomicity(mylite_db *database,
                                                 struct mylite_statement_atomicity *atomicity)
{
    int rc = SQLITE_OK;

    if (atomicity == NULL) {
        return MYLITE_MISUSE;
    }

    *atomicity = (struct mylite_statement_atomicity){0};
    if (!database->transaction_active) {
        int status = mylite_transaction_begin_storage(database);

        if (status == MYLITE_OK) {
            atomicity->kind = MYLITE_STATEMENT_ATOMICITY_TRANSACTION;
        }
        return status;
    }

    rc = sqlite3_exec(database->sqlite, "SAVEPOINT mylite_statement_atomicity", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    atomicity->kind = MYLITE_STATEMENT_ATOMICITY_SAVEPOINT;
    return MYLITE_OK;
}

int mylite_transaction_commit_statement_atomicity(mylite_db *database,
                                                  struct mylite_statement_atomicity *atomicity)
{
    int rc = SQLITE_OK;

    if (atomicity == NULL) {
        return MYLITE_MISUSE;
    }

    switch (atomicity->kind) {
    case MYLITE_STATEMENT_ATOMICITY_TRANSACTION:
        atomicity->kind = MYLITE_STATEMENT_ATOMICITY_NONE;
        return mylite_transaction_commit_storage(database);
    case MYLITE_STATEMENT_ATOMICITY_SAVEPOINT:
        rc = sqlite3_exec(database->sqlite, "RELEASE SAVEPOINT mylite_statement_atomicity", NULL,
                          NULL, NULL);
        atomicity->kind = MYLITE_STATEMENT_ATOMICITY_NONE;
        return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
    case MYLITE_STATEMENT_ATOMICITY_NONE:
        return MYLITE_OK;
    }

    return MYLITE_MISUSE;
}

void mylite_transaction_rollback_statement_atomicity(
    mylite_db *database, const struct mylite_statement_atomicity *atomicity)
{
    if (atomicity == NULL) {
        return;
    }

    switch (atomicity->kind) {
    case MYLITE_STATEMENT_ATOMICITY_TRANSACTION:
        mylite_transaction_rollback_storage(database);
        break;
    case MYLITE_STATEMENT_ATOMICITY_SAVEPOINT:
        (void)sqlite3_exec(database->sqlite, "ROLLBACK TO SAVEPOINT mylite_statement_atomicity",
                           NULL, NULL, NULL);
        (void)sqlite3_exec(database->sqlite, "RELEASE SAVEPOINT mylite_statement_atomicity", NULL,
                           NULL, NULL);
        break;
    case MYLITE_STATEMENT_ATOMICITY_NONE:
        break;
    }
}

int mylite_transaction_set_read_only_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(
        database, "Cannot execute statement in a READ ONLY transaction");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

int mylite_transaction_begin_storage(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

int mylite_transaction_commit_storage(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, "COMMIT", NULL, NULL, NULL);

    return rc == SQLITE_OK ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

void mylite_transaction_rollback_storage(mylite_db *database)
{
    (void)sqlite3_exec(database->sqlite, "ROLLBACK", NULL, NULL, NULL);
}
