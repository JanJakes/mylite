#include "mylite_catalog.h"

#include "mylite_catalog_internal.h"

#include "mylite_connection.h"
#include "mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    catalog_table_key_block_size_eight = 8,
    catalog_table_key_block_size_sixteen = 16,
    catalog_table_stats_sample_pages_max = 65535,
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
    catalog_table_insert_in_mutation_row_format_bind = 10,
    catalog_table_insert_in_mutation_key_block_size_bind = 11,
    catalog_table_insert_in_mutation_pack_keys_bind = 12,
    catalog_table_insert_in_mutation_checksum_bind = 13,
    catalog_table_insert_in_mutation_stats_persistent_bind = 14,
    catalog_table_insert_in_mutation_stats_auto_recalc_bind = 15,
    catalog_table_insert_in_mutation_stats_sample_pages_bind = 16,
    catalog_table_insert_in_mutation_created_time_bind = 17,
    catalog_table_insert_in_mutation_updated_time_bind = 18,
    catalog_table_insert_in_mutation_generation_bind = 19,
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

enum catalog_view_insert_bind_index {
    catalog_view_insert_table_id_bind = 1,
    catalog_view_insert_view_definition_bind = 2,
    catalog_view_insert_show_create_sql_bind = 3,
    catalog_view_insert_check_option_bind = 4,
    catalog_view_insert_is_updatable_bind = 5,
    catalog_view_insert_definer_bind = 6,
    catalog_view_insert_security_type_bind = 7,
    catalog_view_insert_character_set_client_bind = 8,
    catalog_view_insert_collation_connection_bind = 9,
    catalog_view_insert_source_schema_id_bind = 10,
    catalog_view_insert_source_table_id_bind = 11,
    catalog_view_insert_source_schema_name_bind = 12,
    catalog_view_insert_source_table_name_bind = 13,
    catalog_view_insert_generation_bind = 14,
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
    catalog_table_select_row_format_column = 9,
    catalog_table_select_key_block_size_column = 10,
    catalog_table_select_pack_keys_column = 11,
    catalog_table_select_checksum_column = 12,
    catalog_table_select_stats_persistent_column = 13,
    catalog_table_select_stats_auto_recalc_column = 14,
    catalog_table_select_stats_sample_pages_column = 15,
    catalog_table_select_fulltext_doc_id_initialized_column = 16,
    catalog_table_select_created_time_column = 17,
    catalog_table_select_updated_time_column = 18,
    catalog_table_select_descriptor_version_column = 19,
    catalog_table_select_created_generation_column = 20,
    catalog_table_select_updated_generation_column = 21,
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

enum catalog_view_select_column_index {
    catalog_view_select_table_id_column = 0,
    catalog_view_select_view_definition_column = 1,
    catalog_view_select_show_create_sql_column = 2,
    catalog_view_select_check_option_column = 3,
    catalog_view_select_is_updatable_column = 4,
    catalog_view_select_definer_column = 5,
    catalog_view_select_security_type_column = 6,
    catalog_view_select_character_set_client_column = 7,
    catalog_view_select_collation_connection_column = 8,
    catalog_view_select_source_schema_id_column = 9,
    catalog_view_select_source_table_id_column = 10,
    catalog_view_select_source_schema_name_column = 11,
    catalog_view_select_source_table_name_column = 12,
    catalog_view_select_descriptor_version_column = 13,
    catalog_view_select_created_generation_column = 14,
    catalog_view_select_updated_generation_column = 15,
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

enum catalog_schema_select_column_index {
    catalog_schema_select_schema_id_column = 0,
    catalog_schema_select_name_column = 1,
    catalog_schema_select_default_charset_column = 2,
    catalog_schema_select_default_collation_column = 3,
    catalog_schema_select_descriptor_version_column = 4,
    catalog_schema_select_created_generation_column = 5,
    catalog_schema_select_updated_generation_column = 6,
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

struct catalog_view_values {
    const char *view_definition;
    const char *show_create_sql;
    const char *check_option;
    const char *is_updatable;
    const char *definer;
    const char *security_type;
    const char *character_set_client;
    const char *collation_connection;
    int64_t source_schema_id;
    int64_t source_table_id;
    const char *source_schema_name;
    const char *source_table_name;
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

static int validate_catalog_table_descriptor_input(
    const struct catalog_table_descriptor_input *input
);
static int bind_catalog_table_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t auto_increment_next,
    const struct catalog_table_descriptor_input *values,
    uint64_t generation
);
static int bind_catalog_table_insert_identity_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct catalog_table_descriptor_input *values,
    int64_t auto_increment_next
);
static int bind_catalog_table_insert_default_values(
    sqlite3_stmt *statement,
    const struct catalog_table_descriptor_input *values
);
static int bind_catalog_table_insert_storage_statistics_values(
    sqlite3_stmt *statement,
    const struct catalog_table_descriptor_input *values
);
static int bind_catalog_table_insert_lifecycle_values(
    sqlite3_stmt *statement,
    const struct catalog_table_descriptor_input *values,
    uint64_t generation
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
static int bind_catalog_view_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct catalog_view_values *values,
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
static int read_view_by_table_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_view_descriptor *out_view
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
static int materialize_table_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int materialize_table_storage_statistics(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int materialize_table_lifecycle(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
);
static int validate_materialized_table(const struct mylite_catalog_table_descriptor *table);
static int materialize_view(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int materialize_view_text_fields(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int materialize_view_source(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int materialize_view_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
);
static int validate_materialized_view(const struct mylite_catalog_view_descriptor *view);
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
static int text_equals_ascii_case_insensitive(const char *left, const char *right);
static int text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);

int mylite_catalog_allocate_table_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_table_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_table_id != NULL) {
        *out_table_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_index_id != NULL) {
        *out_index_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (out_foreign_key_id != NULL) {
        *out_foreign_key_id = 0;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
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
    int64_t table_id, // NOLINT(bugprone-easily-swappable-parameters)
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind, // NOLINT(bugprone-easily-swappable-parameters)
    int64_t auto_increment_next,
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    const char *row_format_option,
    int64_t key_block_size,
    int64_t pack_keys,
    int64_t checksum,
    int64_t stats_persistent,
    int64_t stats_auto_recalc,
    int64_t stats_sample_pages,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
    struct mylite_catalog_table_descriptor *out_table
) {
    const struct catalog_table_descriptor_input descriptor = {
        .schema_id = schema_id,
        .name = name,
        .physical_name = physical_name,
        .kind = kind,
        .default_charset = default_charset,
        .default_collation = default_collation,
        .comment = comment,
        .row_format_option = row_format_option,
        .key_block_size = key_block_size,
        .pack_keys = pack_keys,
        .checksum = checksum,
        .stats_persistent = stats_persistent,
        .stats_auto_recalc = stats_auto_recalc,
        .stats_sample_pages = stats_sample_pages,
        .created_time_utc_epoch = created_time_utc_epoch,
        .updated_time_utc_epoch = updated_time_utc_epoch,
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }
    rc = validate_catalog_table_descriptor_input(&descriptor);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_tables "
        "(table_id, schema_id, name, kind, physical_name, auto_increment_next, default_charset, "
        "default_collation, comment, row_format_option, key_block_size, pack_keys, checksum, "
        "stats_persistent, stats_auto_recalc, stats_sample_pages, fulltext_doc_id_initialized, "
        "created_time_utc_epoch, "
        "updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, 0, "
        "?17, ?18, 1, ?19, ?19)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_values(
            statement,
            table_id,
            auto_increment_next,
            &descriptor,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_table != NULL) {
        return read_table_by_id(database->sqlite, table_id, out_table);
    }

    return MYLITE_OK;
}

static int bind_catalog_table_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t auto_increment_next,
    const struct catalog_table_descriptor_input *values,
    uint64_t generation
) {
    int rc =
        bind_catalog_table_insert_identity_values(statement, table_id, values, auto_increment_next);

    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_default_values(statement, values);
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_storage_statistics_values(statement, values);
    }
    if (rc == MYLITE_OK) {
        rc = bind_catalog_table_insert_lifecycle_values(statement, values, generation);
    }
    return rc;
}

static int bind_catalog_table_insert_identity_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct catalog_table_descriptor_input *values,
    int64_t auto_increment_next
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_i64(
                                  statement,
                                  catalog_table_insert_in_mutation_table_id_bind,
                                  table_id
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_schema_id_bind,
            values->schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_name_bind,
            values->name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_kind_bind,
            (int64_t)values->kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_physical_name_bind,
            values->physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_auto_increment_next_bind,
            auto_increment_next
        );
    }
    return rc;
}

