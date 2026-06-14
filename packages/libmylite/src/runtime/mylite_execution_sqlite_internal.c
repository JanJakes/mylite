#include "mylite_execution_sqlite_internal.h"

#include "mylite_connection.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

enum {
    sqlite_use_nul_terminated_string = -1,
};

int mylite_execution_execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

    if (rc == MYLITE_NOMEM) {
        mylite_execution_diagnostics_set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_diagnostics_set_physical_sqlite_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_execution_execute_sqlite_control_sql(const struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

int mylite_execution_prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(
        database->sqlite,
        sql,
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

int mylite_execution_finalize_sqlite_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}
