#include "mylite_catalog.h"

#include "mylite_connection.h"
#include "mylite_file_format.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    catalog_table_count = 9,
    pre_check_constraint_catalog_table_count = 8,
    downgraded_catalog_with_check_constraint_table_count = 7,
    pre_foreign_key_catalog_table_count = 6,
    legacy_catalog_table_count = 4,
    catalog_schema_version_v5 = 5U,
    catalog_schema_version_v6 = 6U,
    catalog_schema_version_v7 = 7U,
    catalog_schema_version_v8 = 8U,
    catalog_schema_version_v9 = 9U,
    catalog_schema_version_v10 = 10U,
    catalog_schema_version_v11 = 11U,
    catalog_schema_version_v12 = 12U,
    catalog_schema_version_v13 = 13U,
    catalog_schema_version_v14 = 14U,
    catalog_schema_version_v15 = 15U,
    catalog_schema_version_v16 = 16U,
    catalog_schema_version_v17 = 17U,
    catalog_schema_version_v18 = 18U,
    catalog_schema_version_v19 = 19U,
    catalog_schema_version_v20 = 20U,
    catalog_schema_version_v21 = 21U,
    catalog_schema_version_v22 = 22U,
    catalog_schema_version_v23 = 23U,
    catalog_schema_version_v24 = 24U,
    catalog_schema_version_v25 = 25U,
    catalog_schema_version_v26 = 26U,
    catalog_schema_version_v27 = 27U,
    catalog_schema_version_v28 = 28U,
    catalog_schema_version_v29 = 29U,
    sqlite_use_nul_terminated_string = -1,
};

enum catalog_table_insert_bind_index {
    catalog_table_insert_schema_id_bind = 1,
    catalog_table_insert_name_bind = 2,
    catalog_table_insert_kind_bind = 3,
    catalog_table_insert_physical_name_bind = 4,
    catalog_table_insert_auto_increment_next_bind = 5,
    catalog_table_insert_default_charset_bind = 6,
    catalog_table_insert_default_collation_bind = 7,
    catalog_table_insert_comment_bind = 8,
    catalog_table_insert_created_time_bind = 9,
    catalog_table_insert_updated_time_bind = 10,
    catalog_table_insert_generation_bind = 11,
};

enum catalog_table_insert_in_mutation_bind_index {
    catalog_table_insert_in_mutation_table_id_bind = 1,
    catalog_table_insert_in_mutation_schema_id_bind = 2,
    catalog_table_insert_in_mutation_name_bind = 3,
    catalog_table_insert_in_mutation_kind_bind = 4,
    catalog_table_insert_in_mutation_physical_name_bind = 5,
    catalog_table_insert_in_mutation_auto_increment_next_bind = 6,
    catalog_table_insert_in_mutation_default_charset_bind = 7,
    catalog_table_insert_in_mutation_default_collation_bind = 8,
    catalog_table_insert_in_mutation_comment_bind = 9,
    catalog_table_insert_in_mutation_created_time_bind = 10,
    catalog_table_insert_in_mutation_updated_time_bind = 11,
    catalog_table_insert_in_mutation_generation_bind = 12,
};

enum catalog_column_insert_bind_index {
    catalog_column_insert_table_id_bind = 1,
    catalog_column_insert_ordinal_position_bind = 2,
    catalog_column_insert_name_bind = 3,
    catalog_column_insert_logical_type_bind = 4,
    catalog_column_insert_physical_type_bind = 5,
    catalog_column_insert_is_nullable_bind = 6,
    catalog_column_insert_is_visible_bind = 7,
    catalog_column_insert_is_auto_increment_bind = 8,
    catalog_column_insert_default_kind_bind = 9,
    catalog_column_insert_default_integer_bind = 10,
    catalog_column_insert_default_text_bind = 11,
    catalog_column_insert_on_update_current_timestamp_bind = 12,
    catalog_column_insert_character_set_name_bind = 13,
    catalog_column_insert_collation_name_bind = 14,
    catalog_column_insert_comment_bind = 15,
    catalog_column_insert_is_generated_bind = 16,
    catalog_column_insert_generated_kind_bind = 17,
    catalog_column_insert_generation_expression_bind = 18,
    catalog_column_insert_sqlite_generation_expression_bind = 19,
    catalog_column_insert_generation_bind = 20,
};

enum catalog_column_replace_bind_index {
    catalog_column_replace_name_bind = 1,
    catalog_column_replace_logical_type_bind = 2,
    catalog_column_replace_physical_type_bind = 3,
    catalog_column_replace_is_nullable_bind = 4,
    catalog_column_replace_is_visible_bind = 5,
    catalog_column_replace_is_auto_increment_bind = 6,
    catalog_column_replace_default_kind_bind = 7,
    catalog_column_replace_default_integer_bind = 8,
    catalog_column_replace_default_text_bind = 9,
    catalog_column_replace_on_update_current_timestamp_bind = 10,
    catalog_column_replace_character_set_name_bind = 11,
    catalog_column_replace_collation_name_bind = 12,
    catalog_column_replace_comment_bind = 13,
    catalog_column_replace_is_generated_bind = 14,
    catalog_column_replace_generated_kind_bind = 15,
    catalog_column_replace_generation_expression_bind = 16,
    catalog_column_replace_sqlite_generation_expression_bind = 17,
    catalog_column_replace_generation_bind = 18,
    catalog_column_replace_table_id_bind = 19,
    catalog_column_replace_column_id_bind = 20,
};

enum catalog_column_reorder_offset_bind_index {
    catalog_column_reorder_offset_bind = 1,
    catalog_column_reorder_offset_table_id_bind = 2,
};

enum catalog_column_reorder_bind_index {
    catalog_column_reorder_ordinal_bind = 1,
    catalog_column_reorder_version_increment_bind = 2,
    catalog_column_reorder_generation_bind = 3,
    catalog_column_reorder_table_id_bind = 4,
    catalog_column_reorder_column_id_bind = 5,
};

enum catalog_index_insert_bind_index {
    catalog_index_insert_index_id_bind = 1,
    catalog_index_insert_table_id_bind = 2,
    catalog_index_insert_name_bind = 3,
    catalog_index_insert_kind_bind = 4,
    catalog_index_insert_is_unique_bind = 5,
    catalog_index_insert_is_visible_bind = 6,
    catalog_index_insert_physical_name_bind = 7,
    catalog_index_insert_comment_bind = 8,
    catalog_index_insert_show_create_explicit_btree_bind = 9,
    catalog_index_insert_generation_bind = 10,
};

enum catalog_index_column_insert_bind_index {
    catalog_index_column_insert_index_id_bind = 1,
    catalog_index_column_insert_table_id_bind = 2,
    catalog_index_column_insert_column_id_bind = 3,
    catalog_index_column_insert_ordinal_position_bind = 4,
    catalog_index_column_insert_prefix_length_bind = 5,
    catalog_index_column_insert_sort_direction_bind = 6,
    catalog_index_column_insert_generation_bind = 7,
};

enum catalog_foreign_key_insert_bind_index {
    catalog_foreign_key_insert_foreign_key_id_bind = 1,
    catalog_foreign_key_insert_child_table_id_bind = 2,
    catalog_foreign_key_insert_parent_table_id_bind = 3,
    catalog_foreign_key_insert_name_bind = 4,
    catalog_foreign_key_insert_parent_index_id_bind = 5,
    catalog_foreign_key_insert_child_index_id_bind = 6,
    catalog_foreign_key_insert_update_rule_bind = 7,
    catalog_foreign_key_insert_delete_rule_bind = 8,
    catalog_foreign_key_insert_match_option_bind = 9,
    catalog_foreign_key_insert_generation_bind = 10,
};

enum catalog_foreign_key_column_insert_bind_index {
    catalog_foreign_key_column_insert_foreign_key_id_bind = 1,
    catalog_foreign_key_column_insert_child_table_id_bind = 2,
    catalog_foreign_key_column_insert_parent_table_id_bind = 3,
    catalog_foreign_key_column_insert_child_column_id_bind = 4,
    catalog_foreign_key_column_insert_parent_column_id_bind = 5,
    catalog_foreign_key_column_insert_ordinal_position_bind = 6,
    catalog_foreign_key_column_insert_position_in_unique_constraint_bind = 7,
    catalog_foreign_key_column_insert_generation_bind = 8,
};

enum catalog_check_constraint_insert_bind_index {
    catalog_check_constraint_insert_check_constraint_id_bind = 1,
    catalog_check_constraint_insert_table_id_bind = 2,
    catalog_check_constraint_insert_name_bind = 3,
    catalog_check_constraint_insert_physical_name_bind = 4,
    catalog_check_constraint_insert_check_clause_bind = 5,
    catalog_check_constraint_insert_sqlite_expression_bind = 6,
    catalog_check_constraint_insert_is_enforced_bind = 7,
    catalog_check_constraint_insert_name_is_generated_bind = 8,
    catalog_check_constraint_insert_generated_ordinal_bind = 9,
    catalog_check_constraint_insert_ordinal_position_bind = 10,
    catalog_check_constraint_insert_generation_bind = 11,
};

enum catalog_table_select_column_index {
    catalog_table_select_table_id_column = 0,
    catalog_table_select_schema_id_column = 1,
    catalog_table_select_name_column = 2,
    catalog_table_select_kind_column = 3,
    catalog_table_select_physical_name_column = 4,
    catalog_table_select_auto_increment_next_column = 5,
    catalog_table_select_default_charset_column = 6,
    catalog_table_select_default_collation_column = 7,
    catalog_table_select_comment_column = 8,
    catalog_table_select_fulltext_doc_id_initialized_column = 9,
    catalog_table_select_created_time_column = 10,
    catalog_table_select_updated_time_column = 11,
    catalog_table_select_descriptor_version_column = 12,
    catalog_table_select_created_generation_column = 13,
    catalog_table_select_updated_generation_column = 14,
};

enum catalog_column_select_column_index {
    catalog_column_select_column_id_column = 0,
    catalog_column_select_table_id_column = 1,
    catalog_column_select_ordinal_position_column = 2,
    catalog_column_select_name_column = 3,
    catalog_column_select_logical_type_column = 4,
    catalog_column_select_physical_type_column = 5,
    catalog_column_select_is_nullable_column = 6,
    catalog_column_select_is_visible_column = 7,
    catalog_column_select_is_auto_increment_column = 8,
    catalog_column_select_default_kind_column = 9,
    catalog_column_select_default_integer_column = 10,
    catalog_column_select_default_text_column = 11,
    catalog_column_select_on_update_current_timestamp_column = 12,
    catalog_column_select_character_set_name_column = 13,
    catalog_column_select_collation_name_column = 14,
    catalog_column_select_comment_column = 15,
    catalog_column_select_is_generated_column = 16,
    catalog_column_select_generated_kind_column = 17,
    catalog_column_select_generation_expression_column = 18,
    catalog_column_select_sqlite_generation_expression_column = 19,
    catalog_column_select_descriptor_version_column = 20,
    catalog_column_select_created_generation_column = 21,
    catalog_column_select_updated_generation_column = 22,
};

enum catalog_index_select_column_index {
    catalog_index_select_index_id_column = 0,
    catalog_index_select_table_id_column = 1,
    catalog_index_select_name_column = 2,
    catalog_index_select_kind_column = 3,
    catalog_index_select_is_unique_column = 4,
    catalog_index_select_is_visible_column = 5,
    catalog_index_select_physical_name_column = 6,
    catalog_index_select_comment_column = 7,
    catalog_index_select_show_create_explicit_btree_column = 8,
    catalog_index_select_descriptor_version_column = 9,
    catalog_index_select_created_generation_column = 10,
    catalog_index_select_updated_generation_column = 11,
};

enum catalog_index_column_select_column_index {
    catalog_index_column_select_index_column_id_column = 0,
    catalog_index_column_select_index_id_column = 1,
    catalog_index_column_select_table_id_column = 2,
    catalog_index_column_select_column_id_column = 3,
    catalog_index_column_select_ordinal_position_column = 4,
    catalog_index_column_select_prefix_length_column = 5,
    catalog_index_column_select_sort_direction_column = 6,
    catalog_index_column_select_descriptor_version_column = 7,
    catalog_index_column_select_created_generation_column = 8,
    catalog_index_column_select_updated_generation_column = 9,
};

enum catalog_foreign_key_select_column_index {
    catalog_foreign_key_select_foreign_key_id_column = 0,
    catalog_foreign_key_select_child_table_id_column = 1,
    catalog_foreign_key_select_parent_table_id_column = 2,
    catalog_foreign_key_select_name_column = 3,
    catalog_foreign_key_select_parent_index_id_column = 4,
    catalog_foreign_key_select_child_index_id_column = 5,
    catalog_foreign_key_select_update_rule_column = 6,
    catalog_foreign_key_select_delete_rule_column = 7,
    catalog_foreign_key_select_match_option_column = 8,
    catalog_foreign_key_select_descriptor_version_column = 9,
    catalog_foreign_key_select_created_generation_column = 10,
    catalog_foreign_key_select_updated_generation_column = 11,
};

enum catalog_foreign_key_column_select_column_index {
    catalog_foreign_key_column_select_foreign_key_column_id_column = 0,
    catalog_foreign_key_column_select_foreign_key_id_column = 1,
    catalog_foreign_key_column_select_child_table_id_column = 2,
    catalog_foreign_key_column_select_parent_table_id_column = 3,
    catalog_foreign_key_column_select_child_column_id_column = 4,
    catalog_foreign_key_column_select_parent_column_id_column = 5,
    catalog_foreign_key_column_select_ordinal_position_column = 6,
    catalog_foreign_key_column_select_position_in_unique_constraint_column = 7,
    catalog_foreign_key_column_select_descriptor_version_column = 8,
    catalog_foreign_key_column_select_created_generation_column = 9,
    catalog_foreign_key_column_select_updated_generation_column = 10,
};

enum catalog_check_constraint_select_column_index {
    catalog_check_constraint_select_check_constraint_id_column = 0,
    catalog_check_constraint_select_table_id_column = 1,
    catalog_check_constraint_select_name_column = 2,
    catalog_check_constraint_select_physical_name_column = 3,
    catalog_check_constraint_select_check_clause_column = 4,
    catalog_check_constraint_select_sqlite_expression_column = 5,
    catalog_check_constraint_select_is_enforced_column = 6,
    catalog_check_constraint_select_name_is_generated_column = 7,
    catalog_check_constraint_select_generated_ordinal_column = 8,
    catalog_check_constraint_select_ordinal_position_column = 9,
    catalog_check_constraint_select_descriptor_version_column = 10,
    catalog_check_constraint_select_created_generation_column = 11,
    catalog_check_constraint_select_updated_generation_column = 12,
};

enum catalog_next_table_id_column_index {
    catalog_next_table_id_column = 0,
};

enum catalog_state_select_column_index {
    catalog_state_select_singleton_id_column = 0,
    catalog_state_select_schema_version_column = 1,
    catalog_state_select_minimum_reader_schema_version_column = 2,
    catalog_state_select_catalog_generation_column = 3,
    catalog_state_select_file_format_version_column = 4,
};

enum catalog_schema_select_column_index {
    catalog_schema_select_schema_id_column = 0,
    catalog_schema_select_name_column = 1,
    catalog_schema_select_default_charset_column = 2,
    catalog_schema_select_default_collation_column = 3,
    catalog_schema_select_descriptor_version_column = 4,
    catalog_schema_select_created_generation_column = 5,
    catalog_schema_select_updated_generation_column = 6,
};

struct catalog_generation_change {
    uint64_t next_generation;
};

