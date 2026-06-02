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

int mylite_catalog_validate_database(struct mylite_db *database);
int mylite_catalog_validate_ready_database(struct mylite_db *database);
int mylite_catalog_validate_required_name(const char *name, size_t capacity);
int mylite_catalog_validate_optional_name(const char *name, size_t capacity);
int mylite_catalog_validate_logical_object_name(const char *name, size_t capacity);
int mylite_catalog_validate_table_kind(enum mylite_catalog_table_kind kind);
int mylite_catalog_validate_column_default_kind(enum mylite_catalog_column_default_kind kind);
int mylite_catalog_validate_bool_i64(int64_t value, bool *out_bool);
int mylite_catalog_validate_index_kind(enum mylite_catalog_index_kind kind);
int mylite_catalog_validate_active_mutation(const struct mylite_catalog_mutation *mutation);
int mylite_catalog_validate_positive_id(int64_t id);
int mylite_catalog_validate_positive_ordinal(int64_t ordinal_position);
int mylite_catalog_validate_generation(uint64_t generation);
int mylite_catalog_validate_schema_callback(mylite_catalog_schema_callback callback);
int mylite_catalog_validate_table_callback(mylite_catalog_table_callback callback);
int mylite_catalog_validate_column_callback(mylite_catalog_column_callback callback);
int mylite_catalog_validate_index_callback(mylite_catalog_index_callback callback);
int mylite_catalog_validate_index_column_callback(mylite_catalog_index_column_callback callback);
int mylite_catalog_validate_foreign_key_callback(mylite_catalog_foreign_key_callback callback);
int mylite_catalog_validate_foreign_key_column_callback(
    mylite_catalog_foreign_key_column_callback callback
);
int mylite_catalog_validate_check_constraint_callback(
    mylite_catalog_check_constraint_callback callback
);

#endif
