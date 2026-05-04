#ifndef MYLITE_RUNTIME_MYLITE_RUNTIME_H
#define MYLITE_RUNTIME_MYLITE_RUNTIME_H

#include <mylite/mylite.h>

#include "mylite_dml_types.h"
#include "mylite_expression.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata_types.h"
#include "mylite_schema_types.h"
#include "mylite_select_types.h"
#include "mylite_table_ddl_types.h"
#include "mylite_transaction_types.h"
#include "sql/mylite_ast.h"
#include "sqlite3.h"
#include "types/mylite_column_type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

enum mylite_stmt_kind {
    MYLITE_STMT_SQLITE = 0,
    MYLITE_STMT_CREATE_SCHEMA = 1,
    MYLITE_STMT_ALTER_SCHEMA = 2,
    MYLITE_STMT_DROP_SCHEMA = 3,
    MYLITE_STMT_USE_SCHEMA = 4,
    MYLITE_STMT_SET_NAMES = 5,
    MYLITE_STMT_SET_CHARACTER_SET = 6,
    MYLITE_STMT_CREATE_TABLE = 7,
    MYLITE_STMT_DROP_TABLE = 8,
    MYLITE_STMT_INSERT_VALUES = 9,
    MYLITE_STMT_INSERT_SET = 10,
    MYLITE_STMT_REPLACE_VALUES = 11,
    MYLITE_STMT_REPLACE_SET = 12,
    MYLITE_STMT_SCALAR_SELECT = 13,
    MYLITE_STMT_TABLE_SELECT = 14,
    MYLITE_STMT_UNION_QUERY = 15,
    MYLITE_STMT_UPDATE = 16,
    MYLITE_STMT_DELETE = 17,
    MYLITE_STMT_START_TRANSACTION = 18,
    MYLITE_STMT_BEGIN_TRANSACTION = 19,
    MYLITE_STMT_COMMIT = 20,
    MYLITE_STMT_ROLLBACK = 21,
    MYLITE_STMT_SAVEPOINT = 22,
    MYLITE_STMT_ROLLBACK_TO_SAVEPOINT = 23,
    MYLITE_STMT_RELEASE_SAVEPOINT = 24,
    MYLITE_STMT_CREATE_INDEX = 25,
    MYLITE_STMT_DROP_INDEX = 26,
    MYLITE_STMT_ALTER_TABLE = 27,
    MYLITE_STMT_RENAME_TABLE = 28,
    MYLITE_STMT_TRUNCATE_TABLE = 29,
};

enum mylite_information_schema_table {
    MYLITE_INFORMATION_SCHEMA_NONE = 0,
    MYLITE_INFORMATION_SCHEMA_SCHEMATA = 1,
    MYLITE_INFORMATION_SCHEMA_TABLES = 2,
    MYLITE_INFORMATION_SCHEMA_COLUMNS = 3,
    MYLITE_INFORMATION_SCHEMA_STATISTICS = 4,
    MYLITE_INFORMATION_SCHEMA_ENGINES = 5,
    MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS = 6,
    MYLITE_INFORMATION_SCHEMA_COLLATIONS = 7,
    MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY = 8,
    MYLITE_INFORMATION_SCHEMA_KEYWORDS = 9,
    MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS = 10,
    MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE = 11,
    MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS = 12,
    MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS = 13,
};

struct mylite_catalog_text_match {
    const char *text;
    const char *word;
};

struct mylite_catalog_column_descriptor_source {
    sqlite3_stmt *select;
    const char *extra;
    const char *is_nullable;
    const char *data_type;
    const char *collation_name;
    const char *column_type;
    const char *column_key;
    int column_default_index;
    int character_octet_length_index;
    int numeric_precision_index;
    int numeric_scale_index;
    int datetime_precision_index;
    bool nullable;
    bool is_unsigned;
    bool is_zerofill;
    bool auto_increment;
};

struct mylite_charset_collation_info {
    const char *character_set;
    const char *collation;
    int coercibility;
};

struct mylite_strcmp_compare_options {
    bool ignore_trailing_spaces;
    bool case_sensitive;
};

struct mylite_connection_charset_request {
    const char *character_set_name;
    const char *collation_name;
};

struct mylite_show_variables_query {
    enum mylite_sql_ast_show_variables_scope scope;
    const char *like_pattern;
};

struct mylite_show_status_query {
    enum mylite_sql_ast_show_status_scope scope;
    const char *like_pattern;
};