struct catalog_column_values {
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

struct catalog_column_default_bind_indexes {
    int default_kind;
    int default_integer;
    int default_text;
    int on_update_current_timestamp;
};

struct catalog_column_text_attribute_bind_indexes {
    int character_set_name;
    int collation_name;
    int comment;
};

struct catalog_column_generated_bind_indexes {
    int is_generated;
    int generated_kind;
    int generation_expression;
    int sqlite_generation_expression;
};

struct catalog_table_descriptor_input {
    int64_t schema_id;
    const char *name;
    const char *physical_name;
    enum mylite_catalog_table_kind kind;
    const char *default_charset;
    const char *default_collation;
    const char *comment;
    int64_t created_time_utc_epoch;
    int64_t updated_time_utc_epoch;
};

static int ensure_catalog_schema(struct mylite_db *database);
static int load_existing_catalog(struct mylite_db *database);
static int migrate_catalog_schema(struct mylite_db *database, const struct mylite_catalog *catalog);
static int migrate_catalog_schema_one_step(sqlite3 *sqlite, uint32_t *schema_version);
static int migrate_catalog_schema_v1_to_v2(sqlite3 *sqlite);
static int migrate_catalog_schema_v2_to_v3(sqlite3 *sqlite);
static int migrate_catalog_schema_v3_to_v4(sqlite3 *sqlite);
static int migrate_catalog_schema_v4_to_v5(sqlite3 *sqlite);
static int migrate_catalog_schema_v5_to_v6(sqlite3 *sqlite);
static int migrate_catalog_schema_v6_to_v7(sqlite3 *sqlite);
static int migrate_catalog_schema_v7_to_v8(sqlite3 *sqlite);
static int migrate_catalog_schema_v8_to_v9(sqlite3 *sqlite);
static int migrate_catalog_schema_v9_to_v10(sqlite3 *sqlite);
static int migrate_catalog_schema_v10_to_v11(sqlite3 *sqlite);
static int migrate_catalog_schema_v11_to_v12(sqlite3 *sqlite);
static int migrate_catalog_schema_v12_to_v13(sqlite3 *sqlite);
static int migrate_catalog_schema_v13_to_v14(sqlite3 *sqlite);
static int migrate_catalog_schema_v14_to_v15(sqlite3 *sqlite);
static int migrate_catalog_schema_v15_to_v16(sqlite3 *sqlite);
static int migrate_catalog_schema_v16_to_v17(sqlite3 *sqlite);
static int migrate_catalog_schema_v17_to_v18(sqlite3 *sqlite);
static int migrate_catalog_schema_v18_to_v19(sqlite3 *sqlite);
static int migrate_catalog_schema_v19_to_v20(sqlite3 *sqlite);
static int migrate_catalog_schema_v20_to_v21(sqlite3 *sqlite);
static int migrate_catalog_schema_v21_to_v22(sqlite3 *sqlite);
static int migrate_catalog_schema_v22_to_v23(sqlite3 *sqlite);
static int migrate_catalog_schema_v23_to_v24(sqlite3 *sqlite);
static int migrate_catalog_schema_v24_to_v25(sqlite3 *sqlite);
static int migrate_catalog_schema_v25_to_v26(sqlite3 *sqlite);
static int migrate_catalog_schema_v26_to_v27(sqlite3 *sqlite);
static int migrate_catalog_schema_v27_to_v28(sqlite3 *sqlite);
static int migrate_catalog_schema_v28_to_v29(sqlite3 *sqlite);
static int migrate_catalog_schema_v29_to_v30(sqlite3 *sqlite);
static int validate_catalog_descriptor_tables(sqlite3 *sqlite);
static int validate_select_shape(sqlite3 *sqlite, const char *sql);
static int initialize_catalog_schema(struct mylite_db *database);
static int existing_catalog_table_count(sqlite3 *sqlite, int *out_count);
static int read_catalog_state(sqlite3 *sqlite, struct mylite_catalog *catalog);
static int apply_catalog_state(struct mylite_db *database, const struct mylite_catalog *catalog);
static int begin_catalog_transaction(sqlite3 *sqlite);
static int commit_catalog_transaction(sqlite3 *sqlite);
static void rollback_catalog_transaction(sqlite3 *sqlite);
static int begin_generation_change(
    struct mylite_db *database,
    struct catalog_generation_change *out_change
);
static int finish_generation_change(
    struct mylite_db *database,
    const struct catalog_generation_change *change
);
static void abandon_generation_change(sqlite3 *sqlite);
static int update_catalog_generation(sqlite3 *sqlite, uint64_t generation);
static int execute_sql(sqlite3 *sqlite, const char *sql);
static int prepare_statement(sqlite3 *sqlite, const char *sql, sqlite3_stmt **out_statement);
static int bind_text(sqlite3_stmt *statement, int index, const char *value);
static int bind_nullable_text(
    sqlite3_stmt *statement,
    int index,
    bool has_value,
    const char *value
);
static int bind_i64(sqlite3_stmt *statement, int index, int64_t value);
static int bind_nullable_i64(sqlite3_stmt *statement, int index, bool has_value, int64_t value);
static int bind_u64(sqlite3_stmt *statement, int index, uint64_t value);
static int64_t catalog_bool_value(bool value);
static int validate_catalog_table_descriptor_input(
    const struct catalog_table_descriptor_input *input
);
static int bind_catalog_column_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct catalog_column_values *values,
    uint64_t generation
);
static int bind_catalog_column_replace_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t column_id,
    const struct catalog_column_values *values,
    uint64_t generation
);
static int bind_catalog_column_insert_core_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct catalog_column_values *values
);
static int bind_catalog_column_replace_core_values(
    sqlite3_stmt *statement,
    const struct catalog_column_values *values
);
static int bind_catalog_column_default_values(
    sqlite3_stmt *statement,
    struct catalog_column_default_bind_indexes indexes,
    const struct catalog_column_values *values
);
static int bind_catalog_column_text_attributes(
    sqlite3_stmt *statement,
    struct catalog_column_text_attribute_bind_indexes indexes,
    const struct catalog_column_values *values
);
static int bind_catalog_column_generated_values(
    sqlite3_stmt *statement,
    struct catalog_column_generated_bind_indexes indexes,
    const struct catalog_column_values *values
);
static const char *catalog_text_or_empty(const char *value);
static int mark_table_fulltext_doc_id_initialized_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
);
static int offset_catalog_column_ordinals_for_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_column_reorder *reorder
);
static int apply_catalog_column_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder,
    size_t column_index
);
static int step_done(sqlite3_stmt *statement);
static int require_changed_row(sqlite3 *sqlite);
static int finalize_statement(sqlite3_stmt *statement, int rc);
static int read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int try_read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
);
static int read_schema_by_id(
    sqlite3 *sqlite,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
);
static int try_read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
);
static int read_table_by_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
);
static int read_column_by_name(
    sqlite3 *sqlite,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
);
static int try_read_primary_index_by_table_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
);
static int read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id);
static int read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id);
static int read_next_foreign_key_id(sqlite3 *sqlite, int64_t *out_foreign_key_id);
static int read_next_check_constraint_id(sqlite3 *sqlite, int64_t *out_check_constraint_id);
static int materialize_schema(
    sqlite3_stmt *statement,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int materialize_table(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int materialize_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_flags(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_defaults(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_generated(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_column_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
);
static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
);
static int materialize_index_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_column_descriptor *out_index_column
);
static int materialize_foreign_key(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
);
static int materialize_foreign_key_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
);
static int materialize_check_constraint(
    sqlite3_stmt *statement,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
);
static int insert_index_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    const int64_t *prefix_length,
    enum mylite_catalog_index_sort_direction sort_direction
);
static int insert_foreign_key_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    int64_t child_column_id,
    int64_t parent_column_id,
    int64_t ordinal_position,
    int64_t position_in_unique_constraint
);
static int delete_foreign_keys_for_related_table(sqlite3 *sqlite, int64_t table_id);
static int delete_foreign_keys_for_schema(sqlite3 *sqlite, int64_t schema_id);
static int delete_check_constraints_for_table(sqlite3 *sqlite, int64_t table_id);
static int delete_check_constraints_for_schema(sqlite3 *sqlite, int64_t schema_id);
static int read_inserted_foreign_key(
    struct mylite_db *database,
    int64_t child_table_id,
    const char *name,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
);
static int read_inserted_foreign_key_column(
    struct mylite_db *database,
    int64_t foreign_key_id,
    int64_t ordinal_position,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
);
static int read_inserted_check_constraint(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
);
static int read_inserted_index_column(
    struct mylite_db *database,
    int64_t index_id,
    int64_t ordinal_position,
    struct mylite_catalog_index_column_descriptor *out_index_column
);
static int checked_column_i64(sqlite3_stmt *statement, int index, int64_t *out_value);
static int checked_column_u64(sqlite3_stmt *statement, int index, uint64_t *out_value);
static int checked_nullable_column_i64(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    int64_t *out_value
);
static int checked_nullable_column_text(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    char *destination,
    size_t destination_size
);
static int checked_column_text(
    sqlite3_stmt *statement,
    int index,
    char *destination,
    size_t destination_size
);
static int validate_database(struct mylite_db *database);
static int validate_catalog_ready_database(struct mylite_db *database);
static int validate_required_name(const char *name, size_t capacity);
static int validate_optional_name(const char *name, size_t capacity);
static int validate_logical_object_name(const char *name, size_t capacity);
static int validate_table_kind(enum mylite_catalog_table_kind kind);
static int validate_column_default_kind(enum mylite_catalog_column_default_kind kind);
static int validate_catalog_column_values(
    const struct catalog_column_values *values,
    bool use_logical_object_name
);
static int validate_catalog_column_default_value(const struct catalog_column_values *values);
static int validate_catalog_generated_column_value(const struct catalog_column_values *values);
static int validate_catalog_current_timestamp_default_value(
    const struct catalog_column_values *values
);
static int validate_catalog_current_date_default_value(const struct catalog_column_values *values);
static int validate_catalog_current_time_default_value(const struct catalog_column_values *values);
static int catalog_default_text_length(const char *default_text, size_t *out_text_length);
static int validate_catalog_text_default_value(
    const struct catalog_column_values *values,
    size_t text_length
);
static bool catalog_default_kind_stores_integer(
    enum mylite_catalog_column_default_kind default_kind
);
static bool catalog_default_kind_stores_text(enum mylite_catalog_column_default_kind default_kind);
static bool catalog_logical_type_accepts_integer_expression_default(const char *logical_type);
static bool catalog_logical_type_accepts_text_expression_default(const char *logical_type);
static bool catalog_logical_type_accepts_current_timestamp(const char *logical_type);
static bool catalog_logical_type_accepts_current_date(const char *logical_type);
static bool catalog_logical_type_accepts_current_time(const char *logical_type);
static bool catalog_logical_type_accepts_text_default(const char *logical_type);
static bool catalog_logical_type_accepts_binary_default(const char *logical_type);
static bool catalog_logical_type_is_binary_blob_family(const char *logical_type);
static bool catalog_default_text_is_hex(const char *text, size_t text_length);
static bool catalog_logical_type_is_bit(const char *logical_type);
static bool catalog_logical_type_is_text_family(const char *logical_type);
static bool catalog_logical_type_accepts_empty_text_default(const char *logical_type);
static bool catalog_logical_type_equals(const char *logical_type, const char *expected);
static int validate_catalog_bool_i64(int64_t value, bool *out_bool);
static int validate_index_kind(enum mylite_catalog_index_kind kind);
static int validate_active_mutation(const struct mylite_catalog_mutation *mutation);
static int validate_insert_index_request(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    const char *comment
);
static int insert_index_descriptor_row(
    sqlite3 *sqlite,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    bool is_visible,
    const char *comment,
    bool show_create_explicit_btree
);
static int read_inserted_index_if_requested(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index
);
static int validate_catalog_column_reorder_request(
    const struct mylite_catalog_column_reorder *reorder
);
static int validate_positive_id(int64_t id);
static int validate_positive_ordinal(int64_t ordinal_position);
static int validate_generation(uint64_t generation);
static int validate_schema_callback(mylite_catalog_schema_callback callback);
static int validate_callback(mylite_catalog_table_callback callback);
static int validate_column_callback(mylite_catalog_column_callback callback);
static int validate_index_callback(mylite_catalog_index_callback callback);
static int validate_index_column_callback(mylite_catalog_index_column_callback callback);
static int validate_foreign_key_callback(mylite_catalog_foreign_key_callback callback);
static int validate_foreign_key_column_callback(
    mylite_catalog_foreign_key_column_callback callback
);
static int validate_check_constraint_callback(mylite_catalog_check_constraint_callback callback);
static int u64_to_i64(uint64_t value, int64_t *out_value);
static int i64_to_u32(int64_t value, uint32_t *out_value);
static int i64_to_u64(int64_t value, uint64_t *out_value);
static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);
static void reset_descriptor_cache_state(struct mylite_catalog *catalog);

static const char *catalog_state_table_name(void);
static const char *catalog_schemas_table_name(void);
static const char *catalog_tables_table_name(void);
static const char *catalog_columns_table_name(void);
static const char *catalog_indexes_table_name(void);
static const char *catalog_index_columns_table_name(void);
static const char *catalog_foreign_keys_table_name(void);
static const char *catalog_foreign_key_columns_table_name(void);
static const char *catalog_check_constraints_table_name(void);

void mylite_catalog_init(struct mylite_catalog *catalog) {
    if (catalog == NULL) {
        return;
    }

    memset(catalog, 0, sizeof(*catalog));
}

void mylite_catalog_deinit(struct mylite_catalog *catalog) {
    if (catalog == NULL) {
        return;
    }

    memset(catalog, 0, sizeof(*catalog));
}

int mylite_catalog_initialize_file_backed(struct mylite_db *database) {
    int rc = MYLITE_OK;

    rc = validate_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    mylite_catalog_deinit(&database->catalog);

    rc = ensure_catalog_schema(database);
    if (rc != MYLITE_OK) {
        mylite_catalog_deinit(&database->catalog);
        return rc;
    }

    database->catalog.initialized = true;
    reset_descriptor_cache_state(&database->catalog);
    database->session.catalog_generation = database->catalog.generation;

    return MYLITE_OK;
}

void mylite_catalog_invalidate_descriptor_cache(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    reset_descriptor_cache_state(&database->catalog);
}

void mylite_catalog_mutation_init(struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL) {
        return;
    }

    *mutation = (struct mylite_catalog_mutation){.active = false, .next_generation = 0U};
}

void mylite_catalog_mutation_deinit(struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL) {
        return;
    }

    *mutation = (struct mylite_catalog_mutation){.active = false, .next_generation = 0U};
}

int mylite_catalog_begin_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
) {
    struct mylite_catalog catalog = {.initialized = false};
    int rc = MYLITE_OK;

    if (mutation != NULL) {
        mylite_catalog_mutation_init(mutation);
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mutation == NULL) {
        return MYLITE_MISUSE;
    }

    rc = begin_catalog_transaction(database->sqlite);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_catalog_state(database->sqlite, &catalog);
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }
    if (catalog.generation == UINT64_MAX) {
        rollback_catalog_transaction(database->sqlite);
        return MYLITE_ERROR;
    }

    mutation->active = true;
    mutation->next_generation = catalog.generation + 1U;

    return MYLITE_OK;
}

int mylite_catalog_commit_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = update_catalog_generation(database->sqlite, mutation->next_generation);
    if (rc == MYLITE_OK) {
        rc = commit_catalog_transaction(database->sqlite);
    }
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        mylite_catalog_mutation_deinit(mutation);
        return rc;
    }

    database->catalog.generation = mutation->next_generation;
    database->session.catalog_generation = mutation->next_generation;
    reset_descriptor_cache_state(&database->catalog);
    mylite_catalog_mutation_deinit(mutation);

    return MYLITE_OK;
}

void mylite_catalog_rollback_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
) {
    if (database != NULL && mutation != NULL && mutation->active) {
        rollback_catalog_transaction(database->sqlite);
    }
    mylite_catalog_mutation_deinit(mutation);
}

uint64_t mylite_catalog_mutation_generation(const struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL || !mutation->active) {
        return 0U;
    }

    return mutation->next_generation;
}

int mylite_catalog_allocate_table_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_table_id
) {
    int rc = validate_catalog_ready_database(database);

    if (out_table_id != NULL) {
        *out_table_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table_id == NULL) {
        return MYLITE_MISUSE;
    }

    return read_next_table_id(database->sqlite, out_table_id);
}

int mylite_catalog_allocate_index_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_index_id
) {
    int rc = validate_catalog_ready_database(database);

    if (out_index_id != NULL) {
        *out_index_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index_id == NULL) {
        return MYLITE_MISUSE;
    }

    return read_next_index_id(database->sqlite, out_index_id);
}

int mylite_catalog_allocate_foreign_key_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_foreign_key_id
) {
    int rc = validate_catalog_ready_database(database);

    if (out_foreign_key_id != NULL) {
        *out_foreign_key_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_foreign_key_id == NULL) {
        return MYLITE_MISUSE;
    }

    return read_next_foreign_key_id(database->sqlite, out_foreign_key_id);
}

