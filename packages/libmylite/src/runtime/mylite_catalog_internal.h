#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_INTERNAL_H

#include "mylite_catalog.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_catalog_generation_change {
    uint64_t next_generation;
};

struct mylite_catalog_column_values {
    const char *name;
    const char *logical_type;
    const char *physical_type;
    bool is_nullable;
    bool is_visible;
    bool is_auto_increment;
    enum mylite_catalog_column_default_kind default_kind;
    int64_t default_integer;
    const char *default_text;
    bool on_update_current_timestamp;
    const char *character_set_name;
    const char *collation_name;
    const char *comment;
    bool is_generated;
    enum mylite_catalog_generated_column_kind generated_kind;
    const char *generation_expression;
    const char *sqlite_generation_expression;
};

struct mylite_catalog_table_descriptor_input {
    int64_t schema_id;
    const char *name;
    const char *physical_name;
    enum mylite_catalog_table_kind kind;
    const char *default_charset;
    const char *default_collation;
    const char *comment;
    const char *row_format_option;
    int64_t key_block_size;
    int64_t pack_keys;
    int64_t checksum;
    int64_t stats_persistent;
    int64_t stats_auto_recalc;
    int64_t stats_sample_pages;
    int64_t created_time_utc_epoch;
    int64_t updated_time_utc_epoch;
};

int mylite_catalog_migrate_schema_one_step(sqlite3 *sqlite, uint32_t *schema_version);
int mylite_catalog_begin_generation_change(
    struct mylite_db *database,
    struct mylite_catalog_generation_change *out_change
);
int mylite_catalog_finish_generation_change(
    struct mylite_db *database,
    const struct mylite_catalog_generation_change *change
);
void mylite_catalog_abandon_generation_change(sqlite3 *sqlite);
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
int mylite_catalog_validate_column_values(
    const struct mylite_catalog_column_values *values,
    bool use_logical_object_name
);
int mylite_catalog_bind_column_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct mylite_catalog_column_values *values,
    uint64_t generation
);
int mylite_catalog_bind_column_replace_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t column_id,
    const struct mylite_catalog_column_values *values,
    uint64_t generation
);
int mylite_catalog_validate_table_descriptor_input(
    const struct mylite_catalog_table_descriptor_input *input
);
bool mylite_catalog_column_default_kind_stores_integer(
    enum mylite_catalog_column_default_kind default_kind
);
bool mylite_catalog_column_default_kind_stores_text(
    enum mylite_catalog_column_default_kind default_kind
);

int mylite_catalog_delete_foreign_keys_for_related_table_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id
);
int mylite_catalog_delete_foreign_keys_for_schema_from_sqlite(sqlite3 *sqlite, int64_t schema_id);
int mylite_catalog_delete_check_constraints_for_table_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id
);
int mylite_catalog_delete_check_constraints_for_schema_from_sqlite(
    sqlite3 *sqlite,
    int64_t schema_id
);

int mylite_catalog_read_schema_by_name_from_sqlite(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_read_schema_by_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_read_table_by_name_from_sqlite(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_read_table_by_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_read_view_by_table_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_view_descriptor *out_view
);
int mylite_catalog_read_column_by_name_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
);
int mylite_catalog_try_read_primary_index_by_table_id_from_sqlite(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
);
int mylite_catalog_read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id);
int mylite_catalog_read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id);
int mylite_catalog_read_next_foreign_key_id(sqlite3 *sqlite, int64_t *out_foreign_key_id);
int mylite_catalog_read_next_check_constraint_id(sqlite3 *sqlite, int64_t *out_check_constraint_id);
int mylite_catalog_read_inserted_index_column(
    struct mylite_db *database,
    int64_t index_id,
    int64_t ordinal_position,
    struct mylite_catalog_index_column_descriptor *out_index_column
);
int mylite_catalog_read_inserted_foreign_key(
    struct mylite_db *database,
    int64_t child_table_id,
    const char *name,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
);
int mylite_catalog_read_inserted_foreign_key_column(
    struct mylite_db *database,
    int64_t foreign_key_id,
    int64_t ordinal_position,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
);
int mylite_catalog_read_inserted_check_constraint(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
);

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