static int bind_catalog_table_insert_default_values(
    sqlite3_stmt *statement,
    const struct catalog_table_descriptor_input *values
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_text(
                                  statement,
                                  catalog_table_insert_in_mutation_default_charset_bind,
                                  values->default_charset
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_default_collation_bind,
            values->default_collation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_in_mutation_comment_bind,
            values->comment
        );
    }
    return rc;
}

static int bind_catalog_table_insert_storage_statistics_values(
    sqlite3_stmt *statement,
    const struct catalog_table_descriptor_input *values
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_text(
                                  statement,
                                  catalog_table_insert_in_mutation_row_format_bind,
                                  values->row_format_option
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_key_block_size_bind,
            values->key_block_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_pack_keys_bind,
            values->pack_keys
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_checksum_bind,
            values->checksum
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_stats_persistent_bind,
            values->stats_persistent
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_stats_auto_recalc_bind,
            values->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_stats_sample_pages_bind,
            values->stats_sample_pages
        );
    }
    return rc;
}

static int bind_catalog_table_insert_lifecycle_values(
    sqlite3_stmt *statement,
    const struct catalog_table_descriptor_input *values,
    uint64_t generation
) {
    int rc = values == NULL ? MYLITE_MISUSE
                            : mylite_catalog_bind_i64(
                                  statement,
                                  catalog_table_insert_in_mutation_created_time_bind,
                                  values->created_time_utc_epoch
                              );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_in_mutation_updated_time_bind,
            values->updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_table_insert_in_mutation_generation_bind,
            generation
        );
    }
    return rc;
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_values(&values, false);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_column != NULL) {
        return read_column_by_name(database->sqlite, table_id, name, out_column);
    }

    return MYLITE_OK;
}