int mylite_catalog_allocate_check_constraint_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_check_constraint_id
) {
    int rc = MYLITE_OK;

    if (out_check_constraint_id != NULL) {
        *out_check_constraint_id = 0;
    }
    rc = validate_catalog_ready_database(database);
    if (rc == MYLITE_OK) {
        rc = validate_active_mutation(mutation);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_check_constraint_id == NULL) {
        return MYLITE_MISUSE;
    }

    return read_next_check_constraint_id(database->sqlite, out_check_constraint_id);
}

int mylite_catalog_insert_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    int64_t auto_increment_next,
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
    struct mylite_catalog_table_descriptor *out_table
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }
    rc = validate_catalog_table_descriptor_input(&(const struct catalog_table_descriptor_input){
        .schema_id = schema_id,
        .name = name,
        .physical_name = physical_name,
        .kind = kind,
        .default_charset = default_charset,
        .default_collation = default_collation,
        .comment = comment,
        .created_time_utc_epoch = created_time_utc_epoch,
        .updated_time_utc_epoch = updated_time_utc_epoch,
    });
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(table_id, schema_id, name, kind, physical_name, auto_increment_next, default_charset, "
        "default_collation, comment, fulltext_doc_id_initialized, created_time_utc_epoch, "
        "updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 0, ?10, ?11, 1, ?12, ?12)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_in_mutation_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_in_mutation_schema_id_bind, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_in_mutation_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_in_mutation_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_table_insert_in_mutation_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_table_insert_in_mutation_auto_increment_next_bind,
            auto_increment_next
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_table_insert_in_mutation_default_charset_bind,
            default_charset
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_table_insert_in_mutation_default_collation_bind,
            default_collation
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_in_mutation_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_table_insert_in_mutation_created_time_bind,
            created_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_table_insert_in_mutation_updated_time_bind,
            updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(
            statement,
            catalog_table_insert_in_mutation_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_id(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_insert_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = is_visible,
        .is_auto_increment = is_auto_increment,
        .default_kind = default_kind,
        .default_integer = default_integer,
        .default_text = default_text,
        .on_update_current_timestamp = on_update_current_timestamp,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
        .comment = comment == NULL ? "" : comment,
        .is_generated = is_generated,
        .generated_kind = generated_kind,
        .generation_expression = generation_expression == NULL ? "" : generation_expression,
        .sqlite_generation_expression =
            sqlite_generation_expression == NULL ? "" : sqlite_generation_expression,
    };
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_values(&values, false);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_columns "
        "(table_id, ordinal_position, name, logical_type, physical_type, is_nullable, "
        "is_visible, is_auto_increment, default_kind, default_integer, default_text, "
        "on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, "
        "?16, ?17, ?18, ?19, 1, ?20, ?20)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_insert_values(
            statement,
            table_id,
            ordinal_position,
            &values,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_column != NULL) {
        return read_column_by_name(database->sqlite, table_id, name, out_column);
    }

    return MYLITE_OK;
}

