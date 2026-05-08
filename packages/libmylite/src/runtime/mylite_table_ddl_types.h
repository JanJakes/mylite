#ifndef MYLITE_RUNTIME_MYLITE_TABLE_DDL_TYPES_H
#define MYLITE_RUNTIME_MYLITE_TABLE_DDL_TYPES_H

#include "sql/mylite_ast.h"
#include "types/mylite_column_type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_create_table_options {
    char *engine;
    char *character_set;
    char *collation;
    char *comment;
    uint64_t auto_increment;
    bool has_auto_increment;
};

struct mylite_create_table_column_type {
    enum mylite_sql_ast_column_type ast_type;
    struct mylite_column_type_attributes attributes;
    char *character_set;
    char *collation;
};

struct mylite_create_table_column {
    char *name;
    struct mylite_create_table_column_type type;
    char *default_text;
    char *comment;
    char *generation_expression;
    bool nullable;
    bool auto_increment;
    bool primary_key;
    bool unique_key;
    bool visible;
    bool has_generated_default;
    bool has_default;
    bool has_on_update_current_timestamp;
    unsigned int on_update_current_timestamp_fsp;
    uint64_t srs_id;
    bool has_srs_id;
    enum mylite_sql_ast_generated_column_storage generated_column_storage;
};

struct mylite_create_table_key_part {
    char *column_name;
    uint64_t prefix_length;
    bool has_prefix_length;
    enum mylite_sql_ast_key_part_order order;
};

struct mylite_create_table_index {
    char *name;
    char *comment;
    char *parser_name;
    struct mylite_create_table_key_part *parts;
    size_t part_count;
    enum mylite_sql_ast_index_algorithm algorithm;
    bool is_primary;
    bool is_unique;
    bool is_fulltext;
    bool is_spatial;
    bool is_visible;
    bool explicit_name;
    bool has_engine_attribute;
    bool has_with_parser;
    bool display_index_type;
};

struct mylite_create_table_check {
    char *name;
    char *clause;
    char *expression_sql;
    struct mylite_sql_ast expression_ast;
    const struct mylite_sql_ast_node *expression;
    bool enforced;
    bool generated_name;
};

struct mylite_create_table_foreign_key {
    char *constraint_name;
    char *supporting_index_name;
    char **column_names;
    size_t column_count;
    char *referenced_schema_name;
    char *referenced_table_name;
    char **referenced_column_names;
    size_t referenced_column_count;
    char *unique_constraint_name;
    bool generated_constraint_name;
    enum mylite_sql_ast_reference_match match;
    enum mylite_sql_ast_reference_action on_update;
    enum mylite_sql_ast_reference_action on_delete;
};

struct mylite_create_table_plan {
    char *schema_name;
    char *table_name;
    char *source_schema_name;
    char *source_table_name;
    char *select_sql_text;
    struct mylite_sql_ast select_ast;
    const struct mylite_sql_ast_node *select_statement;
    struct mylite_create_table_options options;
    struct mylite_create_table_column *columns;
    size_t column_count;
    struct mylite_create_table_index *indexes;
    size_t index_count;
    struct mylite_create_table_check *checks;
    size_t check_count;
    struct mylite_create_table_foreign_key *foreign_keys;
    size_t foreign_key_count;
    bool like;
    bool select;
    bool temporary;
    int64_t selected_row_count;
};

struct mylite_create_table_column_index_status {
    bool indexed;
    bool unique;
    bool primary;
};

struct mylite_drop_table_target {
    char *schema_name;
    char *table_name;
    bool exists;
    bool temporary;
};

struct mylite_drop_table_plan {
    struct mylite_drop_table_target *targets;
    size_t target_count;
    bool temporary;
    bool restrict_mode;
    bool cascade_mode;
};

struct mylite_lock_table_target {
    char *schema_name;
    char *table_name;
};

struct mylite_lock_tables_plan {
    struct mylite_lock_table_target *targets;
    size_t target_count;
};

struct mylite_rename_table_target {
    char *source_schema_name;
    char *source_table_name;
    char *target_schema_name;
    char *target_table_name;
};

struct mylite_rename_table_plan {
    struct mylite_rename_table_target *targets;
    size_t target_count;
};

struct mylite_truncate_table_plan {
    char *schema_name;
    char *table_name;
};

enum mylite_alter_table_action_kind {
    MYLITE_ALTER_TABLE_ACTION_ADD_COLUMN = 0,
    MYLITE_ALTER_TABLE_ACTION_DROP_COLUMN = 1,
    MYLITE_ALTER_TABLE_ACTION_RENAME_COLUMN = 2,
    MYLITE_ALTER_TABLE_ACTION_CHANGE_COLUMN = 3,
    MYLITE_ALTER_TABLE_ACTION_MODIFY_COLUMN = 4,
    MYLITE_ALTER_TABLE_ACTION_ADD_PRIMARY_KEY = 5,
    MYLITE_ALTER_TABLE_ACTION_DROP_PRIMARY_KEY = 6,
    MYLITE_ALTER_TABLE_ACTION_ADD_UNIQUE_INDEX = 7,
    MYLITE_ALTER_TABLE_ACTION_ADD_SECONDARY_INDEX = 8,
    MYLITE_ALTER_TABLE_ACTION_ADD_FULLTEXT_INDEX = 9,
    MYLITE_ALTER_TABLE_ACTION_ADD_SPATIAL_INDEX = 10,
    MYLITE_ALTER_TABLE_ACTION_DROP_INDEX = 11,
    MYLITE_ALTER_TABLE_ACTION_RENAME_INDEX = 12,
    MYLITE_ALTER_TABLE_ACTION_ALTER_INDEX_VISIBILITY = 13,
    MYLITE_ALTER_TABLE_ACTION_ADD_CHECK = 14,
    MYLITE_ALTER_TABLE_ACTION_DROP_CHECK = 15,
    MYLITE_ALTER_TABLE_ACTION_ALTER_CHECK = 16,
    MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_CHECK = 17,
    MYLITE_ALTER_TABLE_ACTION_ADD_FOREIGN_KEY = 18,
    MYLITE_ALTER_TABLE_ACTION_DROP_FOREIGN_KEY = 19,
    MYLITE_ALTER_TABLE_ACTION_UNSUPPORTED_FOREIGN_KEY = 20,
    MYLITE_ALTER_TABLE_ACTION_RENAME_TABLE = 21,
    MYLITE_ALTER_TABLE_ACTION_ALTER_COLUMN_SET_DEFAULT = 22,
    MYLITE_ALTER_TABLE_ACTION_ALTER_COLUMN_DROP_DEFAULT = 23,
};

