#include "mylite_show_create_table_checks.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_body.h"
#include "sqlite3.h"

#include <string.h>

static int append_show_create_table_check(sqlite3_str *create_sql, sqlite3_stmt *select);

int mylite_show_create_table_append_checks(
    mylite_db *database,
    sqlite3_str *create_sql,
    const struct mylite_show_create_table_target *target,
    bool *first_line
) {
    sqlite3_stmt *select = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT constraint_name, check_clause, enforced FROM %s "
        "WHERE table_schema = ? AND table_name = ? "
        "ORDER BY constraint_name",
        mylite_catalog_check_constraint_catalog_name(target->temporary)
    );
    int rc = SQLITE_OK;

    if (sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return mylite_diagnostics_set_sqlite_error(database);
    }
    sqlite3_bind_text(
        select,
        1,
        target->schema_name,
        -1,
        mylite_show_sqlite_transient_destructor()
    );
    sqlite3_bind_text(select, 2, target->table_name, -1, mylite_show_sqlite_transient_destructor());

    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = MYLITE_OK;

        mylite_show_create_table_append_line_prefix(create_sql, first_line);
        status = append_show_create_table_check(create_sql, select);
        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }

    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : mylite_diagnostics_set_sqlite_error(database);
}

static int append_show_create_table_check(sqlite3_str *create_sql, sqlite3_stmt *select) {
    enum {
        constraint_name_column = 0,
        check_clause_column = 1,
        enforced_column = 2,
    };

    const char *constraint_name = (const char *)sqlite3_column_text(select, constraint_name_column);
    const char *check_clause = (const char *)sqlite3_column_text(select, check_clause_column);
    const char *enforced = (const char *)sqlite3_column_text(select, enforced_column);

    sqlite3_str_appendall(create_sql, "CONSTRAINT ");
    mylite_show_create_append_identifier(create_sql, constraint_name);
    sqlite3_str_appendall(create_sql, " CHECK (");
    sqlite3_str_appendall(create_sql, check_clause);
    sqlite3_str_appendall(create_sql, ")");
    if (enforced != NULL && strcmp(enforced, "NO") == 0) {
        sqlite3_str_appendall(create_sql, " /*!80016 NOT ENFORCED */");
    }
    return sqlite3_str_errcode(create_sql) == SQLITE_OK ? MYLITE_OK : MYLITE_NOMEM;
}
