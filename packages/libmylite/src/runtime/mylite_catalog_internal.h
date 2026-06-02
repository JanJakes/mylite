#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_INTERNAL_H

#include "mylite_catalog.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int mylite_catalog_migrate_schema_one_step(sqlite3 *sqlite, uint32_t *schema_version);
int mylite_catalog_execute_sql(sqlite3 *sqlite, const char *sql);
int mylite_catalog_prepare_statement(
    sqlite3 *sqlite,
    const char *sql,
    sqlite3_stmt **out_statement
);
int mylite_catalog_bind_text(sqlite3_stmt *statement, int index, const char *value);
int mylite_catalog_bind_nullable_text(
    sqlite3_stmt *statement,
    int index,
    bool has_value,
    const char *value
);
int mylite_catalog_bind_i64(sqlite3_stmt *statement, int index, int64_t value);
int mylite_catalog_bind_nullable_i64(
    sqlite3_stmt *statement,
    int index,
    bool has_value,
    int64_t value
);
int mylite_catalog_bind_u64(sqlite3_stmt *statement, int index, uint64_t value);
int64_t mylite_catalog_bool_value(bool value);
int mylite_catalog_step_done(sqlite3_stmt *statement);
int mylite_catalog_require_changed_row(sqlite3 *sqlite);
int mylite_catalog_finalize_statement(sqlite3_stmt *statement, int rc);
int mylite_catalog_checked_column_i64(sqlite3_stmt *statement, int index, int64_t *out_value);
int mylite_catalog_checked_column_u64(sqlite3_stmt *statement, int index, uint64_t *out_value);
int mylite_catalog_checked_nullable_column_i64(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    int64_t *out_value
);
int mylite_catalog_checked_nullable_column_text(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    char *destination,
    size_t destination_size
);
int mylite_catalog_checked_column_text(
    sqlite3_stmt *statement,
    int index,
    char *destination,
    size_t destination_size
);
int mylite_catalog_u64_to_i64(uint64_t value, int64_t *out_value);
int mylite_catalog_i64_to_u32(int64_t value, uint32_t *out_value);
int mylite_catalog_i64_to_u64(int64_t value, uint64_t *out_value);

#endif