enum mylite_alter_table_column_position_kind {
    MYLITE_ALTER_TABLE_COLUMN_POSITION_NONE = 0,
    MYLITE_ALTER_TABLE_COLUMN_POSITION_FIRST = 1,
    MYLITE_ALTER_TABLE_COLUMN_POSITION_AFTER = 2,
};

struct mylite_alter_table_action {
    enum mylite_alter_table_action_kind kind;
    enum mylite_alter_table_column_position_kind position;
    char *old_name;
    char *new_name;
    char *new_schema_name;
    char *after_column;
    struct mylite_create_table_column column;
    struct mylite_create_table_index index;
    struct mylite_create_table_check check;
    struct mylite_create_table_foreign_key foreign_key;
    bool index_visible;
    bool generic_constraint;
};

struct mylite_alter_table_plan {
    char *schema_name;
    char *table_name;
    struct mylite_alter_table_action *actions;
    size_t action_count;
    uint64_t auto_increment;
    char *unsupported_algorithm;
    char *unsupported_lock;
    bool has_auto_increment;
};

struct mylite_index_ddl_plan {
    char *schema_name;
    char *table_name;
    char *index_name;
    struct mylite_create_table_index index;
    enum mylite_sql_ast_index_class index_class;
};

struct mylite_alter_table_column {
    char *name;
    char *source_name;
    char *column_default;
    char *is_nullable;
    char *data_type;
    char *character_set_name;
    char *collation_name;
    char *column_type;
    char *column_key;
    char *extra;
    char *column_comment;
    char *generation_expression;
    int64_t character_maximum_length;
    int64_t character_octet_length;
    int64_t numeric_precision;
    int64_t numeric_scale;
    int64_t datetime_precision;
    int64_t srs_id;
    bool has_character_maximum_length;
    bool has_character_octet_length;
    bool has_numeric_precision;
    bool has_numeric_scale;
    bool has_datetime_precision;
    bool has_srs_id;
    bool nullable;
    bool auto_increment;
    bool visible;
    bool has_default;
    bool added;
};

struct mylite_alter_table_index_part {
    char *column_name;
    char *collation;
    char *nullable;
    int64_t sub_part;
    bool has_sub_part;
};

struct mylite_alter_table_index {
    char *index_schema;
    char *name;
    char *index_type;
    char *comment;
    char *index_comment;
    char *parser_name;
    char *is_visible;
    struct mylite_alter_table_index_part *parts;
    size_t part_count;
    int non_unique;
    bool changed;
    bool hash_fallback_warning;
    bool fulltext_doc_id_warning;
    bool display_index_type;
};

struct mylite_alter_table_model {
    char *schema_name;
    char *table_name;
    char *physical_name;
    char *table_collation;
    struct mylite_alter_table_column *columns;
    size_t column_count;
    struct mylite_alter_table_index *indexes;
    size_t index_count;
    uint64_t auto_increment;
    bool clear_auto_increment;
    bool had_auto_increment_column;
    bool report_copied_rows;
    bool set_auto_increment;
    bool temporary;
};

enum mylite_alter_table_column_catalog_field {
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_NAME = 0,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_DEFAULT = 1,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_IS_NULLABLE = 2,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATA_TYPE = 3,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_MAXIMUM_LENGTH = 4,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_OCTET_LENGTH = 5,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_PRECISION = 6,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_NUMERIC_SCALE = 7,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_DATETIME_PRECISION = 8,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_CHARACTER_SET_NAME = 9,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLLATION_NAME = 10,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLUMN_TYPE = 11,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_COLUMN_KEY = 12,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_EXTRA = 13,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_COMMENT = 14,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_GENERATION_EXPRESSION = 15,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_SRS_ID = 16,
    MYLITE_ALTER_TABLE_COLUMN_CATALOG_HAS_DEFAULT = 17,
};

enum mylite_alter_table_index_catalog_field {
    MYLITE_ALTER_TABLE_INDEX_CATALOG_NON_UNIQUE = 0,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_SCHEMA = 1,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_NAME = 2,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_SEQ_IN_INDEX = 3,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_COLUMN_NAME = 4,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_COLLATION = 5,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_SUB_PART = 6,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_NULLABLE = 7,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_TYPE = 8,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_COMMENT = 9,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_INDEX_COMMENT = 10,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_VISIBLE = 11,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_DISPLAY_TYPE = 12,
    MYLITE_ALTER_TABLE_INDEX_CATALOG_PARSER_NAME = 13,
};

#endif