int mylite_catalog_insert_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    bool is_visible,
    const char *comment,
    bool show_create_explicit_btree,
    struct mylite_catalog_index_descriptor *out_index
) {
    int rc = MYLITE_OK;

    if (out_index != NULL) {
        *out_index = (struct mylite_catalog_index_descriptor){0};
    }
    rc = validate_insert_index_request(
        database,
        mutation,
        index_id,
        table_id,
        name,
        physical_name,
        kind,
        comment == NULL ? "" : comment
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = insert_index_descriptor_row(
        database->sqlite,
        mutation,
        index_id,
        table_id,
        name,
        physical_name,
        kind,
        is_unique,
        is_visible,
        comment == NULL ? "" : comment,
        show_create_explicit_btree
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        rc = mark_table_fulltext_doc_id_initialized_in_mutation(database, mutation, table_id);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return read_inserted_index_if_requested(database->sqlite, table_id, out_index);
}

static int validate_insert_index_request(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    const char *comment
) {
    struct mylite_catalog_table_descriptor table = {0};
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_index_kind(kind);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_optional_name(comment, MYLITE_CATALOG_INDEX_COMMENT_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_table_by_id(database->sqlite, table_id, &table);
}

static int insert_index_descriptor_row(
    sqlite3 *sqlite,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    bool is_visible,
    const char *comment,
    bool show_create_explicit_btree
) {
    sqlite3_stmt *statement = NULL;
    int64_t unique_value = catalog_bool_value(is_unique);
    int64_t visible_value = catalog_bool_value(is_visible);
    int64_t explicit_btree_value = catalog_bool_value(show_create_explicit_btree);
    int rc = prepare_statement(
        sqlite,
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, ?10, ?10)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_index_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_is_unique_bind, unique_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_insert_is_visible_bind, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_index_insert_physical_name_bind, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_index_insert_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_index_insert_show_create_explicit_btree_bind,
            explicit_btree_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_index_insert_generation_bind, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

static int read_inserted_index_if_requested(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index
) {
    bool found = false;
    int rc = MYLITE_OK;

    if (out_index == NULL) {
        return MYLITE_OK;
    }

    rc = try_read_primary_index_by_table_id(sqlite, table_id, out_index, &found);
    if (rc == MYLITE_OK && !found) {
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int mark_table_fulltext_doc_id_initialized_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_table_by_id(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (table.fulltext_doc_id_initialized) {
        return MYLITE_OK;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET fulltext_doc_id_initialized = 1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?1 "
        "WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_insert_index_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    const int64_t *prefix_length,
    enum mylite_catalog_index_sort_direction sort_direction,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    int rc = MYLITE_OK;

    if (out_index_column != NULL) {
        *out_index_column = (struct mylite_catalog_index_column_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (prefix_length != NULL) {
        rc = validate_positive_ordinal(*prefix_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (sort_direction != MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC &&
        sort_direction != MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
        return MYLITE_MISUSE;
    }

    rc = insert_index_column_row(
        database,
        mutation,
        index_id,
        table_id,
        column_id,
        ordinal_position,
        prefix_length,
        sort_direction
    );
    if (rc != MYLITE_OK || out_index_column == NULL) {
        return rc;
    }

    return read_inserted_index_column(database, index_id, ordinal_position, out_index_column);
}

static int insert_index_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    const int64_t *prefix_length,
    enum mylite_catalog_index_sort_direction sort_direction
) {
    sqlite3_stmt *statement = NULL;
    bool has_prefix_length = prefix_length != NULL;
    int64_t prefix_length_value = 0;
    int rc = MYLITE_OK;

    if (has_prefix_length) {
        prefix_length_value = *prefix_length;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_index_columns "
        "(index_id, table_id, column_id, ordinal_position, prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, ?7, ?7)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_column_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_column_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_index_column_insert_column_id_bind, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_index_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_nullable_i64(
            statement,
            catalog_index_column_insert_prefix_length_bind,
            has_prefix_length,
            prefix_length_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_index_column_insert_sort_direction_bind,
            (int64_t)sort_direction
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(
            statement,
            catalog_index_column_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

static int read_inserted_index_column(
    struct mylite_db *database,
    int64_t index_id,
    int64_t ordinal_position,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 AND ordinal_position = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index_column(statement, out_index_column);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_insert_foreign_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    const char *name,
    int64_t parent_index_id,
    int64_t child_index_id,
    const char *update_rule,
    const char *delete_rule,
    const char *match_option,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_foreign_key != NULL) {
        *out_foreign_key = (struct mylite_catalog_foreign_key_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(foreign_key_id);
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(parent_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(parent_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(child_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(update_rule, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(delete_rule, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(match_option, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_foreign_keys "
        "(foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, ?10, ?10)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_foreign_key_insert_foreign_key_id_bind, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_foreign_key_insert_child_table_id_bind, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_foreign_key_insert_parent_table_id_bind, parent_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_foreign_key_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_foreign_key_insert_parent_index_id_bind, parent_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_foreign_key_insert_child_index_id_bind, child_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_foreign_key_insert_update_rule_bind, update_rule);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_foreign_key_insert_delete_rule_bind, delete_rule);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_foreign_key_insert_match_option_bind, match_option);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(
            statement,
            catalog_foreign_key_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK || out_foreign_key == NULL) {
        return rc;
    }

    return read_inserted_foreign_key(database, child_table_id, name, out_foreign_key);
}

int mylite_catalog_insert_foreign_key_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    int64_t child_column_id,
    int64_t parent_column_id,
    int64_t ordinal_position,
    int64_t position_in_unique_constraint,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    int rc = MYLITE_OK;

    if (out_foreign_key_column != NULL) {
        *out_foreign_key_column = (struct mylite_catalog_foreign_key_column_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(foreign_key_id);
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(parent_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(child_column_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(parent_column_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_ordinal(ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_ordinal(position_in_unique_constraint);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = insert_foreign_key_column_row(
        database,
        mutation,
        foreign_key_id,
        child_table_id,
        parent_table_id,
        child_column_id,
        parent_column_id,
        ordinal_position,
        position_in_unique_constraint
    );
    if (rc != MYLITE_OK || out_foreign_key_column == NULL) {
        return rc;
    }

    return read_inserted_foreign_key_column(
        database,
        foreign_key_id,
        ordinal_position,
        out_foreign_key_column
    );
}

int mylite_catalog_insert_check_constraint_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t check_constraint_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    const char *check_clause,
    const char *sqlite_expression,
    bool is_enforced,
    bool name_is_generated,
    int64_t generated_ordinal,
    int64_t ordinal_position,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_check_constraint != NULL) {
        *out_check_constraint = (struct mylite_catalog_check_constraint_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc == MYLITE_OK) {
        rc = validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(check_clause, MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(sqlite_expression, MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_ordinal(generated_ordinal);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_ordinal(ordinal_position);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_check_constraints "
        "(check_constraint_id, table_id, name, physical_name, check_clause, sqlite_expression, "
        "is_enforced, name_is_generated, generated_ordinal, ordinal_position, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_check_constraint_insert_check_constraint_id_bind,
            check_constraint_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_check_constraint_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_check_constraint_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc =
            bind_text(statement, catalog_check_constraint_insert_physical_name_bind, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_check_constraint_insert_check_clause_bind, check_clause);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_check_constraint_insert_sqlite_expression_bind,
            sqlite_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_check_constraint_insert_is_enforced_bind,
            catalog_bool_value(is_enforced)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_check_constraint_insert_name_is_generated_bind,
            catalog_bool_value(name_is_generated)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_check_constraint_insert_generated_ordinal_bind,
            generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_check_constraint_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(
            statement,
            catalog_check_constraint_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK || out_check_constraint == NULL) {
        return rc;
    }

    return read_inserted_check_constraint(database, table_id, name, out_check_constraint);
}

static int insert_foreign_key_column_row(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    int64_t child_column_id,
    int64_t parent_column_id,
    int64_t ordinal_position,
    int64_t position_in_unique_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_foreign_key_columns "
        "(foreign_key_id, child_table_id, parent_table_id, child_column_id, "
        "parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 1, ?8, ?8)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_foreign_key_column_insert_foreign_key_id_bind,
            foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_foreign_key_column_insert_child_table_id_bind,
            child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_foreign_key_column_insert_parent_table_id_bind,
            parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_foreign_key_column_insert_child_column_id_bind,
            child_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_foreign_key_column_insert_parent_column_id_bind,
            parent_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_foreign_key_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_foreign_key_column_insert_position_in_unique_constraint_bind,
            position_in_unique_constraint
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(
            statement,
            catalog_foreign_key_column_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

static int read_inserted_foreign_key(
    struct mylite_db *database,
    int64_t child_table_id,
    const char *name,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_foreign_key(statement, out_foreign_key);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return finalize_statement(statement, rc);
}

static int read_inserted_foreign_key_column(
    struct mylite_db *database,
    int64_t foreign_key_id,
    int64_t ordinal_position,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 AND ordinal_position = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_foreign_key_column(statement, out_foreign_key_column);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return finalize_statement(statement, rc);
}

static int read_inserted_check_constraint(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_check_constraint(statement, out_check_constraint);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            if (sqlite_rc == SQLITE_DONE) {
                rc = MYLITE_ERROR;
            }
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(index_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1 AND index_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1 AND index_id = ?2",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_index_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    int64_t column_id,
    int64_t ordinal_position
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(index_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(column_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_ordinal(ordinal_position);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns "
        "WHERE table_id = ?1 AND index_id = ?2 AND column_id = ?3 AND ordinal_position = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "UPDATE _mylite_catalog_index_columns "
            "SET ordinal_position = ordinal_position - 1, "
            "descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND index_id = ?3 AND ordinal_position > ?4",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "UPDATE _mylite_catalog_indexes "
            "SET descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND index_id = ?3",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_rename_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    const char *name
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(index_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_indexes "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND index_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_set_index_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    bool is_visible
) {
    sqlite3_stmt *statement = NULL;
    int64_t visible_value = catalog_bool_value(is_visible);
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(index_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_indexes "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND index_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_keys_for_child_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns WHERE child_table_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id,
    int64_t foreign_key_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(foreign_key_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id = ?1 AND foreign_key_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id = ?1 AND foreign_key_id = ?2",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_check_constraints_for_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    int rc = validate_catalog_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return delete_check_constraints_for_table(database->sqlite, table_id);
}

int mylite_catalog_delete_check_constraint_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t check_constraint_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(check_constraint_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_check_constraints "
        "WHERE table_id = ?1 AND check_constraint_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_update_check_constraint_enforcement_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t check_constraint_id,
    bool is_enforced
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(check_constraint_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_check_constraints "
        "SET is_enforced = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND check_constraint_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, catalog_bool_value(is_enforced));
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_rename_generated_check_constraints_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *table_name
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_logical_object_name(table_name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_check_constraints "
        "SET name = ?1 || '_chk_' || generated_ordinal, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND name_is_generated = 1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, table_name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_related_table(database->sqlite, table_id);
    if (rc == MYLITE_OK) {
        rc = delete_check_constraints_for_table(database->sqlite, table_id);
    }

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1 AND column_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "UPDATE _mylite_catalog_columns "
            "SET ordinal_position = ordinal_position - 1, "
            "descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND ordinal_position > ?3",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_rename_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_replace_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression
) {
    const struct catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = is_visible,
        .is_auto_increment = is_auto_increment,
        .default_kind = default_kind,
        .default_integer = default_integer,
        .default_text = default_text,
        .on_update_current_timestamp = on_update_current_timestamp,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
        .comment = comment == NULL ? "" : comment,
        .is_generated = is_generated,
        .generated_kind = generated_kind,
        .generation_expression = generation_expression == NULL ? "" : generation_expression,
        .sqlite_generation_expression =
            sqlite_generation_expression == NULL ? "" : sqlite_generation_expression,
    };
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_values(&values, true);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, logical_type = ?2, physical_type = ?3, is_nullable = ?4, "
        "is_visible = ?5, is_auto_increment = ?6, default_kind = ?7, default_integer = ?8, "
        "default_text = ?9, on_update_current_timestamp = ?10, character_set_name = ?11, "
        "collation_name = ?12, comment = ?13, is_generated = ?14, generated_kind = ?15, "
        "generation_expression = ?16, sqlite_generation_expression = ?17, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?18 "
        "WHERE table_id = ?19 AND column_id = ?20",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_replace_values(
            statement,
            table_id,
            column_id,
            &values,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_reorder_columns_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_reorder_request(reorder);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = offset_catalog_column_ordinals_for_reorder(database, reorder);
    for (size_t index = 0U; rc == MYLITE_OK && index < reorder->column_count; ++index) {
        rc = apply_catalog_column_reorder(database, mutation, reorder, index);
    }

    return rc;
}

static int offset_catalog_column_ordinals_for_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_column_reorder *reorder
) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET ordinal_position = ordinal_position + ?1 WHERE table_id = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc =
            bind_i64(statement, catalog_column_reorder_offset_bind, (int64_t)reorder->column_count);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_reorder_offset_table_id_bind, reorder->table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK &&
        sqlite3_changes64(database->sqlite) != (sqlite3_int64)reorder->column_count) {
        rc = MYLITE_ERROR;
    }

    return finalize_statement(statement, rc);
}

static int apply_catalog_column_reorder(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder,
    size_t column_index
) {
    const struct mylite_catalog_column_descriptor *column = &reorder->columns[column_index];
    int64_t final_ordinal = (int64_t)column_index + 1;
    bool ordinal_changed = column->ordinal_position != final_ordinal;
    int64_t increment_descriptor_version =
        (ordinal_changed && column->column_id != reorder->metadata_replaced_column_id) ? 1 : 0;
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (column->table_id != reorder->table_id) {
        return MYLITE_MISUSE;
    }
    rc = validate_positive_id(column->column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(final_ordinal);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET ordinal_position = ?1, "
        "descriptor_version = descriptor_version + ?2, "
        "updated_catalog_generation = CASE WHEN ?2 <> 0 THEN ?3 "
        "ELSE updated_catalog_generation END "
        "WHERE table_id = ?4 AND column_id = ?5",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_reorder_ordinal_bind, final_ordinal);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_reorder_version_increment_bind,
            increment_descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_column_reorder_generation_bind, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_reorder_table_id_bind, reorder->table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_reorder_column_id_bind, column->column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

static int validate_catalog_column_reorder_request(
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = MYLITE_OK;

    if (reorder == NULL || reorder->columns == NULL || reorder->column_count == 0U ||
        reorder->column_count > (size_t)(INT64_MAX / 2)) {
        return MYLITE_MISUSE;
    }
    rc = validate_positive_id(reorder->table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(reorder->metadata_replaced_column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return MYLITE_OK;
}

int mylite_catalog_set_column_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    bool is_visible
) {
    sqlite3_stmt *statement = NULL;
    int64_t visible_value = 0;
    int rc = validate_catalog_ready_database(database);

    if (is_visible) {
        visible_value = 1;
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_delete_schema_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_schema(database->sqlite, schema_id);
    if (rc == MYLITE_OK) {
        rc = delete_check_constraints_for_schema(database->sqlite, schema_id);
    }

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }

    return finalize_statement(statement, rc);
}

static int delete_foreign_keys_for_related_table(sqlite3 *sqlite, int64_t table_id) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id = ?1 OR parent_table_id = ?1",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id = ?1 OR parent_table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

static int delete_foreign_keys_for_schema(sqlite3 *sqlite, int64_t schema_id) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id IN ("
        "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
        ") OR parent_table_id IN ("
        "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
        ")",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ") OR parent_table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

static int delete_check_constraints_for_table(sqlite3 *sqlite, int64_t table_id) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_check_constraints WHERE table_id = ?1",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

static int delete_check_constraints_for_schema(sqlite3 *sqlite, int64_t schema_id) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_check_constraints "
        "WHERE table_id IN (SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_update_table_identity_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET schema_id = ?1, name = ?2, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?3 "
        "WHERE table_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_id(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_default_charset_collation_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(default_collation, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET default_charset = ?1, default_collation = ?2, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?3 "
        "WHERE table_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_id(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_comment_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *comment,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_optional_name(comment, MYLITE_CATALOG_TABLE_COMMENT_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET comment = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, comment);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_id(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_schema_default_charset_collation_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_schema != NULL) {
        *out_schema = (struct mylite_catalog_schema_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(default_collation, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_schemas "
        "SET default_charset = ?1, default_collation = ?2, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?3 "
        "WHERE schema_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 4, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_schema != NULL) {
        return read_schema_by_id(database->sqlite, schema_id, out_schema);
    }

    return MYLITE_OK;
}

int mylite_catalog_update_table_auto_increment_next(
    struct mylite_db *database,
    int64_t table_id,
    int64_t auto_increment_next
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET auto_increment_next = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, auto_increment_next);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_invalidate_descriptor_cache(database);
    }
    return rc;
}

int mylite_catalog_update_table_updated_time(
    struct mylite_db *database,
    int64_t table_id,
    int64_t updated_time_utc_epoch
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (updated_time_utc_epoch < 0) {
        return MYLITE_ERROR;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET updated_time_utc_epoch = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, updated_time_utc_epoch);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        mylite_catalog_invalidate_descriptor_cache(database);
    }
    return rc;
}

int mylite_catalog_for_each_schema(
    struct mylite_db *database,
    mylite_catalog_schema_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_schema_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation FROM _mylite_catalog_schemas ORDER BY name",
        &statement
    );
    while (rc == MYLITE_OK) {
        struct mylite_catalog_schema_descriptor schema = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_schema(statement, &schema);
        if (rc == MYLITE_OK) {
            rc = callback(&schema, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_table_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_table_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_table_descriptor table = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_table(statement, &table);
        if (rc == MYLITE_OK) {
            rc = callback(&table, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_column_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_column_descriptor column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_column(statement, &column);
        if (rc == MYLITE_OK) {
            rc = callback(&column, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_index_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_index_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 ORDER BY index_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_descriptor index = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_index(statement, &index);
        if (rc == MYLITE_OK) {
            rc = callback(&index, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_column_in_index(
    struct mylite_db *database,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_index_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, index_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_index_column_descriptor index_column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_index_column(statement, &index_column);
        if (rc == MYLITE_OK) {
            rc = callback(&index_column, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_in_child_table(
    struct mylite_db *database,
    int64_t child_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, child_table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_descriptor foreign_key = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key(statement, &foreign_key);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_for_parent_table(
    struct mylite_db *database,
    int64_t parent_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(parent_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE parent_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, parent_table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_descriptor foreign_key = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key(statement, &foreign_key);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_column_in_foreign_key(
    struct mylite_db *database,
    int64_t foreign_key_id,
    mylite_catalog_foreign_key_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(foreign_key_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_foreign_key_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, foreign_key_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_foreign_key_column_descriptor foreign_key_column = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_foreign_key_column(statement, &foreign_key_column);
        if (rc == MYLITE_OK) {
            rc = callback(&foreign_key_column, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_check_constraint_descriptor check_constraint = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_check_constraint(statement, &check_constraint);
        if (rc == MYLITE_OK) {
            rc = callback(&check_constraint, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = validate_catalog_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = validate_positive_id(schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT c.check_constraint_id, c.table_id, c.name, c.physical_name, c.check_clause, "
        "c.sqlite_expression, c.is_enforced, c.name_is_generated, c.generated_ordinal, "
        "c.ordinal_position, c.descriptor_version, c.created_catalog_generation, "
        "c.updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints c "
        "JOIN _mylite_catalog_tables t ON t.table_id = c.table_id "
        "WHERE t.schema_id = ?1 ORDER BY c.name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    while (rc == MYLITE_OK) {
        struct mylite_catalog_check_constraint_descriptor check_constraint = {0};

        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }

        rc = materialize_check_constraint(statement, &check_constraint);
        if (rc == MYLITE_OK) {
            rc = callback(&check_constraint, user_data);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_try_read_check_constraint_by_physical_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_catalog_ready_database(database);

    if (out_check_constraint != NULL) {
        *out_check_constraint = (struct mylite_catalog_check_constraint_descriptor){0};
    }
    if (out_found != NULL) {
        *out_found = false;
    }
    if (rc == MYLITE_OK && (out_check_constraint == NULL || out_found == NULL)) {
        return MYLITE_MISUSE;
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints "
        "WHERE table_id = ?1 AND physical_name = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, physical_name);
    }
    if (rc == MYLITE_OK) {
        int sqlite_rc = sqlite3_step(statement);

        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_check_constraint(statement, out_check_constraint);
            *out_found = rc == MYLITE_OK;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

int mylite_catalog_try_read_primary_index_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_primary_index_by_table_id(database->sqlite, table_id, out_index, out_found);
}

int mylite_catalog_create_schema(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return mylite_catalog_create_schema_with_defaults(
        database,
        name,
        MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
        MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
        out_schema
    );
}

int mylite_catalog_create_schema_with_defaults(
    struct mylite_db *database,
    const char *name,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_schema != NULL) {
        *out_schema = (struct mylite_catalog_schema_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(default_collation, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_schemas "
        "(name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, 1, ?4, ?4)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 3, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 4, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_schema != NULL) {
        return read_schema_by_name(database->sqlite, name, out_schema);
    }

    return MYLITE_OK;
}

int mylite_catalog_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    bool found = false;
    int rc = mylite_catalog_try_read_schema_by_name(database, name, out_schema, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_catalog_try_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_schema == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_schema_by_name(database->sqlite, name, out_schema, out_found);
}

int mylite_catalog_delete_schema(struct mylite_db *database, int64_t schema_id) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_schema(database->sqlite, schema_id);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_create_table(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
    struct mylite_catalog_table_descriptor *out_table
) {
    struct catalog_generation_change generation = {0};
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_table_descriptor_input(&(const struct catalog_table_descriptor_input){
        .schema_id = schema_id,
        .name = name,
        .physical_name = physical_name,
        .kind = kind,
        .default_charset = default_charset,
        .default_collation = default_collation,
        .comment = comment,
        .created_time_utc_epoch = created_time_utc_epoch,
        .updated_time_utc_epoch = updated_time_utc_epoch,
    });
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(schema_id, name, kind, physical_name, auto_increment_next, default_charset, "
        "default_collation, comment, fulltext_doc_id_initialized, created_time_utc_epoch, "
        "updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 0, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_schema_id_bind, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_physical_name_bind, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_auto_increment_next_bind, 1);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_default_charset_bind, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_default_collation_bind, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_table_insert_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_created_time_bind, created_time_utc_epoch);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_table_insert_updated_time_bind, updated_time_utc_epoch);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_table_insert_generation_bind, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_name(database->sqlite, schema_id, name, out_table);
    }

    return MYLITE_OK;
}

static int validate_catalog_table_descriptor_input(
    const struct catalog_table_descriptor_input *input
) {
    int rc = input == NULL ? MYLITE_MISUSE : validate_positive_id(input->schema_id);

    if (rc == MYLITE_OK) {
        rc = validate_logical_object_name(input->name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(input->physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_kind(input->kind);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(input->default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_required_name(input->default_collation, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = validate_optional_name(input->comment, MYLITE_CATALOG_TABLE_COMMENT_CAPACITY);
    }
    if (rc == MYLITE_OK &&
        (input->created_time_utc_epoch < 0 || input->updated_time_utc_epoch < 0)) {
        rc = MYLITE_ERROR;
    }

    return rc;
}

int mylite_catalog_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found = false;
    int rc = mylite_catalog_try_read_table_by_name(database, schema_id, name, out_table, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_catalog_try_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_table_by_name(database->sqlite, schema_id, name, out_table, out_found);
}

int mylite_catalog_read_table_by_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table == NULL) {
        return MYLITE_MISUSE;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_table_by_id(database->sqlite, table_id, out_table);
}

int mylite_catalog_update_table_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name
) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 2, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_table(struct mylite_db *database, int64_t table_id) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_related_table(database->sqlite, table_id);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_create_column(
    struct mylite_db *database,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    const char *comment,
    bool is_generated,
    enum mylite_catalog_generated_column_kind generated_kind,
    const char *generation_expression,
    const char *sqlite_generation_expression,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct catalog_column_values values = {
        .name = name,
        .logical_type = logical_type,
        .physical_type = physical_type,
        .is_nullable = is_nullable,
        .is_visible = true,
        .is_auto_increment = false,
        .default_kind = default_kind,
        .default_integer = default_integer,
        .default_text = default_text,
        .on_update_current_timestamp = on_update_current_timestamp,
        .character_set_name = character_set_name,
        .collation_name = collation_name,
        .comment = comment == NULL ? "" : comment,
        .is_generated = is_generated,
        .generated_kind = generated_kind,
        .generation_expression = generation_expression == NULL ? "" : generation_expression,
        .sqlite_generation_expression =
            sqlite_generation_expression == NULL ? "" : sqlite_generation_expression,
    };
    struct catalog_generation_change generation = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_values(&values, false);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = read_table_by_id(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_columns "
        "(table_id, ordinal_position, name, logical_type, physical_type, is_nullable, "
        "is_visible, is_auto_increment, default_kind, default_integer, default_text, "
        "on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, "
        "?16, ?17, ?18, ?19, 1, ?20, ?20)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_insert_values(
            statement,
            table_id,
            ordinal_position,
            &values,
            generation.next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    if (out_column != NULL) {
        return read_column_by_name(database->sqlite, table_id, name, out_column);
    }

    return MYLITE_OK;
}

int mylite_catalog_read_column_by_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = validate_catalog_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    rc = validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_column_by_name(database->sqlite, table_id, name, out_column);
}

int mylite_catalog_delete_column(struct mylite_db *database, int64_t column_id) {
    struct catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = validate_catalog_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE column_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(database->sqlite);
    }
    rc = finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

bool mylite_catalog_name_is_reserved(const char *name) {
    static const char prefix[] = "_mylite_";

    if (name == NULL) {
        return false;
    }

    return text_has_ascii_case_insensitive_prefix(name, prefix) != 0;
}

static int ensure_catalog_schema(struct mylite_db *database) {
    int table_count = 0;
    int rc = existing_catalog_table_count(database->sqlite, &table_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (table_count == 0) {
        return initialize_catalog_schema(database);
    }
    if (table_count != legacy_catalog_table_count &&
        table_count != pre_foreign_key_catalog_table_count &&
        table_count != downgraded_catalog_with_check_constraint_table_count &&
        table_count != pre_check_constraint_catalog_table_count &&
        table_count != catalog_table_count) {
        return MYLITE_ERROR;
    }

    return load_existing_catalog(database);
}

static int load_existing_catalog(struct mylite_db *database) {
    struct mylite_catalog catalog = {.initialized = false};
    int rc = read_catalog_state(database->sqlite, &catalog);

    if (rc == MYLITE_OK && catalog.schema_version < MYLITE_CATALOG_SCHEMA_VERSION) {
        rc = migrate_catalog_schema(database, &catalog);
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_descriptor_tables(database->sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = read_catalog_state(database->sqlite, &catalog);
    }

    if (rc != MYLITE_OK) {
        return rc;
    }

    return apply_catalog_state(database, &catalog);
}

static int migrate_catalog_schema(
    struct mylite_db *database,
    const struct mylite_catalog *catalog
) {
    uint32_t schema_version = catalog->schema_version;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && schema_version < MYLITE_CATALOG_SCHEMA_VERSION) {
        rc = migrate_catalog_schema_one_step(database->sqlite, &schema_version);
    }
    if (rc == MYLITE_OK && schema_version == MYLITE_CATALOG_SCHEMA_VERSION) {
        return MYLITE_OK;
    }

    return rc == MYLITE_OK ? MYLITE_ERROR : rc;
}

static int migrate_catalog_schema_one_step(sqlite3 *sqlite, uint32_t *schema_version) {
    uint32_t next_schema_version = 0U;
    int rc = MYLITE_ERROR;

    if (schema_version == NULL) {
        return MYLITE_MISUSE;
    }

    switch (*schema_version) {
    case 1U:
        rc = migrate_catalog_schema_v1_to_v2(sqlite);
        next_schema_version = 2U;
        break;
    case 2U:
        rc = migrate_catalog_schema_v2_to_v3(sqlite);
        next_schema_version = 3U;
        break;
    case 3U:
        rc = migrate_catalog_schema_v3_to_v4(sqlite);
        next_schema_version = 4U;
        break;
    case 4U:
        rc = migrate_catalog_schema_v4_to_v5(sqlite);
        next_schema_version = catalog_schema_version_v5;
        break;
    case catalog_schema_version_v5:
        rc = migrate_catalog_schema_v5_to_v6(sqlite);
        next_schema_version = catalog_schema_version_v6;
        break;
    case catalog_schema_version_v6:
        rc = migrate_catalog_schema_v6_to_v7(sqlite);
        next_schema_version = catalog_schema_version_v7;
        break;
    case catalog_schema_version_v7:
        rc = migrate_catalog_schema_v7_to_v8(sqlite);
        next_schema_version = catalog_schema_version_v8;
        break;
    case catalog_schema_version_v8:
        rc = migrate_catalog_schema_v8_to_v9(sqlite);
        next_schema_version = catalog_schema_version_v9;
        break;
    case catalog_schema_version_v9:
        rc = migrate_catalog_schema_v9_to_v10(sqlite);
        next_schema_version = catalog_schema_version_v10;
        break;
    case catalog_schema_version_v10:
        rc = migrate_catalog_schema_v10_to_v11(sqlite);
        next_schema_version = catalog_schema_version_v11;
        break;
    case catalog_schema_version_v11:
        rc = migrate_catalog_schema_v11_to_v12(sqlite);
        next_schema_version = catalog_schema_version_v12;
        break;
    case catalog_schema_version_v12:
        rc = migrate_catalog_schema_v12_to_v13(sqlite);
        next_schema_version = catalog_schema_version_v13;
        break;
    case catalog_schema_version_v13:
        rc = migrate_catalog_schema_v13_to_v14(sqlite);
        next_schema_version = catalog_schema_version_v14;
        break;
    case catalog_schema_version_v14:
        rc = migrate_catalog_schema_v14_to_v15(sqlite);
        next_schema_version = catalog_schema_version_v15;
        break;
    case catalog_schema_version_v15:
        rc = migrate_catalog_schema_v15_to_v16(sqlite);
        next_schema_version = catalog_schema_version_v16;
        break;
    case catalog_schema_version_v16:
        rc = migrate_catalog_schema_v16_to_v17(sqlite);
        next_schema_version = catalog_schema_version_v17;
        break;
    case catalog_schema_version_v17:
        rc = migrate_catalog_schema_v17_to_v18(sqlite);
        next_schema_version = catalog_schema_version_v18;
        break;
    case catalog_schema_version_v18:
        rc = migrate_catalog_schema_v18_to_v19(sqlite);
        next_schema_version = catalog_schema_version_v19;
        break;
    case catalog_schema_version_v19:
        rc = migrate_catalog_schema_v19_to_v20(sqlite);
        next_schema_version = catalog_schema_version_v20;
        break;
    case catalog_schema_version_v20:
        rc = migrate_catalog_schema_v20_to_v21(sqlite);
        next_schema_version = catalog_schema_version_v21;
        break;
    case catalog_schema_version_v21:
        rc = migrate_catalog_schema_v21_to_v22(sqlite);
        next_schema_version = catalog_schema_version_v22;
        break;
    case catalog_schema_version_v22:
        rc = migrate_catalog_schema_v22_to_v23(sqlite);
        next_schema_version = catalog_schema_version_v23;
        break;
    case catalog_schema_version_v23:
        rc = migrate_catalog_schema_v23_to_v24(sqlite);
        next_schema_version = catalog_schema_version_v24;
        break;
    case catalog_schema_version_v24:
        rc = migrate_catalog_schema_v24_to_v25(sqlite);
        next_schema_version = catalog_schema_version_v25;
        break;
    case catalog_schema_version_v25:
        rc = migrate_catalog_schema_v25_to_v26(sqlite);
        next_schema_version = catalog_schema_version_v26;
        break;
    case catalog_schema_version_v26:
        rc = migrate_catalog_schema_v26_to_v27(sqlite);
        next_schema_version = catalog_schema_version_v27;
        break;
    case catalog_schema_version_v27:
        rc = migrate_catalog_schema_v27_to_v28(sqlite);
        next_schema_version = catalog_schema_version_v28;
        break;
    case catalog_schema_version_v28:
        rc = migrate_catalog_schema_v28_to_v29(sqlite);
        next_schema_version = catalog_schema_version_v29;
        break;
    case catalog_schema_version_v29:
        rc = migrate_catalog_schema_v29_to_v30(sqlite);
        next_schema_version = MYLITE_CATALOG_SCHEMA_VERSION;
        break;
    default:
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *schema_version = next_schema_version;
    }

    return rc;
}

static int migrate_catalog_schema_v1_to_v2(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN default_kind INTEGER NOT NULL DEFAULT 0;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN default_integer INTEGER;"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 2, minimum_reader_schema_version = 2;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v2_to_v3(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v2;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v2;"
        "DROP TABLE _mylite_catalog_columns_v2;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 3, minimum_reader_schema_version = 3;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v3_to_v4(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v3;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2)),"
        "default_integer INTEGER,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, 1, default_kind, default_integer, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v3;"
        "DROP TABLE _mylite_catalog_columns_v3;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 4, minimum_reader_schema_version = 4;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v4_to_v5(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "CREATE TABLE _mylite_catalog_indexes ("
                             "index_id INTEGER PRIMARY KEY,"
                             "table_id INTEGER NOT NULL,"
                             "name TEXT NOT NULL,"
                             "kind INTEGER NOT NULL CHECK(kind = 1),"
                             "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
                             "physical_name TEXT NOT NULL UNIQUE,"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(table_id, name)"
                             ");"
                             "CREATE TABLE _mylite_catalog_index_columns ("
                             "index_column_id INTEGER PRIMARY KEY,"
                             "index_id INTEGER NOT NULL,"
                             "table_id INTEGER NOT NULL,"
                             "column_id INTEGER NOT NULL,"
                             "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(index_id, ordinal_position),"
                             "UNIQUE(index_id, column_id)"
                             ");"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 5, minimum_reader_schema_version = 5;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v5_to_v6(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN auto_increment_next INTEGER NOT NULL DEFAULT 1;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN is_auto_increment INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(is_auto_increment IN (0, 1));"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 6, minimum_reader_schema_version = 6;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v6_to_v7(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v6;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "NULL, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v6;"
        "DROP TABLE _mylite_catalog_columns_v6;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 7, minimum_reader_schema_version = 7;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v7_to_v8(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v7;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v7;"
        "DROP TABLE _mylite_catalog_columns_v7;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 8, minimum_reader_schema_version = 8;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v8_to_v9(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes RENAME TO _mylite_catalog_indexes_v8;"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes_v8;"
        "DROP TABLE _mylite_catalog_indexes_v8;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 9, minimum_reader_schema_version = 9;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v9_to_v10(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_index_columns "
        "ADD COLUMN prefix_length INTEGER CHECK(prefix_length IS NULL OR prefix_length > 0);"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 10, minimum_reader_schema_version = 10;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v10_to_v11(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v10;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v10;"
        "DROP TABLE _mylite_catalog_columns_v10;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 11, minimum_reader_schema_version = 11;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v11_to_v12(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v11;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6, 7)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, 0, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation FROM _mylite_catalog_columns_v11;"
        "DROP TABLE _mylite_catalog_columns_v11;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 12, minimum_reader_schema_version = 12;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v12_to_v13(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN default_charset TEXT NOT NULL DEFAULT '" MYLITE_CATALOG_DEFAULT_TABLE_CHARSET
        "';"
        "ALTER TABLE _mylite_catalog_tables "
        "ADD COLUMN default_collation TEXT NOT NULL DEFAULT "
        "'" MYLITE_CATALOG_DEFAULT_TABLE_COLLATION "';"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 13, minimum_reader_schema_version = 13;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v13_to_v14(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "CREATE TABLE IF NOT EXISTS _mylite_catalog_foreign_keys ("
                             "foreign_key_id INTEGER PRIMARY KEY,"
                             "child_table_id INTEGER NOT NULL,"
                             "parent_table_id INTEGER NOT NULL,"
                             "name TEXT NOT NULL,"
                             "parent_index_id INTEGER NOT NULL,"
                             "child_index_id INTEGER NOT NULL,"
                             "update_rule TEXT NOT NULL,"
                             "delete_rule TEXT NOT NULL,"
                             "match_option TEXT NOT NULL,"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(child_table_id, name)"
                             ");"
                             "CREATE TABLE IF NOT EXISTS _mylite_catalog_foreign_key_columns ("
                             "foreign_key_column_id INTEGER PRIMARY KEY,"
                             "foreign_key_id INTEGER NOT NULL,"
                             "child_table_id INTEGER NOT NULL,"
                             "parent_table_id INTEGER NOT NULL,"
                             "child_column_id INTEGER NOT NULL,"
                             "parent_column_id INTEGER NOT NULL,"
                             "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
                             "position_in_unique_constraint INTEGER NOT NULL "
                             "CHECK(position_in_unique_constraint > 0),"
                             "descriptor_version INTEGER NOT NULL,"
                             "created_catalog_generation INTEGER NOT NULL,"
                             "updated_catalog_generation INTEGER NOT NULL,"
                             "UNIQUE(foreign_key_id, ordinal_position)"
                             ");"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 14, minimum_reader_schema_version = 14;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v14_to_v15(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v14;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v14;"
        "DROP TABLE _mylite_catalog_columns_v14;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 15, minimum_reader_schema_version = 15;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v15_to_v16(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN character_set_name TEXT NOT NULL DEFAULT '';"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN collation_name TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 16, minimum_reader_schema_version = 16;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v16_to_v17(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_index_columns "
                             "ADD COLUMN sort_direction INTEGER NOT NULL DEFAULT 1 "
                             "CHECK(sort_direction IN (1, 2));"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 17, minimum_reader_schema_version = 17;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v17_to_v18(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "CREATE TABLE IF NOT EXISTS _mylite_catalog_check_constraints ("
        "check_constraint_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "physical_name TEXT NOT NULL,"
        "check_clause TEXT NOT NULL,"
        "sqlite_expression TEXT NOT NULL,"
        "is_enforced INTEGER NOT NULL CHECK(is_enforced IN (0, 1)),"
        "name_is_generated INTEGER NOT NULL CHECK(name_is_generated IN (0, 1)),"
        "generated_ordinal INTEGER NOT NULL CHECK(generated_ordinal > 0),"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name),"
        "UNIQUE(table_id, physical_name),"
        "UNIQUE(table_id, ordinal_position)"
        ");"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 18, minimum_reader_schema_version = 18;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v18_to_v19(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes RENAME TO _mylite_catalog_indexes_v18;"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2, 3, 4)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "SELECT index_id, table_id, name, kind, is_unique, physical_name, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_indexes_v18;"
        "DROP TABLE _mylite_catalog_indexes_v18;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 19, minimum_reader_schema_version = 19;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v19_to_v20(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN fulltext_doc_id_initialized INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(fulltext_doc_id_initialized IN (0, 1));"
                             "UPDATE _mylite_catalog_tables "
                             "SET fulltext_doc_id_initialized = 1 "
                             "WHERE table_id IN ("
                             "SELECT DISTINCT table_id FROM _mylite_catalog_indexes WHERE kind = 3"
                             ");"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 20, minimum_reader_schema_version = 20;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v20_to_v21(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN created_time_utc_epoch INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(created_time_utc_epoch >= 0);"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN updated_time_utc_epoch INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(updated_time_utc_epoch >= 0);"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 21, minimum_reader_schema_version = 21;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v21_to_v22(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_schemas "
        "ADD COLUMN default_charset TEXT NOT NULL DEFAULT '" MYLITE_CATALOG_DEFAULT_TABLE_CHARSET
        "';"
        "ALTER TABLE _mylite_catalog_schemas "
        "ADD COLUMN default_collation TEXT NOT NULL DEFAULT "
        "'" MYLITE_CATALOG_DEFAULT_TABLE_COLLATION "';"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 22, minimum_reader_schema_version = 22;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v22_to_v23(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_indexes "
                             "ADD COLUMN is_visible INTEGER NOT NULL DEFAULT 1 "
                             "CHECK(is_visible IN (0, 1));"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 23, minimum_reader_schema_version = 23;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v23_to_v24(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_tables "
                             "ADD COLUMN comment TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 24, minimum_reader_schema_version = 24;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v24_to_v25(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes ADD COLUMN comment TEXT NOT NULL DEFAULT '';"
        "ALTER TABLE _mylite_catalog_indexes "
        "ADD COLUMN show_create_explicit_btree INTEGER NOT NULL DEFAULT 0 "
        "CHECK(show_create_explicit_btree IN (0, 1));"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 25, minimum_reader_schema_version = 25;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v25_to_v26(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN comment TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 26, minimum_reader_schema_version = 26;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v26_to_v27(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_indexes RENAME TO _mylite_catalog_indexes_v26;"
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2, 3, 4)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "comment TEXT NOT NULL,"
        "show_create_explicit_btree INTEGER NOT NULL "
        "CHECK(show_create_explicit_btree IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, is_visible, physical_name, comment, "
        "show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation) "
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, comment, "
        "show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes_v26;"
        "DROP TABLE _mylite_catalog_indexes_v26;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 27, minimum_reader_schema_version = 27;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v27_to_v28(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v27;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "character_set_name TEXT NOT NULL,"
        "collation_name TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v27;"
        "DROP TABLE _mylite_catalog_columns_v27;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 28, minimum_reader_schema_version = 28;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v28_to_v29(sqlite3 *sqlite) {
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "ALTER TABLE _mylite_catalog_columns RENAME TO _mylite_catalog_columns_v28;"
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN "
        "(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "character_set_name TEXT NOT NULL,"
        "collation_name TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");"
        "INSERT INTO _mylite_catalog_columns "
        "(column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, "
        "comment, descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns_v28;"
        "DROP TABLE _mylite_catalog_columns_v28;"
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 29, minimum_reader_schema_version = 29;"
        "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int migrate_catalog_schema_v29_to_v30(sqlite3 *sqlite) {
    static const char *sql = "BEGIN IMMEDIATE;"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN is_generated INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(is_generated IN (0, 1));"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN generated_kind INTEGER NOT NULL DEFAULT 0 "
                             "CHECK(generated_kind IN (0, 1, 2));"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN generation_expression TEXT NOT NULL DEFAULT '';"
                             "ALTER TABLE _mylite_catalog_columns "
                             "ADD COLUMN sqlite_generation_expression TEXT NOT NULL DEFAULT '';"
                             "UPDATE _mylite_catalog_state "
                             "SET schema_version = 30, minimum_reader_schema_version = 30;"
                             "COMMIT;";
    int rc = execute_sql(sqlite, sql);

    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(sqlite);
        return rc;
    }

    return MYLITE_OK;
}

static int validate_catalog_descriptor_tables(sqlite3 *sqlite) {
    int rc = validate_select_shape(
        sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE 0"
    );

    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
            "default_charset, default_collation, comment, fulltext_doc_id_initialized, "
            "created_time_utc_epoch, updated_time_utc_epoch, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_tables WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
            "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
            "default_text, on_update_current_timestamp, character_set_name, collation_name, "
            "comment, is_generated, generated_kind, generation_expression, "
            "sqlite_generation_expression, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_columns WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
            "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
            "updated_catalog_generation "
            "FROM _mylite_catalog_indexes WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
            "prefix_length, sort_direction, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_index_columns WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
            "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
            "created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_foreign_keys WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
            "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
            "descriptor_version, created_catalog_generation, updated_catalog_generation "
            "FROM _mylite_catalog_foreign_key_columns WHERE 0"
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_shape(
            sqlite,
            "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
            "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
            "ordinal_position, descriptor_version, created_catalog_generation, "
            "updated_catalog_generation "
            "FROM _mylite_catalog_check_constraints WHERE 0"
        );
    }

    return rc;
}

static int validate_select_shape(sqlite3 *sqlite, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int rc = prepare_statement(sqlite, sql, &statement);

    return finalize_statement(statement, rc);
}

static int initialize_catalog_schema(struct mylite_db *database) {
    static const char *const sql_statements[] = {
        "CREATE TABLE _mylite_catalog_state ("
        "singleton_id INTEGER PRIMARY KEY CHECK(singleton_id = 1),"
        "schema_version INTEGER NOT NULL,"
        "minimum_reader_schema_version INTEGER NOT NULL,"
        "catalog_generation INTEGER NOT NULL,"
        "created_with_file_format_version INTEGER NOT NULL"
        ");",
        "CREATE TABLE _mylite_catalog_schemas ("
        "schema_id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE,"
        "default_charset TEXT NOT NULL,"
        "default_collation TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL"
        ");",
        "CREATE TABLE _mylite_catalog_tables ("
        "table_id INTEGER PRIMARY KEY,"
        "schema_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind = 1),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "auto_increment_next INTEGER NOT NULL CHECK(auto_increment_next > 0),"
        "default_charset TEXT NOT NULL,"
        "default_collation TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "fulltext_doc_id_initialized INTEGER NOT NULL "
        "CHECK(fulltext_doc_id_initialized IN (0, 1)),"
        "created_time_utc_epoch INTEGER NOT NULL CHECK(created_time_utc_epoch >= 0),"
        "updated_time_utc_epoch INTEGER NOT NULL CHECK(updated_time_utc_epoch >= 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(schema_id, name)"
        ");",
        "CREATE TABLE _mylite_catalog_columns ("
        "column_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "name TEXT NOT NULL,"
        "logical_type TEXT NOT NULL,"
        "physical_type TEXT NOT NULL,"
        "is_nullable INTEGER NOT NULL CHECK(is_nullable IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "is_auto_increment INTEGER NOT NULL CHECK(is_auto_increment IN (0, 1)),"
        "default_kind INTEGER NOT NULL CHECK(default_kind IN "
        "(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)),"
        "default_integer INTEGER,"
        "default_text TEXT,"
        "on_update_current_timestamp INTEGER NOT NULL "
        "CHECK(on_update_current_timestamp IN (0, 1)),"
        "character_set_name TEXT NOT NULL,"
        "collation_name TEXT NOT NULL,"
        "comment TEXT NOT NULL,"
        "is_generated INTEGER NOT NULL CHECK(is_generated IN (0, 1)),"
        "generated_kind INTEGER NOT NULL CHECK(generated_kind IN (0, 1, 2)),"
        "generation_expression TEXT NOT NULL,"
        "sqlite_generation_expression TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, ordinal_position),"
        "UNIQUE(table_id, name)"
        ");",
        "CREATE TABLE _mylite_catalog_indexes ("
        "index_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind IN (1, 2, 3, 4)),"
        "is_unique INTEGER NOT NULL CHECK(is_unique IN (0, 1)),"
        "is_visible INTEGER NOT NULL CHECK(is_visible IN (0, 1)),"
        "physical_name TEXT NOT NULL UNIQUE,"
        "comment TEXT NOT NULL,"
        "show_create_explicit_btree INTEGER NOT NULL "
        "CHECK(show_create_explicit_btree IN (0, 1)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name)"
        ");",
        "CREATE TABLE _mylite_catalog_index_columns ("
        "index_column_id INTEGER PRIMARY KEY,"
        "index_id INTEGER NOT NULL,"
        "table_id INTEGER NOT NULL,"
        "column_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "prefix_length INTEGER CHECK(prefix_length IS NULL OR prefix_length > 0),"
        "sort_direction INTEGER NOT NULL CHECK(sort_direction IN (1, 2)),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(index_id, ordinal_position),"
        "UNIQUE(index_id, column_id)"
        ");",
        "CREATE TABLE _mylite_catalog_foreign_keys ("
        "foreign_key_id INTEGER PRIMARY KEY,"
        "child_table_id INTEGER NOT NULL,"
        "parent_table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "parent_index_id INTEGER NOT NULL,"
        "child_index_id INTEGER NOT NULL,"
        "update_rule TEXT NOT NULL,"
        "delete_rule TEXT NOT NULL,"
        "match_option TEXT NOT NULL,"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(child_table_id, name)"
        ");",
        "CREATE TABLE _mylite_catalog_foreign_key_columns ("
        "foreign_key_column_id INTEGER PRIMARY KEY,"
        "foreign_key_id INTEGER NOT NULL,"
        "child_table_id INTEGER NOT NULL,"
        "parent_table_id INTEGER NOT NULL,"
        "child_column_id INTEGER NOT NULL,"
        "parent_column_id INTEGER NOT NULL,"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "position_in_unique_constraint INTEGER NOT NULL "
        "CHECK(position_in_unique_constraint > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(foreign_key_id, ordinal_position)"
        ");",
        "CREATE TABLE _mylite_catalog_check_constraints ("
        "check_constraint_id INTEGER PRIMARY KEY,"
        "table_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "physical_name TEXT NOT NULL,"
        "check_clause TEXT NOT NULL,"
        "sqlite_expression TEXT NOT NULL,"
        "is_enforced INTEGER NOT NULL CHECK(is_enforced IN (0, 1)),"
        "name_is_generated INTEGER NOT NULL CHECK(name_is_generated IN (0, 1)),"
        "generated_ordinal INTEGER NOT NULL CHECK(generated_ordinal > 0),"
        "ordinal_position INTEGER NOT NULL CHECK(ordinal_position > 0),"
        "descriptor_version INTEGER NOT NULL,"
        "created_catalog_generation INTEGER NOT NULL,"
        "updated_catalog_generation INTEGER NOT NULL,"
        "UNIQUE(table_id, name),"
        "UNIQUE(table_id, physical_name),"
        "UNIQUE(table_id, ordinal_position)"
        ");",
        "INSERT INTO _mylite_catalog_state "
        "(singleton_id, schema_version, minimum_reader_schema_version, catalog_generation, "
        "created_with_file_format_version) "
        "VALUES (1, " MYLITE_CATALOG_SCHEMA_VERSION_TEXT
        ", " MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION_TEXT ", 1, 1);",
    };
    struct mylite_catalog catalog = {.initialized = false};
    int rc = execute_sql(database->sqlite, "BEGIN IMMEDIATE;");

    for (size_t index = 0U;
         rc == MYLITE_OK && index < sizeof(sql_statements) / sizeof(sql_statements[0U]);
         ++index) {
        rc = execute_sql(database->sqlite, sql_statements[index]);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sql(database->sqlite, "COMMIT;");
    }
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }

    rc = read_catalog_state(database->sqlite, &catalog);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return apply_catalog_state(database, &catalog);
}

static int existing_catalog_table_count(sqlite3 *sqlite, int *out_count) {
    enum {
        catalog_state_name_bind = 1,
        catalog_schemas_name_bind = 2,
        catalog_tables_name_bind = 3,
        catalog_columns_name_bind = 4,
        catalog_indexes_name_bind = 5,
        catalog_index_columns_name_bind = 6,
        catalog_foreign_keys_name_bind = 7,
        catalog_foreign_key_columns_name_bind = 8,
        catalog_check_constraints_name_bind = 9,
    };

    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_count = 0;
    rc = prepare_statement(
        sqlite,
        "SELECT count(*) FROM sqlite_master "
        "WHERE type = 'table' "
        "AND name IN (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_state_name_bind, catalog_state_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_schemas_name_bind, catalog_schemas_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_tables_name_bind, catalog_tables_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_columns_name_bind, catalog_columns_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_indexes_name_bind, catalog_indexes_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_index_columns_name_bind,
            catalog_index_columns_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            bind_text(statement, catalog_foreign_keys_name_bind, catalog_foreign_keys_table_name());
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_foreign_key_columns_name_bind,
            catalog_foreign_key_columns_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            catalog_check_constraints_name_bind,
            catalog_check_constraints_table_name()
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_count = sqlite3_column_int(statement, 0);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int read_catalog_state(sqlite3 *sqlite, struct mylite_catalog *catalog) {
    sqlite3_stmt *statement = NULL;
    int64_t singleton_id = 0;
    int64_t schema_version = 0;
    int64_t minimum_reader_schema_version = 0;
    int64_t generation = 0;
    int64_t file_format_version = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT singleton_id, schema_version, minimum_reader_schema_version, catalog_generation, "
        "created_with_file_format_version "
        "FROM _mylite_catalog_state",
        &statement
    );

    *catalog = (struct mylite_catalog){.initialized = false};
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_ROW) {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_state_select_singleton_id_column, &singleton_id);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_schema_version_column,
            &schema_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_minimum_reader_schema_version_column,
            &minimum_reader_schema_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_catalog_generation_column,
            &generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_state_select_file_format_version_column,
            &file_format_version
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = sqlite_rc == SQLITE_ROW ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK && (singleton_id != 1 || schema_version < 1 ||
                            schema_version > MYLITE_CATALOG_SCHEMA_VERSION ||
                            minimum_reader_schema_version > MYLITE_CATALOG_SCHEMA_VERSION ||
                            minimum_reader_schema_version < 1 ||
                            file_format_version != MYLITE_FILE_FORMAT_VERSION || generation < 1)) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = i64_to_u32(schema_version, &catalog->schema_version);
    }
    if (rc == MYLITE_OK) {
        rc = i64_to_u64(generation, &catalog->generation);
    }
    if (rc == MYLITE_OK) {
        catalog->initialized = true;
        reset_descriptor_cache_state(catalog);
    }

    return finalize_statement(statement, rc);
}

static int apply_catalog_state(struct mylite_db *database, const struct mylite_catalog *catalog) {
    database->catalog = *catalog;
    database->session.catalog_generation = catalog->generation;

    return MYLITE_OK;
}

static int begin_catalog_transaction(sqlite3 *sqlite) {
    return execute_sql(sqlite, "BEGIN IMMEDIATE");
}

static int commit_catalog_transaction(sqlite3 *sqlite) {
    return execute_sql(sqlite, "COMMIT");
}

static void rollback_catalog_transaction(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return;
    }

    (void)sqlite3_exec(sqlite, "ROLLBACK", NULL, NULL, NULL);
}

static int begin_generation_change(
    struct mylite_db *database,
    struct catalog_generation_change *out_change
) {
    struct mylite_catalog catalog = {.initialized = false};
    int rc = begin_catalog_transaction(database->sqlite);

    *out_change = (struct catalog_generation_change){0};
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_catalog_state(database->sqlite, &catalog);
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }
    if (catalog.generation == UINT64_MAX) {
        rollback_catalog_transaction(database->sqlite);
        return MYLITE_ERROR;
    }

    out_change->next_generation = catalog.generation + 1U;

    return MYLITE_OK;
}

static int finish_generation_change(
    struct mylite_db *database,
    const struct catalog_generation_change *change
) {
    int rc = update_catalog_generation(database->sqlite, change->next_generation);

    if (rc == MYLITE_OK) {
        rc = commit_catalog_transaction(database->sqlite);
    }
    if (rc != MYLITE_OK) {
        rollback_catalog_transaction(database->sqlite);
        return rc;
    }

    database->catalog.generation = change->next_generation;
    database->session.catalog_generation = change->next_generation;
    reset_descriptor_cache_state(&database->catalog);

    return MYLITE_OK;
}

static void abandon_generation_change(sqlite3 *sqlite) {
    rollback_catalog_transaction(sqlite);
}

static int update_catalog_generation(sqlite3 *sqlite, uint64_t generation) {
    sqlite3_stmt *statement = NULL;
    int rc = validate_generation(generation);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = prepare_statement(
        sqlite,
        "UPDATE _mylite_catalog_state SET catalog_generation = ?1 WHERE singleton_id = 1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, 1, generation);
    }
    if (rc == MYLITE_OK) {
        rc = step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = require_changed_row(sqlite);
    }

    return finalize_statement(statement, rc);
}

static int execute_sql(sqlite3 *sqlite, const char *sql) {
    int sqlite_rc = sqlite3_exec(sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int prepare_statement(sqlite3 *sqlite, const char *sql, sqlite3_stmt **out_statement) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(sqlite, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

static int bind_text(sqlite3_stmt *statement, int index, const char *value) {
    int sqlite_rc = sqlite3_bind_text(
        statement,
        index,
        value,
        sqlite_use_nul_terminated_string,
        SQLITE_TRANSIENT
    );

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_nullable_text(
    sqlite3_stmt *statement,
    int index,
    bool has_value,
    const char *value
) {
    int sqlite_rc = SQLITE_OK;

    if (!has_value) {
        sqlite_rc = sqlite3_bind_null(statement, index);
    } else {
        sqlite_rc = sqlite3_bind_text(
            statement,
            index,
            value,
            sqlite_use_nul_terminated_string,
            SQLITE_TRANSIENT
        );
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_i64(sqlite3_stmt *statement, int index, int64_t value) {
    int sqlite_rc = sqlite3_bind_int64(statement, index, (sqlite3_int64)value);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_nullable_i64(sqlite3_stmt *statement, int index, bool has_value, int64_t value) {
    int sqlite_rc = SQLITE_OK;

    if (!has_value) {
        sqlite_rc = sqlite3_bind_null(statement, index);
    } else {
        sqlite_rc = sqlite3_bind_int64(statement, index, (sqlite3_int64)value);
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): mirrors SQLite bind helper order.
static int bind_u64(sqlite3_stmt *statement, int index, uint64_t value) {
    int64_t signed_value = 0;
    int rc = u64_to_i64(value, &signed_value);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return bind_i64(statement, index, signed_value);
}

static int64_t catalog_bool_value(bool value) {
    if (value) {
        return 1;
    }

    return 0;
}

static int bind_catalog_column_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct catalog_column_values *values,
    uint64_t generation
) {
    int rc = bind_catalog_column_insert_core_values(statement, table_id, ordinal_position, values);

    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_default_values(
            statement,
            (struct catalog_column_default_bind_indexes){
                .default_kind = catalog_column_insert_default_kind_bind,
                .default_integer = catalog_column_insert_default_integer_bind,
                .default_text = catalog_column_insert_default_text_bind,
                .on_update_current_timestamp =
                    catalog_column_insert_on_update_current_timestamp_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_text_attributes(
            statement,
            (struct catalog_column_text_attribute_bind_indexes){
                .character_set_name = catalog_column_insert_character_set_name_bind,
                .collation_name = catalog_column_insert_collation_name_bind,
                .comment = catalog_column_insert_comment_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_generated_values(
            statement,
            (struct catalog_column_generated_bind_indexes){
                .is_generated = catalog_column_insert_is_generated_bind,
                .generated_kind = catalog_column_insert_generated_kind_bind,
                .generation_expression = catalog_column_insert_generation_expression_bind,
                .sqlite_generation_expression =
                    catalog_column_insert_sqlite_generation_expression_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_column_insert_generation_bind, generation);
    }

    return rc;
}

static int bind_catalog_column_replace_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t column_id,
    const struct catalog_column_values *values,
    uint64_t generation
) {
    int rc = bind_catalog_column_replace_core_values(statement, values);

    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_default_values(
            statement,
            (struct catalog_column_default_bind_indexes){
                .default_kind = catalog_column_replace_default_kind_bind,
                .default_integer = catalog_column_replace_default_integer_bind,
                .default_text = catalog_column_replace_default_text_bind,
                .on_update_current_timestamp =
                    catalog_column_replace_on_update_current_timestamp_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_text_attributes(
            statement,
            (struct catalog_column_text_attribute_bind_indexes){
                .character_set_name = catalog_column_replace_character_set_name_bind,
                .collation_name = catalog_column_replace_collation_name_bind,
                .comment = catalog_column_replace_comment_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_column_generated_values(
            statement,
            (struct catalog_column_generated_bind_indexes){
                .is_generated = catalog_column_replace_is_generated_bind,
                .generated_kind = catalog_column_replace_generated_kind_bind,
                .generation_expression = catalog_column_replace_generation_expression_bind,
                .sqlite_generation_expression =
                    catalog_column_replace_sqlite_generation_expression_bind,
            },
            values
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_u64(statement, catalog_column_replace_generation_bind, generation);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_replace_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_replace_column_id_bind, column_id);
    }

    return rc;
}

static int bind_catalog_column_insert_core_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct catalog_column_values *values
) {
    int rc = bind_i64(statement, catalog_column_insert_table_id_bind, table_id);

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, catalog_column_insert_ordinal_position_bind, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_name_bind, values->name);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_logical_type_bind, values->logical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_insert_physical_type_bind, values->physical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_insert_is_nullable_bind,
            catalog_bool_value(values->is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_insert_is_visible_bind,
            catalog_bool_value(values->is_visible)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_insert_is_auto_increment_bind,
            catalog_bool_value(values->is_auto_increment)
        );
    }

    return rc;
}

static int bind_catalog_column_replace_core_values(
    sqlite3_stmt *statement,
    const struct catalog_column_values *values
) {
    int rc = bind_text(statement, catalog_column_replace_name_bind, values->name);

    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_replace_logical_type_bind, values->logical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, catalog_column_replace_physical_type_bind, values->physical_type);
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_replace_is_nullable_bind,
            catalog_bool_value(values->is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_replace_is_visible_bind,
            catalog_bool_value(values->is_visible)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            catalog_column_replace_is_auto_increment_bind,
            catalog_bool_value(values->is_auto_increment)
        );
    }

    return rc;
}

static int bind_catalog_column_default_values(
    sqlite3_stmt *statement,
    struct catalog_column_default_bind_indexes indexes,
    const struct catalog_column_values *values
) {
    int rc = bind_i64(statement, indexes.default_kind, (int64_t)values->default_kind);

    if (rc == MYLITE_OK) {
        rc = bind_nullable_i64(
            statement,
            indexes.default_integer,
            catalog_default_kind_stores_integer(values->default_kind),
            values->default_integer
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_nullable_text(
            statement,
            indexes.default_text,
            catalog_default_kind_stores_text(values->default_kind),
            values->default_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_i64(
            statement,
            indexes.on_update_current_timestamp,
            catalog_bool_value(values->on_update_current_timestamp)
        );
    }

    return rc;
}

static int bind_catalog_column_text_attributes(
    sqlite3_stmt *statement,
    struct catalog_column_text_attribute_bind_indexes indexes,
    const struct catalog_column_values *values
) {
    int rc = bind_text(
        statement,
        indexes.character_set_name,
        catalog_text_or_empty(values->character_set_name)
    );

    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            indexes.collation_name,
            catalog_text_or_empty(values->collation_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, indexes.comment, catalog_text_or_empty(values->comment));
    }

    return rc;
}

static int bind_catalog_column_generated_values(
    sqlite3_stmt *statement,
    struct catalog_column_generated_bind_indexes indexes,
    const struct catalog_column_values *values
) {
    int rc = bind_i64(statement, indexes.is_generated, catalog_bool_value(values->is_generated));

    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, indexes.generated_kind, (int64_t)values->generated_kind);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            indexes.generation_expression,
            catalog_text_or_empty(values->generation_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(
            statement,
            indexes.sqlite_generation_expression,
            catalog_text_or_empty(values->sqlite_generation_expression)
        );
    }

    return rc;
}

static const char *catalog_text_or_empty(const char *value) {
    if (value == NULL) {
        return "";
    }
    return value;
}

static int step_done(sqlite3_stmt *statement) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int require_changed_row(sqlite3 *sqlite) {
    if (sqlite3_changes(sqlite) != 1) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int finalize_statement(sqlite3_stmt *statement, int rc) {
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

static int read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    bool found = false;
    int rc = try_read_schema_by_name(sqlite, name, out_schema, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int try_read_schema_by_name(
    sqlite3 *sqlite,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE name = ?1",
        &statement
    );

    *out_schema = (struct mylite_catalog_schema_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_schema(statement, out_schema);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_schema = (struct mylite_catalog_schema_descriptor){0};
        }
    }

    return finalize_statement(statement, rc);
}

static int read_schema_by_id(
    sqlite3 *sqlite,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE schema_id = ?1",
        &statement
    );

    *out_schema = (struct mylite_catalog_schema_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_schema(statement, out_schema);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
) {
    bool found = false;
    int rc = try_read_table_by_name(sqlite, schema_id, name, out_table, &found);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (!found) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int try_read_table_by_name(
    sqlite3 *sqlite,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 AND name = ?2",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_table(statement, out_table);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_table = (struct mylite_catalog_table_descriptor){0};
        }
    }

    return finalize_statement(statement, rc);
}

static int read_table_by_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE table_id = ?1",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_table(statement, out_table);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int read_column_by_name(
    sqlite3 *sqlite,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT column_id, table_id, ordinal_position, name, logical_type, physical_type, "
        "is_nullable, is_visible, is_auto_increment, default_kind, default_integer, "
        "default_text, on_update_current_timestamp, character_set_name, collation_name, comment, "
        "is_generated, generated_kind, generation_expression, sqlite_generation_expression, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_columns WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_column(statement, out_column);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_statement(statement, rc);
}

static int try_read_primary_index_by_table_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 AND kind = 1",
        &statement
    );

    *out_index = (struct mylite_catalog_index_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_index(statement, out_index);
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        } else {
            *out_index = (struct mylite_catalog_index_descriptor){0};
        }
    }

    return finalize_statement(statement, rc);
}

static int read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(table_id), 0) + 1 FROM _mylite_catalog_tables",
        &statement
    );

    *out_table_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = checked_column_i64(statement, catalog_next_table_id_column, out_table_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(*out_table_id);
    }

    return finalize_statement(statement, rc);
}

static int read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(index_id), 0) + 1 FROM _mylite_catalog_indexes",
        &statement
    );

    *out_index_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = checked_column_i64(statement, 0, out_index_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(*out_index_id);
    }

    return finalize_statement(statement, rc);
}

static int read_next_foreign_key_id(sqlite3 *sqlite, int64_t *out_foreign_key_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(foreign_key_id), 0) + 1 FROM _mylite_catalog_foreign_keys",
        &statement
    );

    *out_foreign_key_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = checked_column_i64(statement, 0, out_foreign_key_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(*out_foreign_key_id);
    }

    return finalize_statement(statement, rc);
}

static int read_next_check_constraint_id(sqlite3 *sqlite, int64_t *out_check_constraint_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(check_constraint_id), 0) + 1 "
        "FROM _mylite_catalog_check_constraints",
        &statement
    );

    *out_check_constraint_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = checked_column_i64(statement, 0, out_check_constraint_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_positive_id(*out_check_constraint_id);
    }

    return finalize_statement(statement, rc);
}

static int materialize_schema(
    sqlite3_stmt *statement,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = checked_column_i64(
        statement,
        catalog_schema_select_schema_id_column,
        &out_schema->schema_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_schema_select_name_column,
            out_schema->name,
            sizeof(out_schema->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_schema_select_default_charset_column,
            out_schema->default_charset,
            sizeof(out_schema->default_charset)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_schema_select_default_collation_column,
            out_schema->default_collation,
            sizeof(out_schema->default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_schema_select_descriptor_version_column,
            &out_schema->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_schema_select_created_generation_column,
            &out_schema->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_schema_select_updated_generation_column,
            &out_schema->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_table(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int64_t kind = 0;
    int64_t fulltext_doc_id_initialized = 0;
    int rc =
        checked_column_i64(statement, catalog_table_select_table_id_column, &out_table->table_id);

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_table_select_schema_id_column,
            &out_table->schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_table_select_name_column,
            out_table->name,
            sizeof(out_table->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_table_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_kind((enum mylite_catalog_table_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_table->kind = (enum mylite_catalog_table_kind)kind;
        rc = checked_column_text(
            statement,
            catalog_table_select_physical_name_column,
            out_table->physical_name,
            sizeof(out_table->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_table_select_auto_increment_next_column,
            &out_table->auto_increment_next
        );
    }
    if (rc == MYLITE_OK && out_table->auto_increment_next <= 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_table_select_default_charset_column,
            out_table->default_charset,
            sizeof(out_table->default_charset)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_table_select_default_collation_column,
            out_table->default_collation,
            sizeof(out_table->default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_table_select_comment_column,
            out_table->comment,
            sizeof(out_table->comment)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_table_select_fulltext_doc_id_initialized_column,
            &fulltext_doc_id_initialized
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(
            fulltext_doc_id_initialized,
            &out_table->fulltext_doc_id_initialized
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_table_select_created_time_column,
            &out_table->created_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK && out_table->created_time_utc_epoch < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_table_select_updated_time_column,
            &out_table->updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK && out_table->updated_time_utc_epoch < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_table_select_descriptor_version_column,
            &out_table->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_table_select_created_generation_column,
            &out_table->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_table_select_updated_generation_column,
            &out_table->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = materialize_column_identity(statement, out_column);

    if (rc == MYLITE_OK) {
        rc = materialize_column_flags(statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_defaults(statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_generated(statement, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_column_generations(statement, out_column);
    }

    return rc;
}

static int materialize_column_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = checked_column_i64(
        statement,
        catalog_column_select_column_id_column,
        &out_column->column_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_column_select_table_id_column,
            &out_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_column_select_ordinal_position_column,
            &out_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_name_column,
            out_column->name,
            sizeof(out_column->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_logical_type_column,
            out_column->logical_type,
            sizeof(out_column->logical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_physical_type_column,
            out_column->physical_type,
            sizeof(out_column->physical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_character_set_name_column,
            out_column->character_set_name,
            sizeof(out_column->character_set_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_collation_name_column,
            out_column->collation_name,
            sizeof(out_column->collation_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_comment_column,
            out_column->comment,
            sizeof(out_column->comment)
        );
    }

    return rc;
}

static int materialize_column_flags(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t nullable = 0;
    int64_t visible = 0;
    int64_t auto_increment = 0;
    int64_t on_update_current_timestamp = 0;
    int rc = checked_column_i64(statement, catalog_column_select_is_nullable_column, &nullable);

    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(nullable, &out_column->is_nullable);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_column_select_is_visible_column, &visible);
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(visible, &out_column->is_visible);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_column_select_is_auto_increment_column,
            &auto_increment
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(auto_increment, &out_column->is_auto_increment);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_column_select_on_update_current_timestamp_column,
            &on_update_current_timestamp
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(
            on_update_current_timestamp,
            &out_column->on_update_current_timestamp
        );
    }

    return rc;
}

static int materialize_column_defaults(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t default_kind = 0;
    bool has_default_integer = false;
    bool has_default_text = false;
    int rc =
        checked_column_i64(statement, catalog_column_select_default_kind_column, &default_kind);

    if (rc == MYLITE_OK) {
        rc = validate_column_default_kind((enum mylite_catalog_column_default_kind)default_kind);
    }
    if (rc == MYLITE_OK) {
        out_column->default_kind = (enum mylite_catalog_column_default_kind)default_kind;
        rc = checked_nullable_column_i64(
            statement,
            catalog_column_select_default_integer_column,
            &has_default_integer,
            &out_column->default_integer
        );
    }
    if (rc == MYLITE_OK &&
        ((catalog_default_kind_stores_integer(out_column->default_kind) && !has_default_integer) ||
         (!catalog_default_kind_stores_integer(out_column->default_kind) && has_default_integer))) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = checked_nullable_column_text(
            statement,
            catalog_column_select_default_text_column,
            &has_default_text,
            out_column->default_text,
            sizeof(out_column->default_text)
        );
    }
    if (rc == MYLITE_OK &&
        ((catalog_default_kind_stores_text(out_column->default_kind) && !has_default_text) ||
         (!catalog_default_kind_stores_text(out_column->default_kind) && has_default_text))) {
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int materialize_column_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = checked_column_u64(
        statement,
        catalog_column_select_descriptor_version_column,
        &out_column->descriptor_version
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_column_select_created_generation_column,
            &out_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_column_select_updated_generation_column,
            &out_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_column_generated(
    sqlite3_stmt *statement,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t is_generated = 0;
    int64_t generated_kind = 0;
    int rc =
        checked_column_i64(statement, catalog_column_select_is_generated_column, &is_generated);

    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(is_generated, &out_column->is_generated);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_column_select_generated_kind_column,
            &generated_kind
        );
    }
    if (rc == MYLITE_OK) {
        out_column->generated_kind = (enum mylite_catalog_generated_column_kind)generated_kind;
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_generation_expression_column,
            out_column->generation_expression,
            sizeof(out_column->generation_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_column_select_sqlite_generation_expression_column,
            out_column->sqlite_generation_expression,
            sizeof(out_column->sqlite_generation_expression)
        );
    }

    return rc;
}

static int materialize_index(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_descriptor *out_index
) {
    int64_t kind = 0;
    int64_t is_unique = 0;
    int64_t is_visible = 0;
    int64_t show_create_explicit_btree = 0;
    int rc =
        checked_column_i64(statement, catalog_index_select_index_id_column, &out_index->index_id);

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_select_table_id_column,
            &out_index->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_index_select_name_column,
            out_index->name,
            sizeof(out_index->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(statement, catalog_index_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = validate_index_kind((enum mylite_catalog_index_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_index->kind = (enum mylite_catalog_index_kind)kind;
        rc = checked_column_i64(statement, catalog_index_select_is_unique_column, &is_unique);
    }
    if (rc == MYLITE_OK && is_unique != 0 && is_unique != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_unique = is_unique != 0;
        rc = checked_column_i64(statement, catalog_index_select_is_visible_column, &is_visible);
    }
    if (rc == MYLITE_OK && is_visible != 0 && is_visible != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_visible = is_visible != 0;
        rc = checked_column_text(
            statement,
            catalog_index_select_physical_name_column,
            out_index->physical_name,
            sizeof(out_index->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_index_select_comment_column,
            out_index->comment,
            sizeof(out_index->comment)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_select_show_create_explicit_btree_column,
            &show_create_explicit_btree
        );
    }
    if (rc == MYLITE_OK && show_create_explicit_btree != 0 && show_create_explicit_btree != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->show_create_explicit_btree = show_create_explicit_btree != 0;
        rc = checked_column_u64(
            statement,
            catalog_index_select_descriptor_version_column,
            &out_index->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_select_created_generation_column,
            &out_index->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_select_updated_generation_column,
            &out_index->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_index_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    int rc = checked_column_i64(
        statement,
        catalog_index_column_select_index_column_id_column,
        &out_index_column->index_column_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_index_id_column,
            &out_index_column->index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_table_id_column,
            &out_index_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_column_id_column,
            &out_index_column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_ordinal_position_column,
            &out_index_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK &&
        sqlite3_column_type(statement, catalog_index_column_select_prefix_length_column) ==
            SQLITE_NULL) {
        out_index_column->has_prefix_length = false;
        out_index_column->prefix_length = 0;
    } else if (rc == MYLITE_OK) {
        out_index_column->has_prefix_length = true;
        rc = checked_column_i64(
            statement,
            catalog_index_column_select_prefix_length_column,
            &out_index_column->prefix_length
        );
    }
    if (rc == MYLITE_OK) {
        int64_t sort_direction = 0;

        rc = checked_column_i64(
            statement,
            catalog_index_column_select_sort_direction_column,
            &sort_direction
        );
        if (rc == MYLITE_OK && (sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC ||
                                sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC)) {
            out_index_column->sort_direction =
                (enum mylite_catalog_index_sort_direction)sort_direction;
        } else if (rc == MYLITE_OK) {
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_column_select_descriptor_version_column,
            &out_index_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_column_select_created_generation_column,
            &out_index_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_index_column_select_updated_generation_column,
            &out_index_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_foreign_key(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    int rc = checked_column_i64(
        statement,
        catalog_foreign_key_select_foreign_key_id_column,
        &out_foreign_key->foreign_key_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_select_child_table_id_column,
            &out_foreign_key->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_table_id_column,
            &out_foreign_key->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_foreign_key_select_name_column,
            out_foreign_key->name,
            sizeof(out_foreign_key->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_index_id_column,
            &out_foreign_key->parent_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_select_child_index_id_column,
            &out_foreign_key->child_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_foreign_key_select_update_rule_column,
            out_foreign_key->update_rule,
            sizeof(out_foreign_key->update_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_foreign_key_select_delete_rule_column,
            out_foreign_key->delete_rule,
            sizeof(out_foreign_key->delete_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_foreign_key_select_match_option_column,
            out_foreign_key->match_option,
            sizeof(out_foreign_key->match_option)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_foreign_key_select_descriptor_version_column,
            &out_foreign_key->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_foreign_key_select_created_generation_column,
            &out_foreign_key->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_foreign_key_select_updated_generation_column,
            &out_foreign_key->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_foreign_key_column(
    sqlite3_stmt *statement,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    int rc = checked_column_i64(
        statement,
        catalog_foreign_key_column_select_foreign_key_column_id_column,
        &out_foreign_key_column->foreign_key_column_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_column_select_foreign_key_id_column,
            &out_foreign_key_column->foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_table_id_column,
            &out_foreign_key_column->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_table_id_column,
            &out_foreign_key_column->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_column_id_column,
            &out_foreign_key_column->child_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_column_id_column,
            &out_foreign_key_column->parent_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_column_select_ordinal_position_column,
            &out_foreign_key_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_foreign_key_column_select_position_in_unique_constraint_column,
            &out_foreign_key_column->position_in_unique_constraint
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_foreign_key_column_select_descriptor_version_column,
            &out_foreign_key_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_foreign_key_column_select_created_generation_column,
            &out_foreign_key_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_foreign_key_column_select_updated_generation_column,
            &out_foreign_key_column->updated_catalog_generation
        );
    }

    return rc;
}

static int materialize_check_constraint(
    sqlite3_stmt *statement,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    int64_t is_enforced = 0;
    int64_t name_is_generated = 0;
    int rc = checked_column_i64(
        statement,
        catalog_check_constraint_select_check_constraint_id_column,
        &out_check_constraint->check_constraint_id
    );

    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_check_constraint_select_table_id_column,
            &out_check_constraint->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_check_constraint_select_name_column,
            out_check_constraint->name,
            sizeof(out_check_constraint->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_check_constraint_select_physical_name_column,
            out_check_constraint->physical_name,
            sizeof(out_check_constraint->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_check_constraint_select_check_clause_column,
            out_check_constraint->check_clause,
            sizeof(out_check_constraint->check_clause)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_text(
            statement,
            catalog_check_constraint_select_sqlite_expression_column,
            out_check_constraint->sqlite_expression,
            sizeof(out_check_constraint->sqlite_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_check_constraint_select_is_enforced_column,
            &is_enforced
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(is_enforced, &out_check_constraint->is_enforced);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_check_constraint_select_name_is_generated_column,
            &name_is_generated
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_catalog_bool_i64(name_is_generated, &out_check_constraint->name_is_generated);
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_check_constraint_select_generated_ordinal_column,
            &out_check_constraint->generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_i64(
            statement,
            catalog_check_constraint_select_ordinal_position_column,
            &out_check_constraint->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_check_constraint_select_descriptor_version_column,
            &out_check_constraint->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_check_constraint_select_created_generation_column,
            &out_check_constraint->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = checked_column_u64(
            statement,
            catalog_check_constraint_select_updated_generation_column,
            &out_check_constraint->updated_catalog_generation
        );
    }

    return rc;
}

static int checked_column_i64(sqlite3_stmt *statement, int index, int64_t *out_value) {
    if (sqlite3_column_type(statement, index) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)sqlite3_column_int64(statement, index);

    return MYLITE_OK;
}

static int checked_column_u64(sqlite3_stmt *statement, int index, uint64_t *out_value) {
    int64_t signed_value = 0;
    int rc = checked_column_i64(statement, index, &signed_value);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return i64_to_u64(signed_value, out_value);
}

static int checked_nullable_column_i64(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    int64_t *out_value
) {
    int column_type = sqlite3_column_type(statement, index);

    *out_has_value = false;
    *out_value = 0;
    if (column_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (column_type != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    *out_has_value = true;
    *out_value = (int64_t)sqlite3_column_int64(statement, index);

    return MYLITE_OK;
}

static int checked_column_text(
    sqlite3_stmt *statement,
    int index,
    char *destination,
    size_t destination_size
) {
    const unsigned char *source = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, index) != SQLITE_TEXT) {
        return MYLITE_ERROR;
    }

    source = sqlite3_column_text(statement, index);
    byte_count = sqlite3_column_bytes(statement, index);
    if (source == NULL || byte_count < 0 || (size_t)byte_count >= destination_size) {
        return MYLITE_ERROR;
    }

    memcpy(destination, source, (size_t)byte_count);
    destination[(size_t)byte_count] = '\0';

    return MYLITE_OK;
}

static int checked_nullable_column_text(
    sqlite3_stmt *statement,
    int index,
    bool *out_has_value,
    char *destination,
    size_t destination_size
) {
    *out_has_value = false;
    if (destination != NULL && destination_size > 0U) {
        destination[0] = '\0';
    }
    if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }

    int rc = checked_column_text(statement, index, destination, destination_size);

    if (rc == MYLITE_OK) {
        *out_has_value = true;
    }
    return rc;
}

static int validate_database(struct mylite_db *database) {
    if (database == NULL || database->sqlite == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_catalog_ready_database(struct mylite_db *database) {
    int rc = validate_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!database->catalog.initialized) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_required_name(const char *name, size_t capacity) {
    size_t length = 0U;

    if (name == NULL || name[0] == '\0') {
        return MYLITE_MISUSE;
    }

    length = strlen(name);
    if (length >= capacity) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_optional_name(const char *name, size_t capacity) {
    if (name == NULL || name[0] == '\0') {
        return MYLITE_OK;
    }

    return validate_required_name(name, capacity);
}

static int validate_logical_object_name(const char *name, size_t capacity) {
    int rc = validate_required_name(name, capacity);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(name)) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_table_kind(enum mylite_catalog_table_kind kind) {
    if (kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_column_default_kind(enum mylite_catalog_column_default_kind kind) {
    if (kind != MYLITE_CATALOG_COLUMN_DEFAULT_NONE &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_TEXT &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_BINARY &&
        kind != MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_catalog_column_values(
    const struct catalog_column_values *values,
    bool use_logical_object_name
) {
    int rc = MYLITE_OK;

    if (use_logical_object_name) {
        rc = validate_logical_object_name(values->name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    } else {
        rc = validate_required_name(values->name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(values->logical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_required_name(values->physical_type, MYLITE_CATALOG_TYPE_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_optional_name(values->character_set_name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_optional_name(values->collation_name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_optional_name(values->comment, MYLITE_CATALOG_COLUMN_COMMENT_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = validate_catalog_column_default_value(values);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return validate_catalog_generated_column_value(values);
}

static int validate_catalog_column_default_value(const struct catalog_column_values *values) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (values == NULL) {
        return MYLITE_MISUSE;
    }
    rc = validate_column_default_kind(values->default_kind);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP) {
        return validate_catalog_current_timestamp_default_value(values);
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE) {
        return validate_catalog_current_date_default_value(values);
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME) {
        return validate_catalog_current_time_default_value(values);
    }
    if (values->on_update_current_timestamp &&
        !catalog_logical_type_accepts_current_timestamp(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (!catalog_default_kind_stores_text(values->default_kind)) {
        return MYLITE_OK;
    }
    rc = catalog_default_text_length(values->default_text, &text_length);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return validate_catalog_text_default_value(values, text_length);
}

static int validate_catalog_generated_column_value(const struct catalog_column_values *values) {
    size_t expression_length = 0U;
    size_t sqlite_expression_length = 0U;

    if (values == NULL) {
        return MYLITE_MISUSE;
    }
    if (!values->is_generated) {
        if (values->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_INVALID ||
            values->generation_expression == NULL || values->generation_expression[0] != '\0' ||
            values->sqlite_generation_expression == NULL ||
            values->sqlite_generation_expression[0] != '\0') {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL &&
        values->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_STORED) {
        return MYLITE_MISUSE;
    }
    if (values->generation_expression == NULL || values->sqlite_generation_expression == NULL) {
        return MYLITE_MISUSE;
    }

    expression_length = strlen(values->generation_expression);
    sqlite_expression_length = strlen(values->sqlite_generation_expression);
    if (expression_length == 0U ||
        expression_length >= MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY ||
        sqlite_expression_length == 0U ||
        sqlite_expression_length >= MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_catalog_current_timestamp_default_value(
    const struct catalog_column_values *values
) {
    if (!catalog_logical_type_accepts_current_timestamp(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (values->default_text != NULL && values->default_text[0] != '\0') {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}

static int validate_catalog_current_date_default_value(const struct catalog_column_values *values) {
    if (!catalog_logical_type_accepts_current_date(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (values->default_text != NULL && values->default_text[0] != '\0') {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}

static int validate_catalog_current_time_default_value(const struct catalog_column_values *values) {
    if (!catalog_logical_type_accepts_current_time(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (values->default_text != NULL && values->default_text[0] != '\0') {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}

static int catalog_default_text_length(const char *default_text, size_t *out_text_length) {
    size_t text_length = 0U;

    if (default_text == NULL) {
        return MYLITE_MISUSE;
    }
    for (; text_length < MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY; ++text_length) {
        if (default_text[text_length] == '\0') {
            break;
        }
    }
    if (text_length == MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY) {
        return MYLITE_MISUSE;
    }
    *out_text_length = text_length;
    return MYLITE_OK;
}

static int validate_catalog_text_default_value(
    const struct catalog_column_values *values,
    size_t text_length
) {
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL) {
        if (text_length == 0U ||
            !text_has_ascii_case_insensitive_prefix(values->logical_type, "DECIMAL(")) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        if (text_length == 0U ||
            !catalog_logical_type_accepts_integer_expression_default(values->logical_type)) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        if ((!catalog_logical_type_accepts_integer_expression_default(values->logical_type) &&
             !catalog_logical_type_accepts_text_expression_default(values->logical_type) &&
             !catalog_logical_type_is_text_family(values->logical_type) &&
             !catalog_logical_type_is_binary_blob_family(values->logical_type)) ||
            strcmp(values->default_text, "NULL") != 0) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        if (!catalog_logical_type_accepts_binary_default(values->logical_type) ||
            !catalog_default_text_is_hex(values->default_text, text_length)) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (values->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        if (!catalog_logical_type_accepts_text_expression_default(values->logical_type)) {
            return MYLITE_MISUSE;
        }
        return MYLITE_OK;
    }
    if (!catalog_logical_type_accepts_text_default(values->logical_type)) {
        return MYLITE_MISUSE;
    }
    if (text_length == 0U &&
        !catalog_logical_type_accepts_empty_text_default(values->logical_type)) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static bool catalog_default_kind_stores_integer(
    enum mylite_catalog_column_default_kind default_kind
) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) != 0;
}

static bool catalog_default_kind_stores_text(enum mylite_catalog_column_default_kind default_kind) {
    return (default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION ||
            default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) != 0;
}

static bool catalog_logical_type_accepts_integer_expression_default(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "TINYINT") ||
            catalog_logical_type_equals(logical_type, "TINYINT(1)") ||
            catalog_logical_type_equals(logical_type, "TINYINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "SMALLINT") ||
            catalog_logical_type_equals(logical_type, "SMALLINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "MEDIUMINT") ||
            catalog_logical_type_equals(logical_type, "MEDIUMINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "INT") ||
            catalog_logical_type_equals(logical_type, "INT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "BIGINT") ||
            catalog_logical_type_equals(logical_type, "BIGINT UNSIGNED") ||
            catalog_logical_type_equals(logical_type, "YEAR") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "BIT(")) != 0;
}

static bool catalog_logical_type_accepts_text_expression_default(const char *logical_type) {
    return (text_has_ascii_case_insensitive_prefix(logical_type, "CHAR(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "VARCHAR(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "NCHAR(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "NVARCHAR(")) != 0;
}

static bool catalog_logical_type_accepts_current_timestamp(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "DATETIME") ||
            catalog_logical_type_equals(logical_type, "TIMESTAMP")) != 0;
}

static bool catalog_logical_type_accepts_current_date(const char *logical_type) {
    return catalog_logical_type_equals(logical_type, "DATE");
}

static bool catalog_logical_type_accepts_current_time(const char *logical_type) {
    return catalog_logical_type_equals(logical_type, "TIME");
}

static bool catalog_logical_type_accepts_text_default(const char *logical_type) {
    if (catalog_logical_type_accepts_empty_text_default(logical_type)) {
        return true;
    }
    if (catalog_logical_type_is_text_family(logical_type)) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "ENUM(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "SET(") != 0) {
        return true;
    }
    if (catalog_logical_type_is_bit(logical_type)) {
        return true;
    }
    if (catalog_logical_type_equals(logical_type, "DATE") ||
        catalog_logical_type_equals(logical_type, "TIME") ||
        catalog_logical_type_equals(logical_type, "DATETIME") ||
        catalog_logical_type_equals(logical_type, "TIMESTAMP") ||
        catalog_logical_type_equals(logical_type, "YEAR") ||
        catalog_logical_type_equals(logical_type, "FLOAT") ||
        catalog_logical_type_equals(logical_type, "FLOAT UNSIGNED") ||
        catalog_logical_type_equals(logical_type, "DOUBLE") ||
        catalog_logical_type_equals(logical_type, "DOUBLE UNSIGNED")) {
        return true;
    }

    return false;
}

static bool catalog_logical_type_accepts_binary_default(const char *logical_type) {
    return (text_has_ascii_case_insensitive_prefix(logical_type, "BINARY(") ||
            text_has_ascii_case_insensitive_prefix(logical_type, "VARBINARY(") ||
            catalog_logical_type_is_binary_blob_family(logical_type)) != 0;
}

static bool catalog_logical_type_is_binary_blob_family(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "TINYBLOB") ||
            catalog_logical_type_equals(logical_type, "BLOB") ||
            catalog_logical_type_equals(logical_type, "MEDIUMBLOB") ||
            catalog_logical_type_equals(logical_type, "LONGBLOB")) != 0;
}

static bool catalog_default_text_is_hex(const char *text, size_t text_length) {
    if (text == NULL || (text_length % 2U) != 0U) {
        return false;
    }

    for (size_t index = 0U; index < text_length; ++index) {
        char byte = text[index];

        if ((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'F')) {
            continue;
        }
        return false;
    }
    return true;
}

static bool catalog_logical_type_is_bit(const char *logical_type) {
    size_t index = 0U;

    if (logical_type == NULL || text_has_ascii_case_insensitive_prefix(logical_type, "BIT(") == 0) {
        return false;
    }
    index = sizeof("BIT(") - 1U;
    if (logical_type[index] < '1' || logical_type[index] > '9') {
        return false;
    }
    while (logical_type[index] >= '0' && logical_type[index] <= '9') {
        ++index;
    }

    if (logical_type[index] != ')') {
        return false;
    }
    if (logical_type[index + 1U] != '\0') {
        return false;
    }
    return true;
}

static bool catalog_logical_type_is_text_family(const char *logical_type) {
    return (catalog_logical_type_equals(logical_type, "TINYTEXT") ||
            catalog_logical_type_equals(logical_type, "TEXT") ||
            catalog_logical_type_equals(logical_type, "MEDIUMTEXT") ||
            catalog_logical_type_equals(logical_type, "LONGTEXT")) != 0;
}

static bool catalog_logical_type_accepts_empty_text_default(const char *logical_type) {
    if (catalog_logical_type_is_text_family(logical_type)) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "CHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "NCHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "VARCHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "NVARCHAR(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "ENUM(") != 0) {
        return true;
    }
    if (text_has_ascii_case_insensitive_prefix(logical_type, "SET(") != 0) {
        return true;
    }

    return false;
}

static bool catalog_logical_type_equals(const char *logical_type, const char *expected) {
    size_t index = 0U;

    if (logical_type == NULL || expected == NULL) {
        return false;
    }
    for (; logical_type[index] != '\0' && expected[index] != '\0'; ++index) {
        if (ascii_lower((unsigned char)logical_type[index]) !=
            ascii_lower((unsigned char)expected[index])) {
            return false;
        }
    }

    if (logical_type[index] == '\0' && expected[index] == '\0') {
        return true;
    }

    return false;
}

static int validate_catalog_bool_i64(int64_t value, bool *out_bool) {
    if (value != 0 && value != 1) {
        return MYLITE_ERROR;
    }

    *out_bool = value != 0;
    return MYLITE_OK;
}

static int validate_index_kind(enum mylite_catalog_index_kind kind) {
    if (kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY && kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
        kind != MYLITE_CATALOG_INDEX_KIND_FULLTEXT && kind != MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_active_mutation(const struct mylite_catalog_mutation *mutation) {
    if (mutation == NULL || !mutation->active || mutation->next_generation == 0U) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_positive_id(int64_t id) {
    if (id <= 0) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_positive_ordinal(int64_t ordinal_position) {
    if (ordinal_position <= 0) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_generation(uint64_t generation) {
    int64_t signed_generation = 0;

    if (generation == 0U) {
        return MYLITE_ERROR;
    }

    return u64_to_i64(generation, &signed_generation);
}

static int validate_schema_callback(mylite_catalog_schema_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_callback(mylite_catalog_table_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_column_callback(mylite_catalog_column_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_index_callback(mylite_catalog_index_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_index_column_callback(mylite_catalog_index_column_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_foreign_key_callback(mylite_catalog_foreign_key_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_foreign_key_column_callback(
    mylite_catalog_foreign_key_column_callback callback
) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int validate_check_constraint_callback(mylite_catalog_check_constraint_callback callback) {
    if (callback == NULL) {
        return MYLITE_MISUSE;
    }

    return MYLITE_OK;
}

static int u64_to_i64(uint64_t value, int64_t *out_value) {
    if (value > INT64_MAX) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)value;

    return MYLITE_OK;
}

static int i64_to_u32(int64_t value, uint32_t *out_value) {
    if (value < 0 || value > UINT32_MAX) {
        return MYLITE_ERROR;
    }

    *out_value = (uint32_t)value;

    return MYLITE_OK;
}

static int i64_to_u64(int64_t value, uint64_t *out_value) {
    if (value < 0) {
        return MYLITE_ERROR;
    }

    *out_value = (uint64_t)value;

    return MYLITE_OK;
}

static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix) {
    size_t index = 0U;

    while (prefix[index] != '\0') {
        if (text[index] == '\0' ||
            ascii_lower((unsigned char)text[index]) != ascii_lower((unsigned char)prefix[index])) {
            return 0;
        }
        ++index;
    }

    return 1;
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}

static void reset_descriptor_cache_state(struct mylite_catalog *catalog) {
    catalog->cached_generation = 0U;
    catalog->descriptor_cache_is_valid = false;
}

static const char *catalog_state_table_name(void) {
    return "_mylite_catalog_state";
}

static const char *catalog_schemas_table_name(void) {
    return "_mylite_catalog_schemas";
}

static const char *catalog_tables_table_name(void) {
    return "_mylite_catalog_tables";
}

static const char *catalog_columns_table_name(void) {
    return "_mylite_catalog_columns";
}

static const char *catalog_indexes_table_name(void) {
    return "_mylite_catalog_indexes";
}

static const char *catalog_index_columns_table_name(void) {
    return "_mylite_catalog_index_columns";
}

static const char *catalog_foreign_keys_table_name(void) {
    return "_mylite_catalog_foreign_keys";
}

static const char *catalog_foreign_key_columns_table_name(void) {
    return "_mylite_catalog_foreign_key_columns";
}

static const char *catalog_check_constraints_table_name(void) {
    return "_mylite_catalog_check_constraints";
}