int mylite_catalog_insert_view_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *view_definition,
    const char *show_create_sql,
    const char *check_option,
    const char *is_updatable,
    const char *definer,
    const char *security_type,
    const char *character_set_client,
    const char *collation_connection,
    int64_t source_schema_id,
    int64_t source_table_id,
    const char *source_schema_name,
    const char *source_table_name,
    struct mylite_catalog_view_descriptor *out_view
) {
    const struct catalog_view_values values = {
        .view_definition = view_definition,
        .show_create_sql = show_create_sql,
        .check_option = check_option,
        .is_updatable = is_updatable,
        .definer = definer,
        .security_type = security_type,
        .character_set_client = character_set_client,
        .collation_connection = collation_connection,
        .source_schema_id = source_schema_id,
        .source_table_id = source_table_id,
        .source_schema_name = source_schema_name,
        .source_table_name = source_table_name,
    };
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_view != NULL) {
        *out_view = (struct mylite_catalog_view_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view_definition,
            MYLITE_CATALOG_VIEW_DEFINITION_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            show_create_sql,
            MYLITE_CATALOG_VIEW_SHOW_CREATE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(check_option, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(is_updatable, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(definer, MYLITE_CATALOG_DEFINER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            security_type,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            character_set_client,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            collation_connection,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(source_schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(source_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            source_schema_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            source_table_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_views "
        "(table_id, view_definition, show_create_sql, check_option, is_updatable, definer, "
        "security_type, character_set_client, collation_connection, source_schema_id, "
        "source_table_id, source_schema_name, source_table_name, descriptor_version, "
        "created_catalog_generation, "
        "updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, 1, ?14, ?14)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = bind_catalog_view_insert_values(
            statement,
            table_id,
            &values,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (out_view != NULL) {
        return read_view_by_table_id(database->sqlite, table_id, out_view);
    }

    return MYLITE_OK;
}

static int bind_catalog_view_insert_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    const struct catalog_view_values *values,
    uint64_t generation
) {
    int rc = values == NULL
                 ? MYLITE_MISUSE
                 : mylite_catalog_bind_i64(statement, catalog_view_insert_table_id_bind, table_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_view_definition_bind,
            values->view_definition
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_show_create_sql_bind,
            values->show_create_sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_check_option_bind,
            values->check_option
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_is_updatable_bind,
            values->is_updatable
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_view_insert_definer_bind, values->definer);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_security_type_bind,
            values->security_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_character_set_client_bind,
            values->character_set_client
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_collation_connection_bind,
            values->collation_connection
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_view_insert_source_schema_id_bind,
            values->source_schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_view_insert_source_table_id_bind,
            values->source_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_source_schema_name_bind,
            values->source_schema_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_view_insert_source_table_name_bind,
            values->source_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, catalog_view_insert_generation_bind, generation);
    }
    return rc;
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        mylite_catalog_validate_required_name(physical_name, MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_index_kind(kind);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(comment, MYLITE_CATALOG_INDEX_COMMENT_CAPACITY);
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
    int64_t unique_value = mylite_catalog_bool_value(is_unique);
    int64_t visible_value = mylite_catalog_bool_value(is_visible);
    int64_t explicit_btree_value = mylite_catalog_bool_value(show_create_explicit_btree);
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "INSERT INTO _mylite_catalog_indexes "
        "(index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, ?10, ?10)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_index_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_index_insert_is_unique_bind, unique_value);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_bind_i64(statement, catalog_index_insert_is_visible_bind, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_index_insert_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_index_insert_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_insert_show_create_explicit_btree_bind,
            explicit_btree_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_index_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
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

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET fulltext_doc_id_initialized = 1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?1 "
        "WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (prefix_length != NULL) {
        rc = mylite_catalog_validate_positive_ordinal(*prefix_length);
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

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_index_columns "
        "(index_id, table_id, column_id, ordinal_position, prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, ?7, ?7)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_bind_i64(statement, catalog_index_column_insert_index_id_bind, index_id);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_bind_i64(statement, catalog_index_column_insert_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_column_insert_column_id_bind,
            column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_nullable_i64(
            statement,
            catalog_index_column_insert_prefix_length_bind,
            has_prefix_length,
            prefix_length_value
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_index_column_insert_sort_direction_bind,
            (int64_t)sort_direction
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_index_column_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_inserted_index_column(
    struct mylite_db *database,
    int64_t index_id,
    int64_t ordinal_position,
    struct mylite_catalog_index_column_descriptor *out_index_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 AND ordinal_position = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, ordinal_position);
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

    return mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(update_rule, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(delete_rule, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(match_option, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_foreign_keys "
        "(foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, ?10, ?10)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_foreign_key_id_bind,
            foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_child_table_id_bind,
            child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_parent_table_id_bind,
            parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_foreign_key_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_parent_index_id_bind,
            parent_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_insert_child_index_id_bind,
            child_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_key_insert_update_rule_bind,
            update_rule
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_key_insert_delete_rule_bind,
            delete_rule
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_foreign_key_insert_match_option_bind,
            match_option
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_foreign_key_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(child_column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(parent_column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(position_in_unique_constraint);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            check_clause,
            MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            sqlite_expression,
            MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(generated_ordinal);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_check_constraints "
        "(check_constraint_id, table_id, name, physical_name, check_clause, sqlite_expression, "
        "is_enforced, name_is_generated, generated_ordinal, ordinal_position, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1, ?11, ?11)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_check_constraint_id_bind,
            check_constraint_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_table_id_bind,
            table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_check_constraint_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_check_constraint_insert_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_check_constraint_insert_check_clause_bind,
            check_clause
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_check_constraint_insert_sqlite_expression_bind,
            sqlite_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_is_enforced_bind,
            mylite_catalog_bool_value(is_enforced)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_name_is_generated_bind,
            mylite_catalog_bool_value(name_is_generated)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_generated_ordinal_bind,
            generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_check_constraint_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_check_constraint_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_foreign_key_columns "
        "(foreign_key_id, child_table_id, parent_table_id, child_column_id, "
        "parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 1, ?8, ?8)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_foreign_key_id_bind,
            foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_child_table_id_bind,
            child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_parent_table_id_bind,
            parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_child_column_id_bind,
            child_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_parent_column_id_bind,
            parent_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_foreign_key_column_insert_position_in_unique_constraint_bind,
            position_in_unique_constraint
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_foreign_key_column_insert_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_inserted_foreign_key(
    struct mylite_db *database,
    int64_t child_table_id,
    const char *name,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_inserted_foreign_key_column(
    struct mylite_db *database,
    int64_t foreign_key_id,
    int64_t ordinal_position,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 AND ordinal_position = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, ordinal_position);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_inserted_check_constraint(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 AND name = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1 AND index_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1 AND index_id = ?2",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_index_columns "
        "WHERE table_id = ?1 AND index_id = ?2 AND column_id = ?3 AND ordinal_position = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "UPDATE _mylite_catalog_indexes "
            "SET descriptor_version = descriptor_version + 1, "
            "updated_catalog_generation = ?1 "
            "WHERE table_id = ?2 AND index_id = ?3",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_rename_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    const char *name
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_indexes "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND index_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_set_index_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    bool is_visible
) {
    sqlite3_stmt *statement = NULL;
    int64_t visible_value = mylite_catalog_bool_value(is_visible);
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(index_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_indexes "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND index_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, index_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_keys_for_child_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns WHERE child_table_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_foreign_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id,
    int64_t foreign_key_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id = ?1 AND foreign_key_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id = ?1 AND foreign_key_id = ?2",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, foreign_key_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_check_constraints_for_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(check_constraint_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_check_constraints "
        "WHERE table_id = ?1 AND check_constraint_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_update_check_constraint_enforcement_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t check_constraint_id,
    bool is_enforced
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(check_constraint_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_check_constraints "
        "SET is_enforced = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND check_constraint_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, mylite_catalog_bool_value(is_enforced));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, check_constraint_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_rename_generated_check_constraints_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *table_name
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_active_mutation(mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_logical_object_name(
            table_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_check_constraints "
        "SET name = ?1 || '_chk_' || generated_ordinal, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND name_is_generated = 1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_related_table(database->sqlite, table_id);
    if (rc == MYLITE_OK) {
        rc = delete_check_constraints_for_table(database->sqlite, table_id);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1 AND column_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_u64(statement, 1, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, ordinal_position);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_rename_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_values(&values, true);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_reorder_columns_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
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
    int rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET ordinal_position = ordinal_position + ?1 WHERE table_id = ?2",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_offset_bind,
            (int64_t)reorder->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_offset_table_id_bind,
            reorder->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK &&
        sqlite3_changes64(database->sqlite) != (sqlite3_int64)reorder->column_count) {
        rc = MYLITE_ERROR;
    }

    return mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_positive_id(column->column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(final_ordinal);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, catalog_column_reorder_ordinal_bind, final_ordinal);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_version_increment_bind,
            increment_descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_column_reorder_generation_bind,
            mutation->next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_table_id_bind,
            reorder->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_reorder_column_id_bind,
            column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int validate_catalog_column_reorder_request(
    const struct mylite_catalog_column_reorder *reorder
) {
    int rc = MYLITE_OK;

    if (reorder == NULL || reorder->columns == NULL || reorder->column_count == 0U ||
        reorder->column_count > (size_t)(INT64_MAX / 2)) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(reorder->table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(reorder->metadata_replaced_column_id);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (is_visible) {
        visible_value = 1;
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_columns "
        "SET is_visible = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3 AND column_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, visible_value);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_delete_schema_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_schema(database->sqlite, schema_id);
    if (rc == MYLITE_OK) {
        rc = delete_check_constraints_for_schema(database->sqlite, schema_id);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int delete_foreign_keys_for_related_table(sqlite3 *sqlite, int64_t table_id) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_foreign_key_columns "
        "WHERE child_table_id = ?1 OR parent_table_id = ?1",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            sqlite,
            "DELETE FROM _mylite_catalog_foreign_keys "
            "WHERE child_table_id = ?1 OR parent_table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int delete_foreign_keys_for_schema(sqlite3 *sqlite, int64_t schema_id) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    statement = NULL;

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int delete_check_constraints_for_table(sqlite3 *sqlite, int64_t table_id) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_check_constraints WHERE table_id = ?1",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int delete_check_constraints_for_schema(sqlite3 *sqlite, int64_t schema_id) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "DELETE FROM _mylite_catalog_check_constraints "
        "WHERE table_id IN (SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1)",
        &statement
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }

    return mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET schema_id = ?1, name = ?2, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?3 "
        "WHERE table_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        default_collation,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET default_charset = ?1, default_collation = ?2, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?3 "
        "WHERE table_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(comment, MYLITE_CATALOG_TABLE_COMMENT_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET comment = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, comment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_active_mutation(mutation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        default_collation,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_schemas "
        "SET default_charset = ?1, default_collation = ?2, "
        "descriptor_version = descriptor_version + 1, updated_catalog_generation = ?3 "
        "WHERE schema_id = ?4",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 3, mutation->next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 4, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET auto_increment_next = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, auto_increment_next);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (updated_time_utc_epoch < 0) {
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables SET updated_time_utc_epoch = ?1 WHERE table_id = ?2",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, updated_time_utc_epoch);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 2, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_schema_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_table_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_table_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_table_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, row_format_option, key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_column_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_index_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_index_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_id, table_id, name, kind, is_unique, is_visible, physical_name, "
        "comment, show_create_explicit_btree, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_indexes WHERE table_id = ?1 ORDER BY index_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_index_column_in_index(
    struct mylite_db *database,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(index_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_index_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT index_column_id, index_id, table_id, column_id, ordinal_position, "
        "prefix_length, sort_direction, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_index_columns "
        "WHERE index_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, index_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_in_child_table(
    struct mylite_db *database,
    int64_t child_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(child_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE child_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, child_table_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_for_parent_table(
    struct mylite_db *database,
    int64_t parent_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(parent_table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_id, child_table_id, parent_table_id, name, parent_index_id, "
        "child_index_id, update_rule, delete_rule, match_option, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_keys WHERE parent_table_id = ?1 ORDER BY foreign_key_id",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, parent_table_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_foreign_key_column_in_foreign_key(
    struct mylite_db *database,
    int64_t foreign_key_id,
    mylite_catalog_foreign_key_column_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(foreign_key_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_foreign_key_column_callback(callback);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT foreign_key_column_id, foreign_key_id, child_table_id, parent_table_id, "
        "child_column_id, parent_column_id, ordinal_position, position_in_unique_constraint, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_foreign_key_columns "
        "WHERE foreign_key_id = ?1 ORDER BY ordinal_position",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, foreign_key_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "SELECT check_constraint_id, table_id, name, physical_name, check_clause, "
        "sqlite_expression, is_enforced, name_is_generated, generated_ordinal, "
        "ordinal_position, descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_check_constraints WHERE table_id = ?1 ORDER BY name",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_for_each_check_constraint_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_check_constraint_callback(callback);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_check_constraint_by_physical_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int rc = mylite_catalog_validate_ready_database(database);

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
        rc = mylite_catalog_validate_positive_id(table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, physical_name);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

int mylite_catalog_try_read_primary_index_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_index == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = mylite_catalog_validate_positive_id(table_id);
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
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_schema != NULL) {
        *out_schema = (struct mylite_catalog_schema_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(default_charset, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        default_collation,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "INSERT INTO _mylite_catalog_schemas "
        "(name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation) "
        "VALUES (?1, ?2, ?3, 1, ?4, ?4)",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, default_charset);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 3, default_collation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 4, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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

int mylite_catalog_read_schema_by_id(
    struct mylite_db *database,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(schema_id);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_schema_by_id(database->sqlite, schema_id, out_schema);
}

int mylite_catalog_try_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_schema == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return try_read_schema_by_name(database->sqlite, name, out_schema, out_found);
}

int mylite_catalog_delete_schema(struct mylite_db *database, int64_t schema_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_schema(database->sqlite, schema_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns "
            "WHERE table_id IN ("
            "SELECT table_id FROM _mylite_catalog_tables WHERE schema_id = ?1"
            ")",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_schemas WHERE schema_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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
    struct mylite_catalog_generation_change generation = {0};
    struct mylite_catalog_schema_descriptor schema = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_table != NULL) {
        *out_table = (struct mylite_catalog_table_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
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

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = read_schema_by_id(database->sqlite, schema_id, &schema);
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_schema_id_bind, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_table_insert_name_bind, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_kind_bind, (int64_t)kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_physical_name_bind,
            physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_table_insert_auto_increment_next_bind, 1);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_default_charset_bind,
            default_charset
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_table_insert_default_collation_bind,
            default_collation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_table_insert_comment_bind, comment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_created_time_bind,
            created_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_table_insert_updated_time_bind,
            updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(
            statement,
            catalog_table_insert_generation_bind,
            generation.next_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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
    int rc = input == NULL ? MYLITE_MISUSE : mylite_catalog_validate_positive_id(input->schema_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_logical_object_name(
            input->name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->physical_name,
            MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_table_kind(input->kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->default_charset,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            input->default_collation,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_optional_name(
            input->comment,
            MYLITE_CATALOG_TABLE_COMMENT_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_optional_name(
            input->row_format_option,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    }
    if (rc == MYLITE_OK && input->row_format_option != NULL &&
        input->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "DYNAMIC") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "COMPACT") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "REDUNDANT") &&
        !text_equals_ascii_case_insensitive(input->row_format_option, "COMPRESSED")) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && input->key_block_size != 0 && input->key_block_size != 1 &&
        input->key_block_size != 2 && input->key_block_size != 4 &&
        input->key_block_size != catalog_table_key_block_size_eight &&
        input->key_block_size != catalog_table_key_block_size_sixteen) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && (input->pack_keys < -1 || input->pack_keys > 1 || input->checksum < 0 ||
                            input->checksum > 1 || input->stats_persistent < -1 ||
                            input->stats_persistent > 1 || input->stats_auto_recalc < -1 ||
                            input->stats_auto_recalc > 1 || input->stats_sample_pages < 0 ||
                            input->stats_sample_pages > catalog_table_stats_sample_pages_max)) {
        rc = MYLITE_ERROR;
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc = mylite_catalog_validate_positive_id(schema_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_table == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_table_by_id(database->sqlite, table_id, out_table);
}

int mylite_catalog_read_view_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_view == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_view_by_table_id(database->sqlite, table_id, out_view);
}

int mylite_catalog_update_table_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name
) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_logical_object_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "UPDATE _mylite_catalog_tables "
        "SET name = ?1, descriptor_version = descriptor_version + 1, "
        "updated_catalog_generation = ?2 "
        "WHERE table_id = ?3",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 1, name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_u64(statement, 2, generation.next_generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 3, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    return MYLITE_OK;
}

int mylite_catalog_delete_table(struct mylite_db *database, int64_t table_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = delete_foreign_keys_for_related_table(database->sqlite, table_id);
    if (rc == MYLITE_OK) {
        rc = delete_check_constraints_for_table(database->sqlite, table_id);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_views WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_index_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_indexes WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_columns WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_prepare_statement(
            database->sqlite,
            "DELETE FROM _mylite_catalog_tables WHERE table_id = ?1",
            &statement
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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
    struct mylite_catalog_generation_change generation = {0};
    struct mylite_catalog_table_descriptor table = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    if (out_column != NULL) {
        *out_column = (struct mylite_catalog_column_descriptor){0};
    }
    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_ordinal(ordinal_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_catalog_column_values(&values, false);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = read_table_by_id(database->sqlite, table_id, &table);
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_step_done(statement);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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
    int rc = mylite_catalog_validate_ready_database(database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_catalog_validate_positive_id(table_id);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return read_column_by_name(database->sqlite, table_id, name, out_column);
}

int mylite_catalog_delete_column(struct mylite_db *database, int64_t column_id) {
    struct mylite_catalog_generation_change generation = {0};
    sqlite3_stmt *statement = NULL;
    int rc = MYLITE_OK;

    rc = mylite_catalog_validate_ready_database(database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_positive_id(column_id);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_begin_generation_change(database, &generation);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_prepare_statement(
        database->sqlite,
        "DELETE FROM _mylite_catalog_columns WHERE column_id = ?1",
        &statement
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, column_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_step_done(statement);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_require_changed_row(database->sqlite);
    }
    rc = mylite_catalog_finalize_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_finish_generation_change(database, &generation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_abandon_generation_change(database->sqlite);
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
        rc = mylite_catalog_bind_u64(statement, catalog_column_insert_generation_bind, generation);
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
        rc = mylite_catalog_bind_u64(statement, catalog_column_replace_generation_bind, generation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_column_replace_table_id_bind, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, catalog_column_replace_column_id_bind, column_id);
    }

    return rc;
}

static int bind_catalog_column_insert_core_values(
    sqlite3_stmt *statement,
    int64_t table_id,
    int64_t ordinal_position,
    const struct catalog_column_values *values
) {
    int rc = mylite_catalog_bind_i64(statement, catalog_column_insert_table_id_bind, table_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_ordinal_position_bind,
            ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, catalog_column_insert_name_bind, values->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_insert_logical_type_bind,
            values->logical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_insert_physical_type_bind,
            values->physical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_is_nullable_bind,
            mylite_catalog_bool_value(values->is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_is_visible_bind,
            mylite_catalog_bool_value(values->is_visible)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_insert_is_auto_increment_bind,
            mylite_catalog_bool_value(values->is_auto_increment)
        );
    }

    return rc;
}

static int bind_catalog_column_replace_core_values(
    sqlite3_stmt *statement,
    const struct catalog_column_values *values
) {
    int rc = mylite_catalog_bind_text(statement, catalog_column_replace_name_bind, values->name);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_replace_logical_type_bind,
            values->logical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            catalog_column_replace_physical_type_bind,
            values->physical_type
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_replace_is_nullable_bind,
            mylite_catalog_bool_value(values->is_nullable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_replace_is_visible_bind,
            mylite_catalog_bool_value(values->is_visible)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            catalog_column_replace_is_auto_increment_bind,
            mylite_catalog_bool_value(values->is_auto_increment)
        );
    }

    return rc;
}

static int bind_catalog_column_default_values(
    sqlite3_stmt *statement,
    struct catalog_column_default_bind_indexes indexes,
    const struct catalog_column_values *values
) {
    int rc =
        mylite_catalog_bind_i64(statement, indexes.default_kind, (int64_t)values->default_kind);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_nullable_i64(
            statement,
            indexes.default_integer,
            catalog_default_kind_stores_integer(values->default_kind),
            values->default_integer
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_nullable_text(
            statement,
            indexes.default_text,
            catalog_default_kind_stores_text(values->default_kind),
            values->default_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            indexes.on_update_current_timestamp,
            mylite_catalog_bool_value(values->on_update_current_timestamp)
        );
    }

    return rc;
}

static int bind_catalog_column_text_attributes(
    sqlite3_stmt *statement,
    struct catalog_column_text_attribute_bind_indexes indexes,
    const struct catalog_column_values *values
) {
    int rc = mylite_catalog_bind_text(
        statement,
        indexes.character_set_name,
        catalog_text_or_empty(values->character_set_name)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            indexes.collation_name,
            catalog_text_or_empty(values->collation_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            indexes.comment,
            catalog_text_or_empty(values->comment)
        );
    }

    return rc;
}

static int bind_catalog_column_generated_values(
    sqlite3_stmt *statement,
    struct catalog_column_generated_bind_indexes indexes,
    const struct catalog_column_values *values
) {
    int rc = mylite_catalog_bind_i64(
        statement,
        indexes.is_generated,
        mylite_catalog_bool_value(values->is_generated)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(
            statement,
            indexes.generated_kind,
            (int64_t)values->generated_kind
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
            statement,
            indexes.generation_expression,
            catalog_text_or_empty(values->generation_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(
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
    int rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_text(statement, 1, name);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_schema_by_id(
    sqlite3 *sqlite,
    int64_t schema_id,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT schema_id, name, default_charset, default_collation, "
        "descriptor_version, created_catalog_generation, "
        "updated_catalog_generation "
        "FROM _mylite_catalog_schemas WHERE schema_id = ?1",
        &statement
    );

    *out_schema = (struct mylite_catalog_schema_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
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
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, row_format_option, key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE schema_id = ?1 AND name = ?2",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    *out_found = false;
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_table_by_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_table_descriptor *out_table
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT table_id, schema_id, name, kind, physical_name, auto_increment_next, "
        "default_charset, default_collation, comment, row_format_option, key_block_size, "
        "pack_keys, checksum, stats_persistent, stats_auto_recalc, stats_sample_pages, "
        "fulltext_doc_id_initialized, "
        "created_time_utc_epoch, updated_time_utc_epoch, descriptor_version, "
        "created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_tables WHERE table_id = ?1",
        &statement
    );

    *out_table = (struct mylite_catalog_table_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_view_by_table_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_view_descriptor *out_view
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT table_id, view_definition, show_create_sql, check_option, is_updatable, "
        "definer, security_type, character_set_client, collation_connection, "
        "source_schema_id, source_table_id, source_schema_name, source_table_name, "
        "descriptor_version, created_catalog_generation, updated_catalog_generation "
        "FROM _mylite_catalog_views WHERE table_id = ?1",
        &statement
    );

    *out_view = (struct mylite_catalog_view_descriptor){0};
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = materialize_view(statement, out_view);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_column_by_name(
    sqlite3 *sqlite,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_bind_text(statement, 2, name);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int try_read_primary_index_by_table_id(
    sqlite3 *sqlite,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
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
        rc = mylite_catalog_bind_i64(statement, 1, table_id);
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

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_next_table_id(sqlite3 *sqlite, int64_t *out_table_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(table_id), 0) + 1 FROM _mylite_catalog_tables",
        &statement
    );

    *out_table_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(
                statement,
                catalog_next_table_id_column,
                out_table_id
            );
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_table_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_next_index_id(sqlite3 *sqlite, int64_t *out_index_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(index_id), 0) + 1 FROM _mylite_catalog_indexes",
        &statement
    );

    *out_index_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_index_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_index_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_next_foreign_key_id(sqlite3 *sqlite, int64_t *out_foreign_key_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(foreign_key_id), 0) + 1 FROM _mylite_catalog_foreign_keys",
        &statement
    );

    *out_foreign_key_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_foreign_key_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_foreign_key_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int read_next_check_constraint_id(sqlite3 *sqlite, int64_t *out_check_constraint_id) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_catalog_prepare_statement(
        sqlite,
        "SELECT COALESCE(MAX(check_constraint_id), 0) + 1 "
        "FROM _mylite_catalog_check_constraints",
        &statement
    );

    *out_check_constraint_id = 0;
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = mylite_catalog_checked_column_i64(statement, 0, out_check_constraint_id);
        } else {
            rc =
                sqlite_rc == SQLITE_DONE ? MYLITE_ERROR : mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(*out_check_constraint_id);
    }

    return mylite_catalog_finalize_statement(statement, rc);
}

static int materialize_schema(
    sqlite3_stmt *statement,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_schema_select_schema_id_column,
        &out_schema->schema_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_schema_select_name_column,
            out_schema->name,
            sizeof(out_schema->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_schema_select_default_charset_column,
            out_schema->default_charset,
            sizeof(out_schema->default_charset)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_schema_select_default_collation_column,
            out_schema->default_collation,
            sizeof(out_schema->default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_schema_select_descriptor_version_column,
            &out_schema->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_schema_select_created_generation_column,
            &out_schema->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
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
    int rc = materialize_table_identity(statement, out_table);

    if (rc == MYLITE_OK) {
        rc = materialize_table_storage_statistics(statement, out_table);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_table_lifecycle(statement, out_table);
    }
    if (rc == MYLITE_OK) {
        rc = validate_materialized_table(out_table);
    }
    return rc;
}

static int materialize_table_identity(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int64_t kind = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_table_select_table_id_column,
        &out_table->table_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_schema_id_column,
            &out_table->schema_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_name_column,
            out_table->name,
            sizeof(out_table->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(statement, catalog_table_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_table_kind((enum mylite_catalog_table_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_table->kind = (enum mylite_catalog_table_kind)kind;
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_physical_name_column,
            out_table->physical_name,
            sizeof(out_table->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_auto_increment_next_column,
            &out_table->auto_increment_next
        );
    }
    if (rc == MYLITE_OK && out_table->auto_increment_next <= 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_default_charset_column,
            out_table->default_charset,
            sizeof(out_table->default_charset)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_default_collation_column,
            out_table->default_collation,
            sizeof(out_table->default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_table_select_comment_column,
            out_table->comment,
            sizeof(out_table->comment)
        );
    }
    return rc;
}

static int materialize_table_storage_statistics(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = mylite_catalog_checked_column_text(
        statement,
        catalog_table_select_row_format_column,
        out_table->row_format_option,
        sizeof(out_table->row_format_option)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_key_block_size_column,
            &out_table->key_block_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_pack_keys_column,
            &out_table->pack_keys
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_checksum_column,
            &out_table->checksum
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_stats_persistent_column,
            &out_table->stats_persistent
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_stats_auto_recalc_column,
            &out_table->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_stats_sample_pages_column,
            &out_table->stats_sample_pages
        );
    }
    return rc;
}

static int materialize_table_lifecycle(
    sqlite3_stmt *statement,
    struct mylite_catalog_table_descriptor *out_table
) {
    int64_t fulltext_doc_id_initialized = 0;
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_table_select_fulltext_doc_id_initialized_column,
        &fulltext_doc_id_initialized
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
            fulltext_doc_id_initialized,
            &out_table->fulltext_doc_id_initialized
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_created_time_column,
            &out_table->created_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK && out_table->created_time_utc_epoch < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_table_select_updated_time_column,
            &out_table->updated_time_utc_epoch
        );
    }
    if (rc == MYLITE_OK && out_table->updated_time_utc_epoch < 0) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_table_select_descriptor_version_column,
            &out_table->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_table_select_created_generation_column,
            &out_table->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_table_select_updated_generation_column,
            &out_table->updated_catalog_generation
        );
    }

    return rc;
}

static int validate_materialized_table(const struct mylite_catalog_table_descriptor *table) {
    if (table == NULL) {
        return MYLITE_MISUSE;
    }
    return validate_catalog_table_descriptor_input(&(const struct catalog_table_descriptor_input){
        .schema_id = table->schema_id,
        .name = table->name,
        .physical_name = table->physical_name,
        .kind = table->kind,
        .default_charset = table->default_charset,
        .default_collation = table->default_collation,
        .comment = table->comment,
        .row_format_option = table->row_format_option,
        .key_block_size = table->key_block_size,
        .pack_keys = table->pack_keys,
        .checksum = table->checksum,
        .stats_persistent = table->stats_persistent,
        .stats_auto_recalc = table->stats_auto_recalc,
        .stats_sample_pages = table->stats_sample_pages,
        .created_time_utc_epoch = table->created_time_utc_epoch,
        .updated_time_utc_epoch = table->updated_time_utc_epoch,
    });
}

static int materialize_view(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_view_select_table_id_column,
        &out_view->table_id
    );

    if (rc == MYLITE_OK) {
        rc = materialize_view_text_fields(statement, out_view);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_view_source(statement, out_view);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_view_generations(statement, out_view);
    }
    if (rc == MYLITE_OK) {
        rc = validate_materialized_view(out_view);
    }
    return rc;
}

static int materialize_view_text_fields(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_text(
        statement,
        catalog_view_select_view_definition_column,
        out_view->view_definition,
        sizeof(out_view->view_definition)
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_show_create_sql_column,
            out_view->show_create_sql,
            sizeof(out_view->show_create_sql)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_check_option_column,
            out_view->check_option,
            sizeof(out_view->check_option)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_is_updatable_column,
            out_view->is_updatable,
            sizeof(out_view->is_updatable)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_definer_column,
            out_view->definer,
            sizeof(out_view->definer)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_security_type_column,
            out_view->security_type,
            sizeof(out_view->security_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_character_set_client_column,
            out_view->character_set_client,
            sizeof(out_view->character_set_client)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_collation_connection_column,
            out_view->collation_connection,
            sizeof(out_view->collation_connection)
        );
    }

    return rc;
}

static int materialize_view_source(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_view_select_source_schema_id_column,
        &out_view->source_schema_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_view_select_source_table_id_column,
            &out_view->source_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_source_schema_name_column,
            out_view->source_schema_name,
            sizeof(out_view->source_schema_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_view_select_source_table_name_column,
            out_view->source_table_name,
            sizeof(out_view->source_table_name)
        );
    }

    return rc;
}

static int materialize_view_generations(
    sqlite3_stmt *statement,
    struct mylite_catalog_view_descriptor *out_view
) {
    int rc = mylite_catalog_checked_column_u64(
        statement,
        catalog_view_select_descriptor_version_column,
        &out_view->descriptor_version
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_view_select_created_generation_column,
            &out_view->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_view_select_updated_generation_column,
            &out_view->updated_catalog_generation
        );
    }

    return rc;
}

static int validate_materialized_view(const struct mylite_catalog_view_descriptor *view) {
    int rc = mylite_catalog_validate_positive_id(view->table_id);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->view_definition,
            MYLITE_CATALOG_VIEW_DEFINITION_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->show_create_sql,
            MYLITE_CATALOG_VIEW_SHOW_CREATE_CAPACITY
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(view->check_option, sizeof(view->check_option));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(view->is_updatable, sizeof(view->is_updatable));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(view->definer, sizeof(view->definer));
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_catalog_validate_required_name(view->security_type, sizeof(view->security_type));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->character_set_client,
            sizeof(view->character_set_client)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->collation_connection,
            sizeof(view->collation_connection)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(view->source_schema_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_positive_id(view->source_table_id);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->source_schema_name,
            sizeof(view->source_schema_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_required_name(
            view->source_table_name,
            sizeof(view->source_table_name)
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_column_id_column,
        &out_column->column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_table_id_column,
            &out_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_ordinal_position_column,
            &out_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_name_column,
            out_column->name,
            sizeof(out_column->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_logical_type_column,
            out_column->logical_type,
            sizeof(out_column->logical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_physical_type_column,
            out_column->physical_type,
            sizeof(out_column->physical_type)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_character_set_name_column,
            out_column->character_set_name,
            sizeof(out_column->character_set_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_collation_name_column,
            out_column->collation_name,
            sizeof(out_column->collation_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_is_nullable_column,
        &nullable
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(nullable, &out_column->is_nullable);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_is_visible_column,
            &visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(visible, &out_column->is_visible);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_is_auto_increment_column,
            &auto_increment
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(auto_increment, &out_column->is_auto_increment);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_on_update_current_timestamp_column,
            &on_update_current_timestamp
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_default_kind_column,
        &default_kind
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_column_default_kind(
            (enum mylite_catalog_column_default_kind)default_kind
        );
    }
    if (rc == MYLITE_OK) {
        out_column->default_kind = (enum mylite_catalog_column_default_kind)default_kind;
        rc = mylite_catalog_checked_nullable_column_i64(
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
        rc = mylite_catalog_checked_nullable_column_text(
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
    int rc = mylite_catalog_checked_column_u64(
        statement,
        catalog_column_select_descriptor_version_column,
        &out_column->descriptor_version
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_column_select_created_generation_column,
            &out_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_column_select_is_generated_column,
        &is_generated
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(is_generated, &out_column->is_generated);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_column_select_generated_kind_column,
            &generated_kind
        );
    }
    if (rc == MYLITE_OK) {
        out_column->generated_kind = (enum mylite_catalog_generated_column_kind)generated_kind;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_column_select_generation_expression_column,
            out_column->generation_expression,
            sizeof(out_column->generation_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_index_select_index_id_column,
        &out_index->index_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_table_id_column,
            &out_index->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_name_column,
            out_index->name,
            sizeof(out_index->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(statement, catalog_index_select_kind_column, &kind);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_index_kind((enum mylite_catalog_index_kind)kind);
    }
    if (rc == MYLITE_OK) {
        out_index->kind = (enum mylite_catalog_index_kind)kind;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_is_unique_column,
            &is_unique
        );
    }
    if (rc == MYLITE_OK && is_unique != 0 && is_unique != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_unique = is_unique != 0;
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_select_is_visible_column,
            &is_visible
        );
    }
    if (rc == MYLITE_OK && is_visible != 0 && is_visible != 1) {
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_index->is_visible = is_visible != 0;
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_physical_name_column,
            out_index->physical_name,
            sizeof(out_index->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_index_select_comment_column,
            out_index->comment,
            sizeof(out_index->comment)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
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
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_select_descriptor_version_column,
            &out_index->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_select_created_generation_column,
            &out_index->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_index_column_select_index_column_id_column,
        &out_index_column->index_column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_index_id_column,
            &out_index_column->index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_table_id_column,
            &out_index_column->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_column_id_column,
            &out_index_column->column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
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
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_index_column_select_prefix_length_column,
            &out_index_column->prefix_length
        );
    }
    if (rc == MYLITE_OK) {
        int64_t sort_direction = 0;

        rc = mylite_catalog_checked_column_i64(
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
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_descriptor_version_column,
            &out_index_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_index_column_select_created_generation_column,
            &out_index_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_foreign_key_select_foreign_key_id_column,
        &out_foreign_key->foreign_key_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_child_table_id_column,
            &out_foreign_key->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_table_id_column,
            &out_foreign_key->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_name_column,
            out_foreign_key->name,
            sizeof(out_foreign_key->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_parent_index_id_column,
            &out_foreign_key->parent_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_select_child_index_id_column,
            &out_foreign_key->child_index_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_update_rule_column,
            out_foreign_key->update_rule,
            sizeof(out_foreign_key->update_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_delete_rule_column,
            out_foreign_key->delete_rule,
            sizeof(out_foreign_key->delete_rule)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_foreign_key_select_match_option_column,
            out_foreign_key->match_option,
            sizeof(out_foreign_key->match_option)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_descriptor_version_column,
            &out_foreign_key->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_select_created_generation_column,
            &out_foreign_key->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_foreign_key_column_select_foreign_key_column_id_column,
        &out_foreign_key_column->foreign_key_column_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_foreign_key_id_column,
            &out_foreign_key_column->foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_table_id_column,
            &out_foreign_key_column->child_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_table_id_column,
            &out_foreign_key_column->parent_table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_child_column_id_column,
            &out_foreign_key_column->child_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_parent_column_id_column,
            &out_foreign_key_column->parent_column_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_ordinal_position_column,
            &out_foreign_key_column->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_foreign_key_column_select_position_in_unique_constraint_column,
            &out_foreign_key_column->position_in_unique_constraint
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_descriptor_version_column,
            &out_foreign_key_column->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_foreign_key_column_select_created_generation_column,
            &out_foreign_key_column->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
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
    int rc = mylite_catalog_checked_column_i64(
        statement,
        catalog_check_constraint_select_check_constraint_id_column,
        &out_check_constraint->check_constraint_id
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_table_id_column,
            &out_check_constraint->table_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_name_column,
            out_check_constraint->name,
            sizeof(out_check_constraint->name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_physical_name_column,
            out_check_constraint->physical_name,
            sizeof(out_check_constraint->physical_name)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_check_clause_column,
            out_check_constraint->check_clause,
            sizeof(out_check_constraint->check_clause)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_text(
            statement,
            catalog_check_constraint_select_sqlite_expression_column,
            out_check_constraint->sqlite_expression,
            sizeof(out_check_constraint->sqlite_expression)
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_is_enforced_column,
            &is_enforced
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(is_enforced, &out_check_constraint->is_enforced);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_name_is_generated_column,
            &name_is_generated
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_validate_bool_i64(
            name_is_generated,
            &out_check_constraint->name_is_generated
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_generated_ordinal_column,
            &out_check_constraint->generated_ordinal
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_i64(
            statement,
            catalog_check_constraint_select_ordinal_position_column,
            &out_check_constraint->ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_descriptor_version_column,
            &out_check_constraint->descriptor_version
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_created_generation_column,
            &out_check_constraint->created_catalog_generation
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_checked_column_u64(
            statement,
            catalog_check_constraint_select_updated_generation_column,
            &out_check_constraint->updated_catalog_generation
        );
    }

    return rc;
}

static int validate_catalog_column_values(
    const struct catalog_column_values *values,
    bool use_logical_object_name
) {
    int rc = MYLITE_OK;

    if (use_logical_object_name) {
        rc = mylite_catalog_validate_logical_object_name(
            values->name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY
        );
    } else {
        rc =
            mylite_catalog_validate_required_name(values->name, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        values->logical_type,
        MYLITE_CATALOG_TYPE_NAME_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_required_name(
        values->physical_type,
        MYLITE_CATALOG_TYPE_NAME_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(
        values->character_set_name,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(
        values->collation_name,
        MYLITE_CATALOG_IDENTIFIER_CAPACITY
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_catalog_validate_optional_name(
        values->comment,
        MYLITE_CATALOG_COLUMN_COMMENT_CAPACITY
    );
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
    rc = mylite_catalog_validate_column_default_kind(values->default_kind);
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

static int text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return 0;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return 0;
        }
        ++index;
    }
    return left[index] == '\0' && right[index] == '\0';
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }
    return (char)byte;
}
