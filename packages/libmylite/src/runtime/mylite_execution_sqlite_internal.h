#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SQLITE_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SQLITE_INTERNAL_H

#include "sqlite3.h"

struct mylite_db;

int mylite_execution_execute_sqlite_schema_sql(struct mylite_db *database, const char *sql);
int mylite_execution_execute_sqlite_control_sql(const struct mylite_db *database, const char *sql);
int mylite_execution_prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
);
int mylite_execution_finalize_sqlite_statement(sqlite3_stmt *statement, int rc);

static inline int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    return mylite_execution_execute_sqlite_schema_sql(database, sql);
}

static inline int execute_sqlite_control_sql(const struct mylite_db *database, const char *sql) {
    return mylite_execution_execute_sqlite_control_sql(database, sql);
}

static inline int prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    return mylite_execution_prepare_sqlite_statement(database, sql, out_statement);
}

static inline int finalize_sqlite_statement(sqlite3_stmt *statement, int rc) {
    return mylite_execution_finalize_sqlite_statement(statement, rc);
}

#endif