struct mylite_storage_engine_row {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

struct mylite_storage_engine_columns {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

struct mylite_show_engines_metadata_column {
    const char *name;
    uint64_t length;
    bool nullable;
};

struct mylite_show_character_set_query {
    const char *like_pattern;
};

struct mylite_show_collation_query {
    const char *like_pattern;
};

struct mylite_show_tables_query {
    const char *schema_name;
    const char *column_name;
    const char *glob_pattern;
    bool full;
};

struct mylite_show_table_status_query {
    const char *schema_name;
    const char *glob_pattern;
};

struct mylite_show_columns_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_columns_source_nodes {
    const struct mylite_sql_ast_node *table_name;
    const struct mylite_sql_ast_node *explicit_schema;
};

struct mylite_show_columns_query {
    const char *schema_name;
    const char *table_name;
    const char *like_pattern;
    bool full;
};

struct mylite_show_index_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_index_source_nodes {
    const struct mylite_sql_ast_node *table_name;
    const struct mylite_sql_ast_node *explicit_schema;
};

struct mylite_show_index_query {
    const char *schema_name;
    const char *table_name;
};

struct mylite_show_create_table_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_create_schema_info {
    char *name;
    char *character_set;
    char *collation;
    char *encryption;
};

struct mylite_show_diagnostics_query {
    enum mylite_sql_ast_show_diagnostics_kind kind;
    uint64_t offset;
    uint64_t row_count;
    bool has_limit;
};

struct mylite_show_create_table_info {
    char *engine;
    bool has_auto_increment;
    sqlite3_int64 auto_increment;
    char *table_collation;
    char *table_comment;
};

struct mylite_show_create_column_collation {
    const char *character_set_name;
    const char *column_collation;
    const char *table_collation;
};

struct mylite_db {
    sqlite3 *sqlite;
    char *error_message;
    struct mylite_expression_warnings warnings;
    char *selected_schema;
    uint64_t connection_id;
    uint64_t last_insert_id;
    int64_t previous_row_count;
    enum mylite_transaction_access_mode transaction_access_mode;
    bool transaction_active;
    bool transaction_consistent_snapshot;
    bool transaction_released;
    time_t status_started_at;
    struct mylite_savepoint_state savepoints;
    struct mylite_pending_auto_increment *pending_auto_increments;
    size_t pending_auto_increment_count;
    const char *character_set_client;
    const char *character_set_connection;
    const char *character_set_results;
    const char *collation_connection;
};

struct mylite_statement_timestamp {
    time_t seconds;
    long microseconds;
};

struct mylite_stmt {
    mylite_db *database;
    enum mylite_stmt_kind kind;
    sqlite3_stmt *sqlite_stmt;
    char *schema_name;
    bool if_exists;
    bool if_not_exists;
    bool executed;
    bool previous_row_count_recorded;
    bool preserve_prepare_warnings;
    bool has_statement_timestamp;
    struct mylite_statement_timestamp statement_timestamp;
    struct mylite_schema_options options;
    struct mylite_create_table_plan create_table;
    struct mylite_drop_table_plan drop_table;
    struct mylite_rename_table_plan rename_table;
    struct mylite_truncate_table_plan truncate_table;
    struct mylite_alter_table_plan alter_table;
    struct mylite_index_ddl_plan index_ddl;
    struct mylite_insert_values_plan insert_values;
    struct mylite_insert_set_plan insert_set;
    struct mylite_insert_duplicate_update_plan insert_update;
    struct mylite_update_plan update;
    struct mylite_delete_plan delete_plan;
    struct mylite_transaction_plan transaction;
    struct mylite_savepoint_plan savepoint;
    struct mylite_select_plan select_plan;
    struct mylite_union_plan union_plan;
    struct mylite_result_metadata result_metadata;
    struct mylite_scalar_result scalar_result;
    struct mylite_table_select_result select_result;
    struct mylite_sql_ast select_predicate_ast;
    struct mylite_sql_ast scalar_select_ast;
    struct mylite_sql_ast update_ast;
    struct mylite_sql_ast delete_ast;
    const struct mylite_sql_ast_node *select_predicate;
    char *select_sql_text;
    char *scalar_select_sql_text;
    char *update_sql_text;
    char *delete_sql_text;
    struct mylite_cached_expression_value *select_constant_values;
    size_t select_constant_value_count;
    bool select_constant_predicate_evaluated;
    bool select_constant_predicate_matches;
    char *character_set_name;
    char *collation_name;
    int64_t affected_rows;
    uint64_t matched_rows;
    bool use_default_connection_charset;
};

#endif
