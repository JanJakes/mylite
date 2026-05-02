#include <mylite/mylite.h>

#include "mylite_charset.h"
#include "mylite_expression.h"
#include "mylite_internal.h"
#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"
#include "mylite_vfs.h"
#include "sqlite3.h"
#include "types/mylite_column_type.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    MYLITE_STMT_SCALAR_SELECT = 11,
    MYLITE_STMT_TABLE_SELECT = 12,
    MYLITE_STMT_UPDATE = 13,
};

enum mylite_information_schema_table {
    MYLITE_INFORMATION_SCHEMA_NONE = 0,
    MYLITE_INFORMATION_SCHEMA_SCHEMATA = 1,
    MYLITE_INFORMATION_SCHEMA_TABLES = 2,
    MYLITE_INFORMATION_SCHEMA_COLUMNS = 3,
    MYLITE_INFORMATION_SCHEMA_STATISTICS = 4,
};

enum mylite_mysql_condition_code {
    MYLITE_MYSQL_ER_NO_DB_ERROR = 1046,
    MYLITE_MYSQL_ER_BAD_NULL_ERROR = 1048,
    MYLITE_MYSQL_ER_BAD_DB_ERROR = 1049,
    MYLITE_MYSQL_ER_NON_UNIQ_ERROR = 1052,
    MYLITE_MYSQL_ER_BAD_FIELD_ERROR = 1054,
    MYLITE_MYSQL_ER_DUP_ENTRY = 1062,
    MYLITE_MYSQL_ER_NO_SUCH_TABLE = 1146,
    MYLITE_MYSQL_ER_WRONG_ARGUMENTS = 1210,
    MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE = 1292,
    MYLITE_MYSQL_ER_NO_DEFAULT_FOR_FIELD = 1364,
    MYLITE_MYSQL_ER_DIVISION_BY_ZERO = 1365,
    MYLITE_MYSQL_ER_TRUNCATED_WRONG_VALUE_FOR_FIELD = 1366,
};

struct mylite_schema_options {
    char *character_set;
    char *collation;
    char *encryption;
    bool has_read_only;
    int read_only;
    bool invalid_encryption;
    bool invalid_read_only;
};

struct mylite_schema_presence {
    bool exists;
    bool is_system;
};

struct mylite_schema_default {
    const char *character_set;
    const char *collation;
};

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
    bool nullable;
    bool auto_increment;
    bool primary_key;
    bool unique_key;
    bool visible;
    bool has_generated_default;
    bool has_on_update_current_timestamp;
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
    struct mylite_create_table_key_part *parts;
    size_t part_count;
    enum mylite_sql_ast_index_algorithm algorithm;
    bool is_primary;
    bool is_unique;
    bool is_visible;
    bool explicit_name;
};

struct mylite_create_table_plan {
    char *schema_name;
    char *table_name;
    struct mylite_create_table_options options;
    struct mylite_create_table_column *columns;
    size_t column_count;
    struct mylite_create_table_index *indexes;
    size_t index_count;
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
};

struct mylite_drop_table_plan {
    struct mylite_drop_table_target *targets;
    size_t target_count;
    bool temporary;
    bool restrict_mode;
    bool cascade_mode;
};

enum mylite_insert_value_kind {
    MYLITE_INSERT_VALUE_UNSUPPORTED = 0,
    MYLITE_INSERT_VALUE_DEFAULT = 1,
    MYLITE_INSERT_VALUE_NULL = 2,
    MYLITE_INSERT_VALUE_INTEGER = 3,
    MYLITE_INSERT_VALUE_REAL = 4,
    MYLITE_INSERT_VALUE_TEXT = 5,
    MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP = 6,
    MYLITE_INSERT_VALUE_COLUMN_REFERENCE = 7,
    MYLITE_INSERT_VALUE_UNARY_EXPRESSION = 8,
    MYLITE_INSERT_VALUE_BINARY_EXPRESSION = 9,
};

enum mylite_insert_bound_value_kind {
    MYLITE_INSERT_BOUND_NULL = 0,
    MYLITE_INSERT_BOUND_INTEGER = 1,
    MYLITE_INSERT_BOUND_REAL = 2,
    MYLITE_INSERT_BOUND_TEXT = 3,
};

struct mylite_insert_column_reference {
    char *schema_name;
    char *table_name;
    char *column_name;
};

struct mylite_insert_value {
    enum mylite_insert_value_kind kind;
    enum mylite_sql_ast_operator operator_kind;
    char *text;
    struct mylite_insert_column_reference column_reference;
    struct mylite_insert_value *left;
    struct mylite_insert_value *right;
};

struct mylite_insert_row {
    struct mylite_insert_value *values;
    size_t value_count;
};

struct mylite_insert_values_plan {
    char *schema_name;
    char *table_name;
    char **columns;
    size_t column_count;
    bool has_column_list;
    struct mylite_insert_row *rows;
    size_t row_count;
};

struct mylite_insert_set_assignment {
    struct mylite_insert_column_reference target;
    struct mylite_insert_value value;
};

struct mylite_insert_set_plan {
    struct mylite_insert_set_assignment *assignments;
    size_t assignment_count;
};

struct mylite_update_target {
    char *schema_name;
    char *table_name;
    char *alias;
};

struct mylite_update_column_reference {
    char *schema_name;
    char *table_name;
    char *column_name;
};

struct mylite_update_assignment {
    struct mylite_update_column_reference target;
    const struct mylite_sql_ast_node *value;
};

struct mylite_update_plan {
    struct mylite_update_target target;
    struct mylite_update_assignment *assignments;
    size_t assignment_count;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_by_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct mylite_insert_table_column {
    char *name;
    char *default_text;
    char *data_type;
    char *extra;
    bool nullable;
    bool auto_increment;
    bool generated_default;
};

struct mylite_insert_unique_index {
    char *name;
    size_t *column_indexes;
    size_t column_count;
    bool is_primary;
};

struct mylite_insert_unique_index_part_name {
    const char *index_name;
    const char *column_name;
};

struct mylite_insert_table {
    char *physical_name;
    struct mylite_insert_table_column *columns;
    size_t column_count;
    struct mylite_insert_unique_index *unique_indexes;
    size_t unique_index_count;
    size_t auto_increment_column_index;
    uint64_t next_auto_increment;
    bool has_auto_increment;
};

struct mylite_insert_bound_value {
    enum mylite_insert_bound_value_kind kind;
    int64_t integer_value;
    double real_value;
    char *text_value;
    bool generated_auto_increment;
};

struct mylite_insert_execution_state {
    uint64_t next_auto_increment;
    uint64_t reserved_auto_increment_end;
    uint64_t first_insert_id;
    bool generated_insert_id;
};

struct mylite_insert_set_row_state {
    bool *generate_auto_increment;
    bool *assigned_columns;
};

struct mylite_select_column {
    char *name;
    bool visible;
};

struct mylite_select_table {
    char *schema_name;
    char *table_name;
    char *alias;
    char *physical_name;
    struct mylite_select_column *columns;
    size_t column_count;
};

enum mylite_select_output_kind {
    MYLITE_SELECT_OUTPUT_COLUMN = 0,
    MYLITE_SELECT_OUTPUT_EXPRESSION = 1,
};

struct mylite_select_output_column {
    enum mylite_select_output_kind kind;
    size_t column_index;
    const struct mylite_sql_ast_node *expression;
    char *label;
    bool referenced_by_order;
};

enum mylite_select_order_key_kind {
    MYLITE_SELECT_ORDER_KEY_EXPRESSION = 0,
    MYLITE_SELECT_ORDER_KEY_OUTPUT = 1,
};

struct mylite_select_order_key {
    enum mylite_select_order_key_kind kind;
    enum mylite_sql_ast_key_part_order direction;
    size_t output_index;
    const struct mylite_sql_ast_node *expression;
};

struct mylite_select_limit {
    uint64_t offset;
    uint64_t row_count;
    bool has_limit;
};

struct mylite_select_limit_position {
    uint64_t matched_row;
    size_t kept_count;
};

struct mylite_select_plan {
    struct mylite_select_table table;
    struct mylite_select_output_column *outputs;
    size_t output_count;
    struct mylite_select_order_key *order_keys;
    size_t order_key_count;
    struct mylite_select_limit limit;
};

struct mylite_result_column_metadata {
    char *name;
    char *schema_name;
    char *table_name;
    char *origin_schema_name;
    char *origin_table_name;
    char *origin_column_name;
};

struct mylite_result_metadata {
    struct mylite_result_column_metadata *columns;
    size_t column_count;
};

struct mylite_scalar_result {
    struct mylite_expression_value *values;
    char **texts;
    struct mylite_expression_warnings warnings;
    size_t value_count;
    bool has_row;
};

struct mylite_cached_expression_value {
    const struct mylite_sql_ast_node *expression;
    struct mylite_expression_value value;
    bool evaluated;
    int status;
};

struct mylite_table_select_row {
    struct mylite_expression_value *values;
    struct mylite_expression_value *order_values;
    size_t value_count;
    size_t order_value_count;
};

struct mylite_table_select_result {
    struct mylite_table_select_row *rows;
    struct mylite_expression_value *current_values;
    char **current_texts;
    size_t row_count;
    size_t next_row;
    size_t current_value_count;
    bool materialized;
    bool has_current_row;
};

struct mylite_table_select_expression_context {
    mylite_stmt *stmt;
    const struct mylite_table_select_row *row;
    bool order_resolution;
};

struct mylite_update_bound_assignment {
    size_t column_index;
    const struct mylite_sql_ast_node *value;
};

struct mylite_update_order_plan {
    struct mylite_select_order_key *order_keys;
    size_t order_key_count;
};

struct mylite_update_row {
    sqlite3_int64 rowid;
    struct mylite_expression_value *values;
    struct mylite_expression_value *order_values;
    size_t value_count;
    size_t order_value_count;
};

struct mylite_update_rowset {
    struct mylite_update_row *rows;
    size_t row_count;
};

struct mylite_update_expression_context {
    const struct mylite_select_table *table;
    const struct mylite_update_row *row;
};

struct mylite_connection_charset_request {
    const char *character_set_name;
    const char *collation_name;
};

struct mylite_db {
    sqlite3 *sqlite;
    char *error_message;
    struct mylite_expression_warnings warnings;
    char *selected_schema;
    uint64_t last_insert_id;
    const char *character_set_client;
    const char *character_set_connection;
    const char *character_set_results;
    const char *collation_connection;
};

struct mylite_stmt {
    mylite_db *database;
    enum mylite_stmt_kind kind;
    sqlite3_stmt *sqlite_stmt;
    char *schema_name;
    bool if_exists;
    bool if_not_exists;
    bool executed;
    struct mylite_schema_options options;
    struct mylite_create_table_plan create_table;
    struct mylite_drop_table_plan drop_table;
    struct mylite_insert_values_plan insert_values;
    struct mylite_insert_set_plan insert_set;
    struct mylite_update_plan update;
    struct mylite_select_plan select_plan;
    struct mylite_result_metadata result_metadata;
    struct mylite_scalar_result scalar_result;
    struct mylite_table_select_result select_result;
    struct mylite_sql_ast select_predicate_ast;
    struct mylite_sql_ast update_ast;
    const struct mylite_sql_ast_node *select_predicate;
    char *select_sql_text;
    char *update_sql_text;
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

static const char schema_catalog_sql[] = "CREATE TABLE IF NOT EXISTS __mylite_schema_catalog("
                                         "name TEXT PRIMARY KEY COLLATE BINARY,"
                                         "default_character_set TEXT NOT NULL,"
                                         "default_collation TEXT NOT NULL,"
                                         "default_encryption TEXT NOT NULL,"
                                         "read_only INTEGER NOT NULL,"
                                         "is_system INTEGER NOT NULL)";
static const char table_catalog_sql[] = "CREATE TABLE IF NOT EXISTS __mylite_table_catalog("
                                        "table_catalog TEXT NOT NULL,"
                                        "table_schema TEXT NOT NULL,"
                                        "table_name TEXT NOT NULL,"
                                        "table_type TEXT NOT NULL,"
                                        "engine TEXT,"
                                        "version INTEGER,"
                                        "row_format TEXT,"
                                        "table_rows INTEGER,"
                                        "avg_row_length INTEGER,"
                                        "data_length INTEGER,"
                                        "max_data_length INTEGER,"
                                        "index_length INTEGER,"
                                        "data_free INTEGER,"
                                        "auto_increment INTEGER,"
                                        "create_time TEXT NOT NULL,"
                                        "update_time TEXT,"
                                        "check_time TEXT,"
                                        "table_collation TEXT,"
                                        "checksum INTEGER,"
                                        "create_options TEXT,"
                                        "table_comment TEXT,"
                                        "PRIMARY KEY(table_schema, table_name))";
static const char column_catalog_sql[] = "CREATE TABLE IF NOT EXISTS __mylite_column_catalog("
                                         "table_catalog TEXT NOT NULL,"
                                         "table_schema TEXT NOT NULL,"
                                         "table_name TEXT NOT NULL,"
                                         "column_name TEXT,"
                                         "ordinal_position INTEGER NOT NULL,"
                                         "column_default TEXT,"
                                         "is_nullable TEXT NOT NULL,"
                                         "data_type TEXT,"
                                         "character_maximum_length INTEGER,"
                                         "character_octet_length INTEGER,"
                                         "numeric_precision INTEGER,"
                                         "numeric_scale INTEGER,"
                                         "datetime_precision INTEGER,"
                                         "character_set_name TEXT,"
                                         "collation_name TEXT,"
                                         "column_type TEXT NOT NULL,"
                                         "column_key TEXT NOT NULL,"
                                         "extra TEXT,"
                                         "privileges TEXT,"
                                         "column_comment TEXT NOT NULL,"
                                         "generation_expression TEXT NOT NULL,"
                                         "srs_id INTEGER,"
                                         "PRIMARY KEY(table_schema, table_name, ordinal_position))";
static const char index_catalog_sql[] =
    "CREATE TABLE IF NOT EXISTS __mylite_index_catalog("
    "table_catalog TEXT NOT NULL,"
    "table_schema TEXT NOT NULL,"
    "table_name TEXT NOT NULL,"
    "non_unique INTEGER NOT NULL,"
    "index_schema TEXT NOT NULL,"
    "index_name TEXT,"
    "seq_in_index INTEGER NOT NULL,"
    "column_name TEXT,"
    "collation TEXT,"
    "cardinality INTEGER,"
    "sub_part INTEGER,"
    "packed TEXT,"
    "nullable TEXT NOT NULL,"
    "index_type TEXT NOT NULL,"
    "comment TEXT NOT NULL,"
    "index_comment TEXT NOT NULL,"
    "is_visible TEXT NOT NULL,"
    "expression TEXT,"
    "PRIMARY KEY(table_schema, table_name, index_name, seq_in_index))";
static const char show_schemas_sql[] =
    "SELECT name AS \"Database\" FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";
static const char information_schema_schemata_sql[] =
    "SELECT 'def' AS CATALOG_NAME,"
    "name AS SCHEMA_NAME,"
    "default_character_set AS DEFAULT_CHARACTER_SET_NAME,"
    "default_collation AS DEFAULT_COLLATION_NAME,"
    "NULL AS SQL_PATH,"
    "CASE WHEN upper(default_encryption) = 'Y' THEN 'YES' ELSE 'NO' END AS DEFAULT_ENCRYPTION "
    "FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";
static const char information_schema_tables_sql[] =
    "SELECT * FROM ("
    "SELECT 'def' AS TABLE_CATALOG,"
    "'information_schema' AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "'SYSTEM VIEW' AS TABLE_TYPE,"
    "NULL AS ENGINE,"
    "10 AS VERSION,"
    "NULL AS ROW_FORMAT,"
    "0 AS TABLE_ROWS,"
    "NULL AS AVG_ROW_LENGTH,"
    "NULL AS DATA_LENGTH,"
    "NULL AS MAX_DATA_LENGTH,"
    "NULL AS INDEX_LENGTH,"
    "NULL AS DATA_FREE,"
    "NULL AS AUTO_INCREMENT,"
    "'1970-01-01 00:00:00' AS CREATE_TIME,"
    "NULL AS UPDATE_TIME,"
    "NULL AS CHECK_TIME,"
    "NULL AS TABLE_COLLATION,"
    "NULL AS CHECKSUM,"
    "'' AS CREATE_OPTIONS,"
    "'' AS TABLE_COMMENT "
    "FROM ("
    "SELECT 'SCHEMATA' AS table_name "
    "UNION ALL SELECT 'TABLES' "
    "UNION ALL SELECT 'COLUMNS' "
    "UNION ALL SELECT 'STATISTICS') "
    "UNION ALL "
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "table_type AS TABLE_TYPE,"
    "engine AS ENGINE,"
    "version AS VERSION,"
    "row_format AS ROW_FORMAT,"
    "table_rows AS TABLE_ROWS,"
    "avg_row_length AS AVG_ROW_LENGTH,"
    "data_length AS DATA_LENGTH,"
    "max_data_length AS MAX_DATA_LENGTH,"
    "index_length AS INDEX_LENGTH,"
    "data_free AS DATA_FREE,"
    "auto_increment AS AUTO_INCREMENT,"
    "create_time AS CREATE_TIME,"
    "update_time AS UPDATE_TIME,"
    "check_time AS CHECK_TIME,"
    "table_collation AS TABLE_COLLATION,"
    "checksum AS CHECKSUM,"
    "create_options AS CREATE_OPTIONS,"
    "table_comment AS TABLE_COMMENT "
    "FROM __mylite_table_catalog) "
    "ORDER BY TABLE_SCHEMA COLLATE BINARY, TABLE_NAME COLLATE BINARY";
static const char information_schema_columns_sql[] =
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "column_name AS COLUMN_NAME,"
    "ordinal_position AS ORDINAL_POSITION,"
    "column_default AS COLUMN_DEFAULT,"
    "is_nullable AS IS_NULLABLE,"
    "data_type AS DATA_TYPE,"
    "character_maximum_length AS CHARACTER_MAXIMUM_LENGTH,"
    "character_octet_length AS CHARACTER_OCTET_LENGTH,"
    "numeric_precision AS NUMERIC_PRECISION,"
    "numeric_scale AS NUMERIC_SCALE,"
    "datetime_precision AS DATETIME_PRECISION,"
    "character_set_name AS CHARACTER_SET_NAME,"
    "collation_name AS COLLATION_NAME,"
    "column_type AS COLUMN_TYPE,"
    "column_key AS COLUMN_KEY,"
    "extra AS EXTRA,"
    "privileges AS PRIVILEGES,"
    "column_comment AS COLUMN_COMMENT,"
    "generation_expression AS GENERATION_EXPRESSION,"
    "srs_id AS SRS_ID "
    "FROM __mylite_column_catalog "
    "ORDER BY table_schema COLLATE BINARY, table_name COLLATE BINARY, ordinal_position";
static const char information_schema_statistics_sql[] =
    "SELECT table_catalog AS TABLE_CATALOG,"
    "table_schema AS TABLE_SCHEMA,"
    "table_name AS TABLE_NAME,"
    "non_unique AS NON_UNIQUE,"
    "index_schema AS INDEX_SCHEMA,"
    "index_name AS INDEX_NAME,"
    "seq_in_index AS SEQ_IN_INDEX,"
    "column_name AS COLUMN_NAME,"
    "collation AS COLLATION,"
    "cardinality AS CARDINALITY,"
    "sub_part AS SUB_PART,"
    "packed AS PACKED,"
    "nullable AS NULLABLE,"
    "index_type AS INDEX_TYPE,"
    "comment AS COMMENT,"
    "index_comment AS INDEX_COMMENT,"
    "is_visible AS IS_VISIBLE,"
    "expression AS EXPRESSION "
    "FROM __mylite_index_catalog "
    "ORDER BY table_schema COLLATE BINARY, table_name COLLATE BINARY, "
    "index_name COLLATE BINARY, seq_in_index";

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db);
static int initialize_schema_catalog(mylite_db *database);
static int seed_system_schema(mylite_db *database, const char *name, const char *character_set,
                              const char *collation);
static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt);
static int prepare_schema_lifecycle_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt);
static int prepare_connection_charset_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                mylite_stmt **out_stmt);
static int prepare_create_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
static int prepare_drop_table_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_insert_values_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt);
static int prepare_insert_set_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_update_statement(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt **out_stmt);
static int prepare_show_schemas_statement(mylite_db *database, mylite_stmt **out_stmt);
static int prepare_information_schema_select_statement(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement,
                                                       mylite_stmt **out_stmt);
static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt);
static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt);
static int attach_select_result_metadata(mylite_stmt *stmt, const struct mylite_select_plan *plan);
static int copy_select_result_column_metadata(mylite_db *database,
                                              struct mylite_result_column_metadata *metadata,
                                              const struct mylite_select_plan *plan,
                                              size_t output_index);
static int copy_result_metadata_text(mylite_db *database, char **out_text, const char *text);
static int copy_select_table_reference(const struct mylite_sql_ast_node *from_clause,
                                       struct mylite_select_table *table);
static int resolve_select_table_target(mylite_db *database, struct mylite_select_table *table);
static bool select_schema_name_is_system(const char *schema_name);
static int load_select_columns(mylite_db *database, struct mylite_select_table *table);
static int load_select_column_from_catalog_row(struct mylite_select_table *table,
                                               sqlite3_stmt *select);
static bool select_column_extra_is_visible(const char *extra);
static int build_select_outputs(mylite_db *database, const struct mylite_sql_ast_node *select_list,
                                bool allow_expression_outputs, struct mylite_select_plan *plan);
static int prepare_table_select_custom_statement(mylite_db *database,
                                                 const struct mylite_sql_ast_node *where_clause,
                                                 const char *sql, size_t sql_length,
                                                 struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt);
static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan);
static int bind_select_predicate_expression(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_select_plan *plan);
static int bind_select_limit_clause(const struct mylite_sql_ast_node *limit_clause,
                                    struct mylite_select_plan *plan);
static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan);
static int bind_select_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                                  struct mylite_select_plan *plan);
static int bind_select_order_expression(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_select_plan *plan);
static int validate_select_expression_outputs(mylite_db *database,
                                              const struct mylite_select_plan *plan);
static int append_select_item_outputs(mylite_db *database,
                                      const struct mylite_sql_ast_node *select_item,
                                      bool allow_expression_outputs,
                                      struct mylite_select_plan *plan);
static int append_select_wildcard_outputs(mylite_db *database,
                                          const struct mylite_sql_ast_node *wildcard,
                                          struct mylite_select_plan *plan);
static int append_select_column_output(mylite_db *database,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_sql_ast_node *alias,
                                       struct mylite_select_plan *plan);
static int append_select_expression_output(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_sql_ast_node *alias,
                                           struct mylite_select_plan *plan);
static int add_select_output_column(struct mylite_select_plan *plan,
                                    const struct mylite_select_output_column *output);
static int add_select_order_key(struct mylite_select_plan *plan,
                                const struct mylite_select_order_key *order_key);
static void mark_select_output_order_reference(struct mylite_select_plan *plan,
                                               size_t output_index);
static int resolve_select_wildcard(const struct mylite_select_table *table,
                                   const struct mylite_sql_ast_node *wildcard, bool *out_matches);
static int resolve_select_column_reference(const struct mylite_select_table *table,
                                           const struct mylite_sql_ast_node *expression,
                                           size_t *out_index);
static int resolve_select_order_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_order_key_kind *out_kind,
                                          size_t *out_index);
static size_t select_output_label_count(const struct mylite_select_plan *plan, const char *label,
                                        size_t *out_index);
static bool select_reference_qualifiers_match(const struct mylite_select_table *table, char **parts,
                                              size_t part_count);
static size_t select_column_index(const struct mylite_select_table *table, const char *column_name);
static bool parse_uint64_span(struct mylite_sql_source_span span, uint64_t *out_value);
static int copy_select_identifier_parts(const struct mylite_sql_ast_node *identifier, char **parts,
                                        size_t *part_count);
static char *copy_select_alias(const struct mylite_sql_ast_node *alias);
static char *copy_select_final_identifier_label(const struct mylite_sql_ast_node *identifier);
static char *copy_select_reference_name(const struct mylite_sql_ast_node *identifier);
static char *copy_select_wildcard_qualifier_name(const struct mylite_sql_ast_node *wildcard);
static int set_select_unknown_column_error(mylite_db *database, const char *column_name);
static int set_select_unknown_where_column_error(mylite_db *database, const char *column_name);
static int set_select_unknown_order_column_error(mylite_db *database, const char *column_name);
static int set_select_ambiguous_order_column_error(mylite_db *database, const char *column_name);
static int set_select_unknown_table_error(mylite_db *database, const char *table_name);
static int set_select_unsupported_projection_error(mylite_db *database);
static int set_select_unsupported_where_error(mylite_db *database);
static int set_select_unsupported_order_error(mylite_db *database);
static char *build_select_physical_sql(mylite_db *database, const struct mylite_select_plan *plan);
static char *build_select_scan_sql(mylite_db *database, const struct mylite_select_plan *plan);
static int clone_table_select_expressions(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *where_clause,
                                          const char *sql, size_t sql_length);
static int clone_table_select_expression_node(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              const char *source_sql, size_t sql_length,
                                              struct mylite_sql_ast_node **out_node);
static int clone_update_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length);
static int clone_update_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, size_t sql_length,
                                 const struct mylite_sql_ast_node **out_node);
static int clone_sql_ast_subtree(struct mylite_sql_ast *ast, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, const char *sql_copy, size_t sql_length,
                                 struct mylite_sql_ast_node **out_node);
static struct mylite_sql_source_span remap_source_span(struct mylite_sql_source_span span,
                                                       const char *source_sql, const char *sql_copy,
                                                       size_t sql_length);
static int prepare_sqlite_statement(mylite_db *database, const char *sqlite_sql,
                                    mylite_stmt **out_stmt);
static int prepare_custom_statement(mylite_db *database, enum mylite_stmt_kind kind,
                                    const struct mylite_sql_ast_node *statement,
                                    mylite_stmt **out_stmt);
static int execute_custom_statement(mylite_stmt *stmt);
static int execute_create_schema_statement(mylite_stmt *stmt);
static int execute_alter_schema_statement(mylite_stmt *stmt);
static int execute_drop_schema_statement(mylite_stmt *stmt);
static int execute_use_schema_statement(mylite_stmt *stmt);
static int execute_set_names_statement(mylite_stmt *stmt);
static int execute_set_character_set_statement(mylite_stmt *stmt);
static int execute_create_table_statement(mylite_stmt *stmt);
static int execute_drop_table_statement(mylite_stmt *stmt);
static int execute_insert_values_statement(mylite_stmt *stmt);
static int execute_insert_set_statement(mylite_stmt *stmt);
static int execute_update_statement(mylite_stmt *stmt);
static int copy_update_target_to_select_table(mylite_stmt *stmt, struct mylite_select_table *table);
static int bind_update_subset(mylite_stmt *stmt, const struct mylite_select_table *table,
                              struct mylite_update_bound_assignment **out_assignments);
static int reject_deferred_update_clauses(mylite_stmt *stmt);
static int bind_update_assignment_targets(mylite_stmt *stmt,
                                          const struct mylite_select_table *table,
                                          struct mylite_update_bound_assignment *assignments,
                                          size_t assignment_count);
static int bind_update_assignment_values(mylite_stmt *stmt, const struct mylite_select_table *table,
                                         struct mylite_update_bound_assignment *assignments,
                                         size_t assignment_count);
static int bind_update_assignment_expression(mylite_stmt *stmt,
                                             const struct mylite_select_table *table,
                                             const struct mylite_sql_ast_node *expression);
static int bind_update_where_clause(mylite_stmt *stmt, const struct mylite_select_table *table);
static int bind_update_predicate_expression(mylite_stmt *stmt,
                                            const struct mylite_select_table *table,
                                            const struct mylite_sql_ast_node *expression,
                                            const char *clause_context);
static int bind_update_order_by_clause(mylite_stmt *stmt, const struct mylite_select_table *table,
                                       struct mylite_update_order_plan *order_plan);
static int bind_update_order_expression(mylite_stmt *stmt, const struct mylite_select_table *table,
                                        const struct mylite_sql_ast_node *expression);
static int add_update_order_key(struct mylite_update_order_plan *plan,
                                const struct mylite_select_order_key *order_key);
static int materialize_update_rows(mylite_stmt *stmt, const struct mylite_select_table *table,
                                   const struct mylite_update_order_plan *order_plan,
                                   struct mylite_update_rowset *rowset);
static char *build_update_scan_sql(mylite_db *database, const struct mylite_select_table *table);
static int copy_update_sqlite_row(mylite_stmt *stmt, const struct mylite_select_table *table,
                                  sqlite3_stmt *scan, struct mylite_update_row *out_row);
static int copy_update_sqlite_column_value(sqlite3_stmt *scan, int column,
                                           struct mylite_expression_value *out_value);
static int evaluate_update_row_matches(mylite_stmt *stmt, const struct mylite_select_table *table,
                                       const struct mylite_update_row *row, bool *out_matches);
static int evaluate_update_order_values(mylite_stmt *stmt, const struct mylite_select_table *table,
                                        const struct mylite_update_order_plan *order_plan,
                                        struct mylite_update_row *row);
static int evaluate_update_order_key(mylite_stmt *stmt, const struct mylite_select_table *table,
                                     const struct mylite_update_row *row,
                                     const struct mylite_select_order_key *order_key,
                                     struct mylite_expression_value *out_value);
static int append_update_row(mylite_stmt *stmt, struct mylite_update_rowset *rowset,
                             struct mylite_update_row *row);
static int sort_update_rowset(struct mylite_update_rowset *rowset,
                              const struct mylite_update_order_plan *order_plan);
// NOLINTNEXTLINE(misc-no-recursion)
static int merge_sort_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                                  size_t first, size_t last,
                                  const struct mylite_update_order_plan *order_plan);
static void merge_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                              size_t first, size_t middle, size_t last,
                              const struct mylite_update_order_plan *order_plan);
static int compare_update_rows(const struct mylite_update_row *left,
                               const struct mylite_update_row *right,
                               const struct mylite_update_order_plan *order_plan);
static void apply_update_limit(const struct mylite_sql_ast_node *limit_clause,
                               struct mylite_update_rowset *rowset);
static int execute_update_rows_transaction(mylite_stmt *stmt,
                                           const struct mylite_select_table *table,
                                           const struct mylite_insert_table *write_table,
                                           const struct mylite_update_bound_assignment *assignments,
                                           size_t assignment_count,
                                           const struct mylite_update_rowset *rowset);
static int execute_update_row(mylite_stmt *stmt, sqlite3_stmt *update,
                              const struct mylite_select_table *table,
                              const struct mylite_insert_table *write_table,
                              const struct mylite_update_bound_assignment *assignments,
                              size_t assignment_count, const struct mylite_update_row *stored,
                              uint64_t *next_auto_increment);
static int write_update_candidate(mylite_stmt *stmt, sqlite3_stmt *update,
                                  const struct mylite_select_table *table,
                                  const struct mylite_insert_table *write_table,
                                  const struct mylite_update_row *candidate,
                                  uint64_t *next_auto_increment);
static char *build_update_physical_sql(mylite_db *database,
                                       const struct mylite_select_table *table);
static int copy_update_candidate_values(mylite_stmt *stmt, const struct mylite_update_row *row,
                                        struct mylite_update_row *candidate);
static int apply_update_assignments(mylite_stmt *stmt, const struct mylite_select_table *table,
                                    const struct mylite_insert_table *write_table,
                                    const struct mylite_update_bound_assignment *assignments,
                                    size_t assignment_count, struct mylite_update_row *candidate);
static int evaluate_update_assignment_value(mylite_stmt *stmt,
                                            const struct mylite_select_table *table,
                                            const struct mylite_insert_table *write_table,
                                            const struct mylite_update_row *candidate,
                                            size_t target_column,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_expression_value *out_value);
static int resolve_update_default_value(mylite_stmt *stmt,
                                        const struct mylite_insert_table_column *column,
                                        struct mylite_expression_value *out_value);
static int copy_insert_bound_value_to_expression(const struct mylite_insert_bound_value *value,
                                                 struct mylite_expression_value *out_value);
static int validate_update_assignment_value(mylite_stmt *stmt,
                                            const struct mylite_insert_table_column *column,
                                            struct mylite_expression_value *value);
static int validate_update_unique_indexes(mylite_stmt *stmt,
                                          const struct mylite_select_table *table,
                                          const struct mylite_insert_table *write_table,
                                          const struct mylite_update_row *candidate);
static int update_unique_index_conflicts(mylite_stmt *stmt, const struct mylite_select_table *table,
                                         const struct mylite_insert_table *write_table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_update_row *candidate,
                                         bool *out_conflicts);
static char *build_update_unique_check_sql(mylite_db *database,
                                           const struct mylite_select_table *table,
                                           const struct mylite_insert_table *write_table,
                                           const struct mylite_insert_unique_index *index);
static int bind_update_unique_check_values(mylite_db *database, sqlite3_stmt *check,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_update_row *candidate);
static int bind_update_row_values(mylite_db *database, sqlite3_stmt *update,
                                  const struct mylite_update_row *candidate);
static int bind_update_value(sqlite3_stmt *stmt, int index,
                             const struct mylite_expression_value *value);
static int advance_update_auto_increment(mylite_stmt *stmt, const struct mylite_select_table *table,
                                         const struct mylite_insert_table *write_table,
                                         const struct mylite_update_row *candidate,
                                         uint64_t *next_auto_increment);
static bool update_expression_value_positive_uint64(const struct mylite_expression_value *value,
                                                    uint64_t *out_value);
static bool update_row_changed(const struct mylite_update_row *stored,
                               const struct mylite_update_row *candidate);
static bool update_values_equal(const struct mylite_expression_value *left,
                                const struct mylite_expression_value *right);
static int resolve_update_expression_identifier(void *user_data,
                                                const struct mylite_sql_ast_node *identifier,
                                                struct mylite_expression_value *out_value);
static size_t update_column_reference_index(const struct mylite_select_table *table,
                                            const struct mylite_update_column_reference *reference);
static bool
update_column_reference_qualifiers_match(const struct mylite_select_table *table,
                                         const struct mylite_update_column_reference *reference);
static char *
copy_update_column_reference_name(const struct mylite_update_column_reference *reference);
static int set_update_unknown_column_error(mylite_db *database, const char *column_name,
                                           const char *clause_context);
static int set_update_unknown_field_error(mylite_db *database, const char *column_name);
static int set_update_duplicate_entry_error(mylite_db *database, const char *table_name,
                                            const struct mylite_insert_unique_index *index,
                                            const struct mylite_update_row *candidate);
static char *copy_update_duplicate_entry_value(const struct mylite_insert_unique_index *index,
                                               const struct mylite_update_row *candidate);
static int set_update_unsupported_expression_error(mylite_db *database, const char *clause_context);
static int set_update_unsupported_clause_error(mylite_db *database);
static int set_update_unsupported_assignment_error(mylite_db *database);
static int execute_scalar_select_statement(mylite_stmt *stmt);
static int execute_table_select_statement(mylite_stmt *stmt);
static int materialize_table_select_result(mylite_stmt *stmt);
static int materialize_ordered_table_select_result(mylite_stmt *stmt);
static int materialize_unordered_table_select_result(mylite_stmt *stmt);
static int evaluate_table_select_row_matches(mylite_stmt *stmt, bool *out_matches);
static int append_table_select_result_row(mylite_stmt *stmt, struct mylite_table_select_row *row);
static int copy_table_select_sqlite_row(mylite_stmt *stmt, struct mylite_table_select_row *out_row);
static int evaluate_table_select_order_values(mylite_stmt *stmt,
                                              struct mylite_table_select_row *row);
static int evaluate_table_select_order_key(mylite_stmt *stmt,
                                           const struct mylite_table_select_row *row,
                                           const struct mylite_select_order_key *order_key,
                                           struct mylite_expression_value *out_value);
static int sort_table_select_result_rows(mylite_stmt *stmt);
// NOLINTNEXTLINE(misc-no-recursion)
static int merge_sort_table_select_rows(struct mylite_table_select_row *rows,
                                        struct mylite_table_select_row *scratch, size_t first,
                                        size_t last, const struct mylite_select_plan *plan);
static void merge_table_select_rows(struct mylite_table_select_row *rows,
                                    struct mylite_table_select_row *scratch, size_t first,
                                    size_t middle, size_t last,
                                    const struct mylite_select_plan *plan);
static int compare_table_select_rows(const struct mylite_table_select_row *left,
                                     const struct mylite_table_select_row *right,
                                     const struct mylite_select_plan *plan);
static int compare_table_select_values(const struct mylite_expression_value *left,
                                       const struct mylite_expression_value *right);
static int compare_table_select_text_values(const char *left, const char *right);
static int apply_table_select_limit(mylite_stmt *stmt);
static bool table_select_limit_row_is_kept(const struct mylite_select_limit *limit,
                                           struct mylite_select_limit_position position);
static bool table_select_limit_is_full(const struct mylite_select_limit *limit, size_t kept_count);
static int set_table_select_current_row(mylite_stmt *stmt,
                                        const struct mylite_table_select_row *row);
static int evaluate_table_select_output_value(mylite_stmt *stmt,
                                              const struct mylite_table_select_row *row,
                                              size_t output_index,
                                              struct mylite_expression_value *out_value);
static int copy_table_select_row_value(const struct mylite_table_select_row *row,
                                       size_t column_index,
                                       struct mylite_expression_value *out_value);
static const struct mylite_expression_value *
table_select_current_output_value(const mylite_stmt *stmt, int column);
static const char *table_select_current_output_text(const mylite_stmt *stmt, int column);
static void table_select_current_values_deinit(struct mylite_table_select_result *result);
static int evaluate_table_select_constant_predicate(mylite_stmt *stmt);
static int evaluate_table_select_row_predicate(mylite_stmt *stmt, bool *out_matches);
static int evaluate_table_select_cached_constant_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int resolve_table_select_expression_identifier(void *user_data,
                                                      const struct mylite_sql_ast_node *identifier,
                                                      struct mylite_expression_value *out_value);
static int copy_table_select_column_value(mylite_stmt *stmt, size_t column_index,
                                          struct mylite_expression_value *out_value);
static int set_where_predicate_eval_error(mylite_stmt *stmt);
static int validate_create_table_plan(mylite_stmt *stmt, const char *schema_name,
                                      struct mylite_schema_default *schema_default);
static int create_table_transaction(mylite_stmt *stmt, const char *schema_name,
                                    const struct mylite_schema_default *schema_default);
static int create_physical_table(mylite_stmt *stmt, const char *schema_name,
                                 const struct mylite_schema_default *schema_default);
static int insert_table_catalog_row(mylite_stmt *stmt, const char *schema_name,
                                    const struct mylite_schema_default *schema_default);
static int insert_column_catalog_rows(mylite_stmt *stmt, const char *schema_name,
                                      const struct mylite_schema_default *schema_default);
static int insert_column_catalog_row(mylite_stmt *stmt, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_schema_default *schema_default,
                                     const struct mylite_create_table_column *column,
                                     size_t column_index);
static int insert_index_catalog_rows(mylite_stmt *stmt, const char *schema_name);
static int insert_index_catalog_part(mylite_stmt *stmt, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_create_table_index *index,
                                     const struct mylite_create_table_key_part *part,
                                     size_t part_index);
static int validate_drop_table_plan(mylite_stmt *stmt);
static int validate_drop_table_temporary_target(mylite_stmt *stmt,
                                                const struct mylite_drop_table_target *target);
static int validate_drop_table_target(mylite_stmt *stmt, struct mylite_drop_table_target *target);
static bool drop_table_target_is_duplicate(const struct mylite_drop_table_plan *plan,
                                           size_t target_index);
static int drop_table_transaction(mylite_stmt *stmt);
static int drop_physical_table(mylite_stmt *stmt, const struct mylite_drop_table_target *target);
static int delete_table_catalog_rows(mylite_stmt *stmt,
                                     const struct mylite_drop_table_target *target);
static int delete_table_catalog_row(mylite_db *database, const char *sql,
                                    const struct mylite_drop_table_target *target);
static int validate_insert_values_target(mylite_stmt *stmt, const char **out_schema_name);
static int load_insert_table(mylite_stmt *stmt, const char *schema_name,
                             struct mylite_insert_table *out_table);
static int load_write_table(mylite_stmt *stmt, const char *schema_name, const char *table_name,
                            struct mylite_insert_table *out_table);
static int load_insert_columns(mylite_stmt *stmt, const char *schema_name, const char *table_name,
                               struct mylite_insert_table *table);
static int load_insert_column_from_catalog_row(mylite_stmt *stmt, sqlite3_stmt *select,
                                               struct mylite_insert_table *table);
static int add_insert_table_column(struct mylite_insert_table *table,
                                   struct mylite_insert_table_column column);
static int load_insert_unique_indexes(mylite_stmt *stmt, const char *schema_name,
                                      const char *table_name, struct mylite_insert_table *table);
static int add_insert_unique_index_part(struct mylite_db *database,
                                        struct mylite_insert_table *table,
                                        const struct mylite_insert_unique_index_part_name *part);
static int append_insert_unique_index_part(struct mylite_insert_unique_index *index,
                                           size_t column_index);
static int initialize_insert_auto_increment(mylite_stmt *stmt, struct mylite_insert_table *table,
                                            uint64_t catalog_auto_increment,
                                            bool has_catalog_auto_increment);
static int read_insert_auto_increment_max(mylite_stmt *stmt,
                                          const struct mylite_insert_table *table,
                                          uint64_t *out_next_auto_increment);
static int validate_insert_column_list(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                       size_t **out_column_indexes);
static int execute_insert_values_transaction(mylite_stmt *stmt, const char *schema_name,
                                             const struct mylite_insert_table *table,
                                             const size_t *column_indexes);
static int finish_failed_insert_values_transaction(
    mylite_stmt *stmt, const char *schema_name, const struct mylite_insert_table *table,
    const struct mylite_insert_execution_state *state, int original_status);
static char *build_insert_physical_sql(mylite_db *database,
                                       const struct mylite_insert_table *table);
static int execute_insert_row(mylite_stmt *stmt, sqlite3_stmt *insert,
                              const struct mylite_insert_table *table, const size_t *column_indexes,
                              struct mylite_insert_execution_state *state, size_t row_index);
static void record_insert_row_auto_increment_id(const struct mylite_insert_table *table,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_execution_state *state);
static int advance_insert_row_auto_increment(const struct mylite_insert_table *table,
                                             const struct mylite_insert_bound_value *values,
                                             struct mylite_insert_execution_state *state);
static int resolve_insert_row_values(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                     const size_t *column_indexes,
                                     struct mylite_insert_execution_state *state, size_t row_index,
                                     struct mylite_insert_bound_value *values);
static int validate_insert_set_assignments(mylite_stmt *stmt,
                                           const struct mylite_insert_table *table,
                                           const char *schema_name, size_t **out_column_indexes);
static int execute_insert_set_transaction(mylite_stmt *stmt, const char *schema_name,
                                          const struct mylite_insert_table *table,
                                          const size_t *column_indexes);
static int initialize_insert_set_row_values(mylite_stmt *stmt,
                                            const struct mylite_insert_table *table,
                                            struct mylite_insert_execution_state *state,
                                            struct mylite_insert_bound_value *values,
                                            struct mylite_insert_set_row_state *row_state);
static int apply_insert_set_assignments(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                        const size_t *column_indexes,
                                        struct mylite_insert_bound_value *values,
                                        struct mylite_insert_set_row_state *row_state);
static int finish_insert_set_row_values(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                        struct mylite_insert_execution_state *state,
                                        struct mylite_insert_bound_value *values,
                                        const struct mylite_insert_set_row_state *row_state);
static int evaluate_insert_set_assignment_value(
    mylite_stmt *stmt, const struct mylite_insert_table *table, size_t target_column,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *values,
    bool *out_generate_auto_increment, struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_expression(mylite_stmt *stmt,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_value *value,
                                          const struct mylite_insert_bound_value *values,
                                          struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_simple_expression(mylite_stmt *stmt,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_column_reference(mylite_stmt *stmt,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_column_reference *ref,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_unary_expression(mylite_stmt *stmt,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_value *value,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value);
static int evaluate_insert_set_binary_expression(mylite_stmt *stmt,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value);
static int copy_insert_bound_value(const struct mylite_insert_bound_value *value,
                                   struct mylite_insert_bound_value *out_value);
static bool insert_bound_value_is_numeric(const struct mylite_insert_bound_value *value,
                                          double *out_value, bool *out_is_integer);
static int
resolve_insert_implicit_expression_default(mylite_stmt *stmt,
                                           const struct mylite_insert_table_column *column,
                                           struct mylite_insert_bound_value *out_value);
static int set_insert_set_candidate_auto_value(struct mylite_insert_bound_value *out_value);
static int resolve_insert_explicit_value(mylite_stmt *stmt,
                                         const struct mylite_insert_table_column *column,
                                         const struct mylite_insert_value *value,
                                         struct mylite_insert_execution_state *state,
                                         struct mylite_insert_bound_value *out_value);
static int resolve_insert_default_value(mylite_stmt *stmt,
                                        const struct mylite_insert_table_column *column,
                                        struct mylite_insert_execution_state *state,
                                        struct mylite_insert_bound_value *out_value);
static int resolve_insert_text_value(mylite_stmt *stmt,
                                     const struct mylite_insert_table_column *column,
                                     const char *text, struct mylite_insert_execution_state *state,
                                     struct mylite_insert_bound_value *out_value);
static int allocate_insert_auto_increment(mylite_stmt *stmt,
                                          struct mylite_insert_execution_state *state,
                                          struct mylite_insert_bound_value *out_value);
static int reserve_insert_auto_increment(mylite_stmt *stmt,
                                         struct mylite_insert_execution_state *state,
                                         uint64_t first_value);
static uint64_t insert_statement_row_count(const mylite_stmt *stmt);
static uint64_t insert_auto_increment_next_value(const struct mylite_insert_execution_state *state);
static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name);
static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference);
static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name);
static int bind_insert_row_values(mylite_db *database, sqlite3_stmt *insert,
                                  const struct mylite_insert_bound_value *values,
                                  size_t value_count);
static int bind_insert_bound_value(sqlite3_stmt *stmt, int index,
                                   const struct mylite_insert_bound_value *value);
static int validate_insert_unique_indexes(mylite_stmt *stmt,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_bound_value *values);
static int insert_unique_index_conflicts(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_insert_bound_value *values,
                                         bool *out_conflicts);
static char *build_insert_unique_check_sql(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_insert_bound_value *values);
static int bind_insert_unique_check_values(mylite_db *database, sqlite3_stmt *check,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_insert_bound_value *values);
static int update_insert_auto_increment(mylite_stmt *stmt, const char *schema_name,
                                        uint64_t next_auto_increment);
static int update_table_auto_increment(mylite_stmt *stmt, const char *schema_name,
                                       const char *table_name, uint64_t next_auto_increment);
static bool insert_row_uses_all_defaults(const struct mylite_insert_values_plan *plan,
                                         size_t row_index);
static size_t insert_row_target_column_count(const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             size_t row_index);
static int set_insert_wrong_value_count_error(mylite_db *database, size_t row_index);
static int set_insert_no_default_error(mylite_db *database, const char *column_name);
static int set_insert_null_error(mylite_db *database, const char *column_name);
static int set_insert_unsupported_generated_default_error(mylite_db *database,
                                                          const char *column_name);
static int set_insert_unsupported_expression_error(mylite_db *database);
static int set_insert_duplicate_entry_error(mylite_db *database, const char *table_name,
                                            const struct mylite_insert_unique_index *index,
                                            const struct mylite_insert_bound_value *values);
static char *copy_insert_duplicate_entry_value(const struct mylite_insert_unique_index *index,
                                               const struct mylite_insert_bound_value *values);
static int set_table_doesnt_exist_error(mylite_db *database, const char *schema_name,
                                        const char *table_name);
static int table_exists(mylite_db *database, const char *schema_name, const char *table_name,
                        bool *out_exists);
static int schema_default_by_name(mylite_db *database, const char *schema_name,
                                  struct mylite_schema_default *out_default);
static int set_names_connection_state(mylite_db *database,
                                      struct mylite_connection_charset_request request);
static int set_character_set_connection_state(mylite_db *database, const char *character_set_name);
static int set_default_connection_state(mylite_db *database);
static int selected_schema_default(mylite_db *database, struct mylite_schema_default *out_default);
static int schema_exists(mylite_db *database, const char *schema_name,
                         struct mylite_schema_presence *out_presence);
static int insert_schema(mylite_db *database, const char *schema_name,
                         const struct mylite_schema_options *options);
static int update_schema(mylite_db *database, const char *schema_name,
                         const struct mylite_schema_options *options);
static int delete_schema(mylite_db *database, const char *schema_name);
static int set_selected_schema(mylite_db *database, const char *schema_name);
static void clear_selected_schema_if_matches(mylite_db *database, const char *schema_name);
static int information_schema_table_from_select(const struct mylite_sql_ast_node *statement,
                                                enum mylite_information_schema_table *out_table);
static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list);
static int
information_schema_table_from_from_clause(const struct mylite_sql_ast_node *from_clause,
                                          enum mylite_information_schema_table *out_table);
static int
information_schema_table_from_qualified_name(const struct mylite_sql_ast_node *identifier,
                                             enum mylite_information_schema_table *out_table);
static enum mylite_information_schema_table information_schema_table_from_name(const char *name);
static const char *information_schema_table_sql(enum mylite_information_schema_table table);
static int copy_statement_schema_name(const struct mylite_sql_ast_node *statement,
                                      enum mylite_stmt_kind kind, char **out_schema_name);
static int copy_schema_options(const struct mylite_sql_ast_node *statement,
                               enum mylite_stmt_kind kind, struct mylite_schema_options *options);
static int copy_connection_charset_statement(const struct mylite_sql_ast_node *statement,
                                             mylite_stmt *stmt);
static int copy_create_table_statement(const struct mylite_sql_ast_node *statement,
                                       mylite_stmt *stmt);
static int copy_drop_table_statement(const struct mylite_sql_ast_node *statement,
                                     mylite_stmt *stmt);
static int copy_insert_values_statement(const struct mylite_sql_ast_node *statement,
                                        mylite_stmt *stmt);
static int copy_insert_set_statement(const struct mylite_sql_ast_node *statement,
                                     mylite_stmt *stmt);
static int copy_update_statement(const struct mylite_sql_ast_node *statement, mylite_stmt *stmt);
static int copy_scalar_select_statement(const struct mylite_sql_ast_node *statement,
                                        mylite_stmt *stmt);
static int copy_create_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_create_table_plan *plan);
static int copy_drop_table_target(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_drop_table_target *target);
static int copy_insert_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_insert_values_plan *plan);
static int add_drop_table_target(struct mylite_drop_table_plan *plan,
                                 struct mylite_drop_table_target target);
static int copy_insert_column_list(const struct mylite_sql_ast_node *columns,
                                   struct mylite_insert_values_plan *plan);
static int add_insert_column(struct mylite_insert_values_plan *plan, char *column_name);
static int copy_insert_rows(const struct mylite_sql_ast_node *rows,
                            struct mylite_insert_values_plan *plan);
static int copy_insert_row(const struct mylite_sql_ast_node *row,
                           struct mylite_insert_values_plan *plan);
static int add_insert_row(struct mylite_insert_values_plan *plan, struct mylite_insert_row row);
static int copy_insert_value(const struct mylite_sql_ast_node *value_node,
                             struct mylite_insert_value *out_value);
static int copy_insert_simple_value(const struct mylite_sql_ast_node *value_node,
                                    struct mylite_insert_value *out_value);
static int copy_insert_column_reference(const struct mylite_sql_ast_node *identifier,
                                        struct mylite_insert_column_reference *out_reference);
static int copy_insert_column_reference_parts(const struct mylite_sql_ast_node *identifier,
                                              char **parts, size_t *part_count);
static int copy_insert_literal_value(const struct mylite_sql_ast_node *literal,
                                     struct mylite_insert_value *out_value);
static int copy_insert_unary_value(const struct mylite_sql_ast_node *expression,
                                   struct mylite_insert_value *out_value);
static int copy_insert_binary_value(const struct mylite_sql_ast_node *expression,
                                    struct mylite_insert_value *out_value);
static int copy_insert_set_assignments(const struct mylite_sql_ast_node *assignments,
                                       struct mylite_insert_set_plan *plan);
static int copy_insert_set_assignment(const struct mylite_sql_ast_node *assignment,
                                      struct mylite_insert_set_plan *plan);
static int add_insert_set_assignment(struct mylite_insert_set_plan *plan,
                                     struct mylite_insert_set_assignment assignment);
static int copy_update_target(const struct mylite_sql_ast_node *target,
                              struct mylite_update_target *out_target);
static int copy_update_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_update_target *target);
static int copy_update_assignments(const struct mylite_sql_ast_node *assignments,
                                   struct mylite_update_plan *plan);
static int copy_update_assignment(const struct mylite_sql_ast_node *assignment,
                                  struct mylite_update_plan *plan);
static int add_update_assignment(struct mylite_update_plan *plan,
                                 struct mylite_update_assignment assignment);
static int copy_update_column_reference(const struct mylite_sql_ast_node *identifier,
                                        struct mylite_update_column_reference *out_reference);
static int copy_update_column_reference_parts(const struct mylite_sql_ast_node *identifier,
                                              char **parts, size_t *part_count);
static int copy_create_table_elements(const struct mylite_sql_ast_node *elements,
                                      struct mylite_create_table_plan *plan);
static int copy_create_table_column(const struct mylite_sql_ast_node *column_node,
                                    struct mylite_create_table_plan *plan);
static int copy_create_table_column_type(const struct mylite_sql_ast_node *type_node,
                                         struct mylite_create_table_column_type *type);
static int copy_create_table_column_attributes(const struct mylite_sql_ast_node *attributes,
                                               struct mylite_create_table_column *column);
static int copy_create_table_index(const struct mylite_sql_ast_node *index_node,
                                   struct mylite_create_table_plan *plan);
static int copy_create_table_key_parts(const struct mylite_sql_ast_node *key_parts,
                                       struct mylite_create_table_index *index);
static int copy_create_table_index_options(const struct mylite_sql_ast_node *options,
                                           struct mylite_create_table_index *index);
static int copy_create_table_options(const struct mylite_sql_ast_node *statement,
                                     struct mylite_create_table_options *options);
static int add_create_table_index(struct mylite_create_table_plan *plan,
                                  struct mylite_create_table_index index);
static int add_inline_create_table_column_indexes(struct mylite_create_table_plan *plan,
                                                  const struct mylite_create_table_column *column);
static int add_single_column_index(struct mylite_create_table_plan *plan, const char *column_name,
                                   bool is_primary, bool is_unique);
static int assign_generated_index_names(mylite_db *database, struct mylite_create_table_plan *plan);
static int assign_generated_index_name(mylite_db *database, struct mylite_create_table_plan *plan,
                                       size_t index);
static char *generated_index_name_candidate(const char *base, unsigned int suffix);
static bool create_table_index_name_exists(const struct mylite_create_table_plan *plan,
                                           const char *name, size_t before_index);
static int describe_create_table_column(const struct mylite_create_table_column *column,
                                        const struct mylite_schema_default *schema_default,
                                        const struct mylite_create_table_options *table_options,
                                        struct mylite_column_type_descriptor *out_descriptor);
static const char *create_table_column_type_name(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_integer_descriptor(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_string_binary_descriptor(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_numeric_descriptor(enum mylite_sql_ast_column_type column_type);
static bool
create_table_column_uses_temporal_descriptor(enum mylite_sql_ast_column_type column_type);
static const char *
sqlite_affinity_for_descriptor(const struct mylite_column_type_descriptor *descriptor);
static char *physical_table_name(const char *schema_name, const char *table_name);
static char *build_create_physical_table_sql(mylite_stmt *stmt, const char *physical_name,
                                             const struct mylite_schema_default *schema_default);
static char *copy_expression_text(const struct mylite_sql_ast_node *node);
static int normalize_create_table_options(mylite_db *database, const char *schema_name,
                                          const struct mylite_schema_default *schema_default,
                                          struct mylite_create_table_options *options);
static int normalize_create_table_option_text(mylite_db *database, char **target,
                                              const char *value);
static bool is_supported_engine_name(const char *name);
static bool validate_create_table_column_names(mylite_db *database,
                                               const struct mylite_create_table_plan *plan);
static bool validate_create_table_indexes(mylite_db *database,
                                          const struct mylite_create_table_plan *plan);
static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan);
static const struct mylite_create_table_column *
find_create_table_column(const struct mylite_create_table_plan *plan, const char *name);
static struct mylite_create_table_column_index_status
create_table_column_index_status(const struct mylite_create_table_plan *plan,
                                 const char *column_name);
static const char *create_table_column_key(const struct mylite_create_table_plan *plan,
                                           const char *column_name);
static const char *create_table_column_extra(const struct mylite_create_table_column *column);
static const char *index_collation_for_order(enum mylite_sql_ast_key_part_order order);
static int begin_sqlite_transaction(mylite_db *database);
static int commit_sqlite_transaction(mylite_db *database);
static void rollback_sqlite_transaction(mylite_db *database);
static int apply_schema_option(const struct mylite_sql_ast_node *option,
                               struct mylite_schema_options *options);
static int normalize_schema_options(mylite_db *database, struct mylite_schema_options *options);
static int normalize_schema_charset_and_collation(mylite_db *database,
                                                  struct mylite_schema_options *options);
static int normalize_schema_option_text(mylite_db *database, char **target, const char *value);
static int set_unknown_charset_error(mylite_db *database, const char *name);
static int set_unknown_collation_error(mylite_db *database, const char *name);
static int set_collation_charset_error(mylite_db *database, const char *collation,
                                       const char *character_set);
static int set_unknown_table_error(mylite_db *database, const char *schema_name,
                                   const char *table_name);
static bool is_valid_encryption_value(const char *value);
static bool ascii_case_equal(const char *left, const char *right);
static char *copy_identifier_span(const struct mylite_sql_ast_node *node);
static char *copy_string_literal_span(const struct mylite_sql_ast_node *node);
static char *copy_schema_text_span(const struct mylite_sql_ast_node *node);
static char *copy_span_text(const char *text, size_t length);
static bool span_contains_newline(const char *text, size_t length);
static bool text_contains_word(const char *text, const char *word);
static const struct mylite_result_column_metadata *result_metadata_column(const mylite_stmt *stmt,
                                                                          int column);
static bool
insert_column_uses_numeric_implicit_default(const struct mylite_insert_table_column *column);
static bool column_default_is_current_timestamp(const char *default_text);
static bool parse_insert_integer_text(const char *text, int64_t *out_value);
static bool parse_insert_real_text(const char *text, double *out_value);
static char *insert_current_timestamp_text(void);
static void schema_options_deinit(struct mylite_schema_options *options);
static void create_table_options_deinit(struct mylite_create_table_options *options);
static void create_table_plan_deinit(struct mylite_create_table_plan *plan);
static void drop_table_plan_deinit(struct mylite_drop_table_plan *plan);
static void drop_table_target_deinit(struct mylite_drop_table_target *target);
static void insert_values_plan_deinit(struct mylite_insert_values_plan *plan);
static void insert_set_plan_deinit(struct mylite_insert_set_plan *plan);
static void insert_set_assignment_deinit(struct mylite_insert_set_assignment *assignment);
static void insert_column_reference_deinit(struct mylite_insert_column_reference *reference);
static void update_plan_deinit(struct mylite_update_plan *plan);
static void update_target_deinit(struct mylite_update_target *target);
static void update_assignment_deinit(struct mylite_update_assignment *assignment);
static void update_column_reference_deinit(struct mylite_update_column_reference *reference);
static void update_order_plan_deinit(struct mylite_update_order_plan *plan);
static void update_rowset_deinit(struct mylite_update_rowset *rowset);
static void update_row_deinit(struct mylite_update_row *row);
static void insert_row_deinit(struct mylite_insert_row *row);
static void insert_value_deinit(struct mylite_insert_value *value);
static void insert_value_child_deinit(struct mylite_insert_value *value);
static void insert_table_deinit(struct mylite_insert_table *table);
static void insert_table_column_deinit(struct mylite_insert_table_column *column);
static void insert_unique_index_deinit(struct mylite_insert_unique_index *index);
static void result_metadata_deinit(struct mylite_result_metadata *metadata);
static void scalar_result_deinit(struct mylite_scalar_result *result);
static void table_select_result_deinit(struct mylite_table_select_result *result);
static void table_select_row_deinit(struct mylite_table_select_row *row);
static void result_column_metadata_deinit(struct mylite_result_column_metadata *metadata);
static void select_plan_deinit(struct mylite_select_plan *plan);
static void select_constant_values_deinit(mylite_stmt *stmt);
static void select_table_deinit(struct mylite_select_table *table);
static void select_column_deinit(struct mylite_select_column *column);
static void select_output_column_deinit(struct mylite_select_output_column *column);
static void insert_bound_values_deinit(struct mylite_insert_bound_value *values,
                                       size_t value_count);
static void insert_bound_value_deinit(struct mylite_insert_bound_value *value);
static void create_table_column_deinit(struct mylite_create_table_column *column);
static void create_table_index_deinit(struct mylite_create_table_index *index);
static void create_table_key_part_deinit(struct mylite_create_table_key_part *part);
static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index);
static const struct mylite_sql_ast_node *find_child_kind(const struct mylite_sql_ast_node *node,
                                                         enum mylite_sql_ast_node_kind kind);
static const struct mylite_sql_ast_node *single_statement(const struct mylite_sql_ast_node *root);
static int map_parse_status(mylite_db *database, enum mylite_sql_parse_status status);
static int map_translate_status(mylite_db *database, enum mylite_sqlite_translate_status status);
static int set_sqlite_error(mylite_db *database);
static void clear_warnings(mylite_db *database);
static int set_error_message(mylite_db *database, const char *message);
static int set_error_message_parts(mylite_db *database, const char *prefix, const char *value,
                                   const char *suffix);
static int append_database_warning(mylite_db *database, unsigned int code, const char *message);
static void clear_error_message(mylite_db *database);
static sqlite3_destructor_type sqlite_transient_destructor(void);

const char *mylite_status_name(int status)
{
    switch (status) {
    case MYLITE_OK:
        return "ok";
    case MYLITE_MISUSE:
        return "misuse";
    case MYLITE_NOMEM:
        return "nomem";
    case MYLITE_PARSE_ERROR:
        return "parse_error";
    case MYLITE_UNSUPPORTED:
        return "unsupported";
    case MYLITE_SQLITE_ERROR:
        return "sqlite_error";
    case MYLITE_EXEC_ERROR:
        return "exec_error";
    case MYLITE_ROW:
        return "row";
    case MYLITE_DONE:
        return "done";
    default:
        return "unknown";
    }
}

int mylite_open(const char *filename, mylite_db **out_db)
{
    int rc = SQLITE_OK;

    if (filename == NULL || out_db == NULL) {
        return MYLITE_MISUSE;
    }

    *out_db = NULL;
    rc = mylite_vfs_register();
    if (rc != SQLITE_OK) {
        return MYLITE_SQLITE_ERROR;
    }

    return open_sqlite_database(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                mylite_vfs_name(), out_db);
}

int mylite_open_memory(mylite_db **out_db)
{
    if (out_db == NULL) {
        return MYLITE_MISUSE;
    }

    return open_sqlite_database(
        ":memory:", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, NULL, out_db);
}

void mylite_close(mylite_db *database)
{
    if (database == NULL) {
        return;
    }

    sqlite3_close(database->sqlite);
    free(database->error_message);
    mylite_expression_warnings_deinit(&database->warnings);
    free(database->selected_schema);
    free(database);
}

const char *mylite_error_message(const mylite_db *database)
{
    if (database == NULL || database->error_message == NULL) {
        return "";
    }

    return database->error_message;
}

int mylite_prepare(mylite_db *database, const char *sql, size_t length, mylite_stmt **out_stmt)
{
    struct mylite_sql_parse_result parse_result;
    enum mylite_sql_parse_status parse_status = MYLITE_SQL_PARSE_OK;
    int status = MYLITE_OK;

    if (out_stmt == NULL) {
        return MYLITE_MISUSE;
    }
    *out_stmt = NULL;

    if (database == NULL || sql == NULL) {
        return MYLITE_MISUSE;
    }

    clear_error_message(database);
    clear_warnings(database);
    parse_status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = length,
            .modes = 0U,
        },
        &parse_result);
    if (parse_status != MYLITE_SQL_PARSE_OK) {
        status = map_parse_status(database, parse_status);
        mylite_sql_parse_result_deinit(&parse_result);
        return status;
    }

    status = prepare_parsed_statement(database, parse_result.root, sql, length, out_stmt);
    mylite_sql_parse_result_deinit(&parse_result);
    return status;
}

void mylite_finalize(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return;
    }

    sqlite3_finalize(stmt->sqlite_stmt);
    free(stmt->schema_name);
    free(stmt->character_set_name);
    free(stmt->collation_name);
    schema_options_deinit(&stmt->options);
    create_table_plan_deinit(&stmt->create_table);
    drop_table_plan_deinit(&stmt->drop_table);
    insert_values_plan_deinit(&stmt->insert_values);
    insert_set_plan_deinit(&stmt->insert_set);
    update_plan_deinit(&stmt->update);
    select_plan_deinit(&stmt->select_plan);
    result_metadata_deinit(&stmt->result_metadata);
    scalar_result_deinit(&stmt->scalar_result);
    table_select_result_deinit(&stmt->select_result);
    mylite_sql_ast_deinit(&stmt->select_predicate_ast);
    mylite_sql_ast_deinit(&stmt->update_ast);
    free(stmt->select_sql_text);
    free(stmt->update_sql_text);
    select_constant_values_deinit(stmt);
    free(stmt);
}

int mylite_step(mylite_stmt *stmt)
{
    int rc = SQLITE_OK;

    if (stmt == NULL) {
        return MYLITE_MISUSE;
    }

    clear_error_message(stmt->database);
    if (!stmt->executed) {
        clear_warnings(stmt->database);
    }
    if (stmt->kind != MYLITE_STMT_SQLITE) {
        return execute_custom_statement(stmt);
    }

    rc = sqlite3_step(stmt->sqlite_stmt);
    if (rc == SQLITE_ROW) {
        return MYLITE_ROW;
    }
    if (rc == SQLITE_DONE) {
        stmt->affected_rows =
            sqlite3_stmt_readonly(stmt->sqlite_stmt) ? -1 : sqlite3_changes(stmt->database->sqlite);
        return MYLITE_DONE;
    }

    return set_sqlite_error(stmt->database);
}

int64_t mylite_affected_rows(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return -1;
    }

    return stmt->affected_rows;
}

uint64_t mylite_last_insert_id(const mylite_db *database)
{
    if (database == NULL) {
        return 0U;
    }

    return database->last_insert_id;
}

int mylite_warning_count(const mylite_db *database)
{
    return database == NULL ? 0 : (int)database->warnings.count;
}

unsigned int mylite_warning_code(const mylite_db *database, int warning)
{
    if (database == NULL || warning < 0 || (size_t)warning >= database->warnings.count) {
        return 0U;
    }
    return database->warnings.items[warning].code;
}

const char *mylite_warning_message(const mylite_db *database, int warning)
{
    if (database == NULL || warning < 0 || (size_t)warning >= database->warnings.count) {
        return NULL;
    }
    return database->warnings.items[warning].message;
}

int mylite_column_count(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return 0;
    }

    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return (int)stmt->scalar_result.value_count;
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT) {
        return (int)stmt->result_metadata.column_count;
    }
    if (stmt->sqlite_stmt == NULL) {
        return 0;
    }

    return sqlite3_column_count(stmt->sqlite_stmt);
}

const char *mylite_column_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata = result_metadata_column(stmt, column);

    if (metadata != NULL && metadata->name != NULL) {
        return metadata->name;
    }
    if (stmt != NULL && stmt->kind == MYLITE_STMT_SCALAR_SELECT && column >= 0 &&
        (size_t)column < stmt->result_metadata.column_count) {
        return stmt->result_metadata.columns[column].name;
    }
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        return NULL;
    }

    return sqlite3_column_name(stmt->sqlite_stmt, column);
}

const char *mylite_column_schema_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata = result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->schema_name;
}

const char *mylite_column_table_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata = result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->table_name;
}

const char *mylite_column_origin_table_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata = result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->origin_table_name;
}

const char *mylite_column_origin_name(const mylite_stmt *stmt, int column)
{
    const struct mylite_result_column_metadata *metadata = result_metadata_column(stmt, column);

    return metadata == NULL ? NULL : metadata->origin_column_name;
}

int64_t mylite_column_int64(const mylite_stmt *stmt, int column)
{
    const struct mylite_expression_value *value = table_select_current_output_value(stmt, column);

    if (value != NULL) {
        return mylite_expression_value_to_int64(value);
    }
    if (stmt != NULL && stmt->sqlite_stmt != NULL && stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        column >= 0 && (size_t)column < stmt->select_plan.output_count) {
        size_t physical_column = stmt->select_plan.outputs[column].column_index;

        return (int64_t)sqlite3_column_int64(stmt->sqlite_stmt, (int)physical_column);
    }
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        if (stmt != NULL && stmt->kind == MYLITE_STMT_SCALAR_SELECT && column >= 0 &&
            (size_t)column < stmt->scalar_result.value_count) {
            return mylite_expression_value_to_int64(&stmt->scalar_result.values[column]);
        }
        return 0;
    }

    return (int64_t)sqlite3_column_int64(stmt->sqlite_stmt, column);
}

const char *mylite_column_text(const mylite_stmt *stmt, int column)
{
    if (stmt != NULL && stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        stmt->select_result.has_current_row) {
        return table_select_current_output_text(stmt, column);
    }
    if (stmt != NULL && stmt->sqlite_stmt != NULL && stmt->kind == MYLITE_STMT_TABLE_SELECT &&
        column >= 0 && (size_t)column < stmt->select_plan.output_count) {
        size_t physical_column = stmt->select_plan.outputs[column].column_index;

        return (const char *)sqlite3_column_text(stmt->sqlite_stmt, (int)physical_column);
    }
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        if (stmt != NULL && stmt->kind == MYLITE_STMT_SCALAR_SELECT && column >= 0 &&
            (size_t)column < stmt->scalar_result.value_count) {
            return stmt->scalar_result.texts[column];
        }
        return NULL;
    }

    return (const char *)sqlite3_column_text(stmt->sqlite_stmt, column);
}

const char *mylite_connection_character_set_client(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_client;
}

const char *mylite_connection_character_set_connection(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_connection;
}

const char *mylite_connection_character_set_results(const mylite_db *database)
{
    return database == NULL ? NULL : database->character_set_results;
}

const char *mylite_connection_collation_connection(const mylite_db *database)
{
    return database == NULL ? NULL : database->collation_connection;
}

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db)
{
    mylite_db *database = calloc(1U, sizeof(*database));
    int rc = SQLITE_OK;

    *out_db = NULL;
    if (database == NULL) {
        return MYLITE_NOMEM;
    }

    rc = sqlite3_open_v2(filename, &database->sqlite, flags, vfs_name);
    if (rc != SQLITE_OK) {
        sqlite3_close(database->sqlite);
        free(database);
        return MYLITE_SQLITE_ERROR;
    }

    rc = initialize_schema_catalog(database);
    if (rc != MYLITE_OK) {
        sqlite3_close(database->sqlite);
        free(database->error_message);
        free(database);
        return rc;
    }

    (void)set_default_connection_state(database);
    *out_db = database;
    return MYLITE_OK;
}

static int initialize_schema_catalog(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, schema_catalog_sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, table_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, column_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    rc = sqlite3_exec(database->sqlite, index_catalog_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    rc = seed_system_schema(database, "information_schema", "utf8mb3", "utf8mb3_general_ci");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = seed_system_schema(database, "mysql", "utf8mb4", "utf8mb4_0900_ai_ci");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = seed_system_schema(database, "performance_schema", "utf8mb4", "utf8mb4_0900_ai_ci");
    if (rc != MYLITE_OK) {
        return rc;
    }
    return seed_system_schema(database, "sys", "utf8mb4", "utf8mb4_0900_ai_ci");
}

static int seed_system_schema(mylite_db *database, const char *name, const char *character_set,
                              const char *collation)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_schema_catalog("
        "name, default_character_set, default_collation, default_encryption, read_only, is_system)"
        " VALUES(?, ?, ?, 'N', 0, 1) "
        "ON CONFLICT(name) DO UPDATE SET "
        "default_character_set = excluded.default_character_set,"
        "default_collation = excluded.default_collation,"
        "default_encryption = 'N',"
        "is_system = 1";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, character_set, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, collation, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt)
{
    struct mylite_sqlite_translate_result translate_result;
    enum mylite_sqlite_translate_status translate_status = MYLITE_SQLITE_TRANSLATE_OK;
    const struct mylite_sql_ast_node *statement = single_statement(root);
    int status = MYLITE_OK;

    if (statement != NULL) {
        switch (statement->kind) {
        case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_USE_STATEMENT:
            return prepare_schema_lifecycle_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
            return prepare_connection_charset_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
            return prepare_create_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
            return prepare_drop_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
            return prepare_insert_values_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
            return prepare_insert_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_UPDATE_STATEMENT:
            return prepare_update_statement(database, statement, sql, sql_length, out_stmt);
        case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
            return prepare_show_schemas_statement(database, out_stmt);
        case MYLITE_SQL_AST_SELECT_STATEMENT:
            status = prepare_information_schema_select_statement(database, statement, out_stmt);
            if (status != MYLITE_UNSUPPORTED) {
                return status;
            }
            status = prepare_table_select_statement(database, statement, sql, sql_length, out_stmt);
            if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
                return status;
            }
            status = prepare_scalar_select_statement(database, statement, out_stmt);
            if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
                return status;
            }
            break;
        case MYLITE_SQL_AST_SCRIPT:
        case MYLITE_SQL_AST_SELECT_LIST:
        case MYLITE_SQL_AST_SELECT_ITEM:
        case MYLITE_SQL_AST_FROM_DUAL:
        case MYLITE_SQL_AST_FROM_TABLE:
        case MYLITE_SQL_AST_IDENTIFIER:
        case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        case MYLITE_SQL_AST_WILDCARD:
        case MYLITE_SQL_AST_LITERAL:
        case MYLITE_SQL_AST_UNARY_EXPRESSION:
        case MYLITE_SQL_AST_BINARY_EXPRESSION:
        case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        case MYLITE_SQL_AST_EXPRESSION_LIST:
        case MYLITE_SQL_AST_WHERE_CLAUSE:
        case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        case MYLITE_SQL_AST_ORDER_ITEM_LIST:
        case MYLITE_SQL_AST_ORDER_ITEM:
        case MYLITE_SQL_AST_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_LIMIT_BOUND:
        case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        case MYLITE_SQL_AST_IF_EXISTS:
        case MYLITE_SQL_AST_IF_NOT_EXISTS:
        case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
        case MYLITE_SQL_AST_SCHEMA_OPTION:
        case MYLITE_SQL_AST_DEFAULT:
        case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        case MYLITE_SQL_AST_COLUMN_DEFINITION:
        case MYLITE_SQL_AST_COLUMN_TYPE:
        case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
        case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
        case MYLITE_SQL_AST_KEY_PART_LIST:
        case MYLITE_SQL_AST_KEY_PART:
        case MYLITE_SQL_AST_INDEX_TYPE:
        case MYLITE_SQL_AST_INDEX_OPTION_LIST:
        case MYLITE_SQL_AST_INDEX_OPTION:
        case MYLITE_SQL_AST_SECONDARY_INDEX:
        case MYLITE_SQL_AST_UNIQUE_INDEX:
        case MYLITE_SQL_AST_TABLE_OPTION_LIST:
        case MYLITE_SQL_AST_TABLE_OPTION:
        case MYLITE_SQL_AST_TABLE_NAME_LIST:
        case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
        case MYLITE_SQL_AST_INSERT_ROW:
        case MYLITE_SQL_AST_INSERT_ROW_LIST:
        case MYLITE_SQL_AST_INSERT_VALUE_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_TARGET:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
            break;
        }
    }

    translate_status = mylite_sqlite_translate(root, &translate_result);
    if (translate_status != MYLITE_SQLITE_TRANSLATE_OK) {
        return map_translate_status(database, translate_status);
    }

    status = prepare_sqlite_statement(database, translate_result.sql, out_stmt);
    mylite_sqlite_translate_result_deinit(&translate_result);
    return status;
}

static int prepare_schema_lifecycle_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt)
{
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;

    switch (statement->kind) {
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_CREATE_SCHEMA;
        break;
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_ALTER_SCHEMA;
        break;
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_DROP_SCHEMA;
        break;
    case MYLITE_SQL_AST_USE_STATEMENT:
        kind = MYLITE_STMT_USE_SCHEMA;
        break;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
}

static int prepare_connection_charset_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                mylite_stmt **out_stmt)
{
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;

    switch (statement->kind) {
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        kind = MYLITE_STMT_SET_NAMES;
        break;
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
        kind = MYLITE_STMT_SET_CHARACTER_SET;
        break;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
}

static int prepare_create_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_CREATE_TABLE, statement, out_stmt);
}

static int prepare_drop_table_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_DROP_TABLE, statement, out_stmt);
}

static int prepare_insert_values_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_INSERT_VALUES, statement, out_stmt);
}

static int prepare_insert_set_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_INSERT_SET, statement, out_stmt);
}

static int prepare_update_statement(mylite_db *database,
                                    const struct mylite_sql_ast_node *statement, const char *sql,
                                    size_t sql_length, mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_UPDATE,
        .affected_rows = 0,
    };

    status = copy_update_statement(statement, stmt);
    if (status == MYLITE_OK) {
        status = clone_update_plan_nodes(stmt, statement, sql, sql_length);
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

static int prepare_show_schemas_statement(mylite_db *database, mylite_stmt **out_stmt)
{
    return prepare_sqlite_statement(database, show_schemas_sql, out_stmt);
}

static int prepare_information_schema_select_statement(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement,
                                                       mylite_stmt **out_stmt)
{
    enum mylite_information_schema_table table = MYLITE_INFORMATION_SCHEMA_NONE;
    const char *sql = NULL;
    int status = information_schema_table_from_select(statement, &table);

    if (status != MYLITE_OK) {
        return status;
    }
    if (table == MYLITE_INFORMATION_SCHEMA_NONE) {
        return MYLITE_UNSUPPORTED;
    }

    sql = information_schema_table_sql(table);
    if (sql == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    return prepare_sqlite_statement(database, sql, out_stmt);
}

static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause =
        find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE);
    const struct mylite_sql_ast_node *order_by_clause =
        find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    bool custom_runtime =
        (where_clause != NULL || order_by_clause != NULL || limit_clause != NULL) != 0;
    struct mylite_select_plan plan = {0};
    char *sqlite_sql = NULL;
    int status = MYLITE_OK;

    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_select_table_reference(from_clause, &plan.table);
    if (status == MYLITE_OK) {
        status = resolve_select_table_target(database, &plan.table);
    }
    if (status == MYLITE_OK) {
        status = load_select_columns(database, &plan.table);
    }
    if (status == MYLITE_OK) {
        status = build_select_outputs(database, select_list, order_by_clause != NULL, &plan);
    }
    if (status == MYLITE_OK && where_clause != NULL) {
        status = bind_select_where_clause(database, where_clause, &plan);
    }
    if (status == MYLITE_OK && limit_clause != NULL) {
        status = bind_select_limit_clause(limit_clause, &plan);
    }
    if (status == MYLITE_OK && order_by_clause != NULL) {
        status = bind_select_order_by_clause(database, order_by_clause, &plan);
    }
    if (status == MYLITE_OK && custom_runtime) {
        status = prepare_table_select_custom_statement(database, where_clause, sql, sql_length,
                                                       &plan, out_stmt);
    }
    if (status == MYLITE_OK && !custom_runtime) {
        sqlite_sql = build_select_physical_sql(database, &plan);
        if (sqlite_sql == NULL) {
            (void)set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && !custom_runtime) {
        status = prepare_sqlite_statement(database, sqlite_sql, out_stmt);
    }
    if (status == MYLITE_OK && !custom_runtime) {
        status = attach_select_result_metadata(*out_stmt, &plan);
        if (status != MYLITE_OK) {
            mylite_finalize(*out_stmt);
            *out_stmt = NULL;
        }
    }

    sqlite3_free(sqlite_sql);
    select_plan_deinit(&plan);
    return status;
}

static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL)) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);

        if (item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !mylite_expression_is_supported_no_table(expression)) {
            return MYLITE_UNSUPPORTED;
        }
    }
    return prepare_custom_statement(database, MYLITE_STMT_SCALAR_SELECT, statement, out_stmt);
}

static int attach_select_result_metadata(mylite_stmt *stmt, const struct mylite_select_plan *plan)
{
    struct mylite_result_metadata metadata = {0};

    if (plan->output_count == 0U) {
        return MYLITE_OK;
    }

    metadata.columns = calloc(plan->output_count, sizeof(*metadata.columns));
    if (metadata.columns == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    metadata.column_count = plan->output_count;

    for (size_t index = 0U; index < plan->output_count; ++index) {
        int status = copy_select_result_column_metadata(stmt->database, &metadata.columns[index],
                                                        plan, index);

        if (status != MYLITE_OK) {
            result_metadata_deinit(&metadata);
            return status;
        }
    }

    result_metadata_deinit(&stmt->result_metadata);
    stmt->result_metadata = metadata;
    return MYLITE_OK;
}

static int copy_select_result_column_metadata(mylite_db *database,
                                              struct mylite_result_column_metadata *metadata,
                                              const struct mylite_select_plan *plan,
                                              size_t output_index)
{
    const struct mylite_select_output_column *output = &plan->outputs[output_index];
    const char *visible_table_name =
        plan->table.alias == NULL ? plan->table.table_name : plan->table.alias;
    int status = copy_result_metadata_text(database, &metadata->name, output->label);

    if (output->kind == MYLITE_SELECT_OUTPUT_EXPRESSION) {
        return status;
    }

    const struct mylite_select_column *column = &plan->table.columns[output->column_index];
    if (status == MYLITE_OK) {
        status =
            copy_result_metadata_text(database, &metadata->schema_name, plan->table.schema_name);
    }
    if (status == MYLITE_OK) {
        status = copy_result_metadata_text(database, &metadata->table_name, visible_table_name);
    }
    if (status == MYLITE_OK) {
        status = copy_result_metadata_text(database, &metadata->origin_schema_name,
                                           plan->table.schema_name);
    }
    if (status == MYLITE_OK) {
        status = copy_result_metadata_text(database, &metadata->origin_table_name,
                                           plan->table.table_name);
    }
    if (status == MYLITE_OK) {
        status = copy_result_metadata_text(database, &metadata->origin_column_name, column->name);
    }
    return status;
}

static int copy_result_metadata_text(mylite_db *database, char **out_text, const char *text)
{
    *out_text = NULL;
    if (text == NULL) {
        return MYLITE_OK;
    }

    *out_text = copy_span_text(text, strlen(text));
    if (*out_text == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static int copy_select_table_reference(const struct mylite_sql_ast_node *from_clause,
                                       struct mylite_select_table *table)
{
    const struct mylite_sql_ast_node *table_name = child_at(from_clause, 0U);
    const struct mylite_sql_ast_node *alias = child_at(from_clause, 1U);

    if (table_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        table->table_name = copy_identifier_span(table_name);
        if (table->table_name == NULL) {
            return MYLITE_NOMEM;
        }
    } else if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
               child_at(table_name, 0U) != NULL && child_at(table_name, 1U) != NULL &&
               child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
               child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        table->schema_name = copy_identifier_span(child_at(table_name, 0U));
        table->table_name = copy_identifier_span(child_at(table_name, 1U));
        if (table->schema_name == NULL || table->table_name == NULL) {
            return MYLITE_NOMEM;
        }
    } else {
        return MYLITE_UNSUPPORTED;
    }

    if (alias != NULL) {
        if (alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
            return MYLITE_UNSUPPORTED;
        }
        table->alias = copy_identifier_span(alias);
        if (table->alias == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int resolve_select_table_target(mylite_db *database, struct mylite_select_table *table)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = MYLITE_OK;

    if (table->schema_name == NULL) {
        if (database->selected_schema == NULL) {
            (void)set_error_message(database, "No database selected");
            return MYLITE_EXEC_ERROR;
        }
        table->schema_name =
            copy_span_text(database->selected_schema, strlen(database->selected_schema));
        if (table->schema_name == NULL) {
            (void)set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (select_schema_name_is_system(table->schema_name)) {
        return MYLITE_UNSUPPORTED;
    }

    status = schema_exists(database, table->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)set_error_message_parts(database, "Unknown database '", table->schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        return MYLITE_UNSUPPORTED;
    }

    status = table_exists(database, table->schema_name, table->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return set_table_doesnt_exist_error(database, table->schema_name, table->table_name);
    }

    table->physical_name = physical_table_name(table->schema_name, table->table_name);
    if (table->physical_name == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

static bool select_schema_name_is_system(const char *schema_name)
{
    if (ascii_case_equal(schema_name, "information_schema")) {
        return true;
    }
    if (ascii_case_equal(schema_name, "mysql")) {
        return true;
    }
    if (ascii_case_equal(schema_name, "performance_schema")) {
        return true;
    }
    if (ascii_case_equal(schema_name, "sys")) {
        return true;
    }
    return false;
}

static int load_select_columns(mylite_db *database, struct mylite_select_table *table)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] = "SELECT column_name, extra FROM __mylite_column_catalog "
                              "WHERE table_schema = ? AND table_name = ? ORDER BY ordinal_position";
    int rc =
        sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(select, 1, table->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table->table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = load_select_column_from_catalog_row(table, select);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            if (status == MYLITE_NOMEM) {
                (void)set_error_message(database, "out of memory");
            }
            return status;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    if (table->column_count == 0U) {
        return set_table_doesnt_exist_error(database, table->schema_name, table->table_name);
    }
    return MYLITE_OK;
}

static int load_select_column_from_catalog_row(struct mylite_select_table *table,
                                               sqlite3_stmt *select)
{
    const char *name = (const char *)sqlite3_column_text(select, 0);
    const char *extra = (const char *)sqlite3_column_text(select, 1);
    struct mylite_select_column column = {
        .visible = select_column_extra_is_visible(extra),
    };
    struct mylite_select_column *columns = NULL;

    column.name = copy_span_text(name, name == NULL ? 0U : strlen(name));
    if (column.name == NULL) {
        return MYLITE_NOMEM;
    }

    columns = realloc(table->columns, (table->column_count + 1U) * sizeof(*table->columns));
    if (columns == NULL) {
        select_column_deinit(&column);
        return MYLITE_NOMEM;
    }

    table->columns = columns;
    table->columns[table->column_count++] = column;
    return MYLITE_OK;
}

static bool select_column_extra_is_visible(const char *extra)
{
    if (extra == NULL) {
        return true;
    }
    if (strstr(extra, "INVISIBLE") == NULL) {
        return true;
    }
    return false;
}

static int build_select_outputs(mylite_db *database, const struct mylite_sql_ast_node *select_list,
                                bool allow_expression_outputs, struct mylite_select_plan *plan)
{
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        int status = append_select_item_outputs(database, item, allow_expression_outputs, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (plan->output_count == 0U) {
        return set_select_unsupported_projection_error(database);
    }
    return MYLITE_OK;
}

static int prepare_table_select_custom_statement(mylite_db *database,
                                                 const struct mylite_sql_ast_node *where_clause,
                                                 const char *sql, size_t sql_length,
                                                 struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt)
{
    sqlite3_stmt *sqlite_stmt = NULL;
    mylite_stmt *stmt = NULL;
    char *scan_sql = build_select_scan_sql(database, plan);
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (scan_sql == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(database->sqlite, scan_sql, -1, SQLITE_PREPARE_PERSISTENT, &sqlite_stmt,
                            NULL);
    sqlite3_free(scan_sql);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    stmt = calloc(1U, sizeof(*stmt));
    if (stmt == NULL) {
        sqlite3_finalize(sqlite_stmt);
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_TABLE_SELECT,
        .sqlite_stmt = sqlite_stmt,
        .affected_rows = -1,
    };

    status = attach_select_result_metadata(stmt, plan);
    if (status == MYLITE_OK) {
        stmt->select_plan = *plan;
        *plan = (struct mylite_select_plan){0};
        status = clone_table_select_expressions(stmt, where_clause, sql, sql_length);
    }
    if (status == MYLITE_OK) {
        *out_stmt = stmt;
        return MYLITE_OK;
    }

    mylite_finalize(stmt);
    return status;
}

static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *predicate = child_at(where_clause, 0U);

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        predicate == NULL) {
        return set_select_unsupported_where_error(database);
    }
    return bind_select_predicate_expression(database, predicate, plan);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_predicate_expression(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_select_plan *plan)
{
    if (expression == NULL) {
        return set_select_unsupported_where_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        size_t column_index = plan->table.column_count;
        int status = resolve_select_column_reference(&plan->table, expression, &column_index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (column_index == plan->table.column_count) {
            char *reference = copy_select_reference_name(expression);

            if (reference == NULL) {
                (void)set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = set_select_unknown_where_column_error(database, reference);
            free(reference);
            return status;
        }
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_select_predicate_expression(database, child, plan);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        return set_select_unsupported_where_error(database);
    }

    return set_select_unsupported_where_error(database);
}

static int bind_select_limit_clause(const struct mylite_sql_ast_node *limit_clause,
                                    struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *offset = child_at(limit_clause, 0U);
    const struct mylite_sql_ast_node *row_count = child_at(limit_clause, 1U);

    if (limit_clause == NULL || limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE ||
        offset == NULL || offset->kind != MYLITE_SQL_AST_LIMIT_BOUND ||
        !offset->has_limit_bound_value || row_count == NULL ||
        row_count->kind != MYLITE_SQL_AST_LIMIT_BOUND || !row_count->has_limit_bound_value) {
        return MYLITE_UNSUPPORTED;
    }

    plan->limit = (struct mylite_select_limit){
        .offset = offset->limit_bound_value,
        .row_count = row_count->limit_bound_value,
        .has_limit = true,
    };
    return MYLITE_OK;
}

static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *items = child_at(order_by_clause, 0U);

    if (order_by_clause == NULL || order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE ||
        items == NULL || items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return set_select_unsupported_order_error(database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        int status = bind_select_order_item(database, item, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (plan->order_key_count == 0U) {
        return set_select_unsupported_order_error(database);
    }
    return validate_select_expression_outputs(database, plan);
}

static int bind_select_order_item(mylite_db *database, const struct mylite_sql_ast_node *order_item,
                                  struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *expression = child_at(order_item, 0U);
    struct mylite_select_order_key order_key = {
        .kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION,
        .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
        .expression = expression,
    };

    if (order_item == NULL || order_item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
        return set_select_unsupported_order_error(database);
    }
    if (order_item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
        order_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        expression->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        uint64_t ordinal = 0U;

        if (!parse_uint64_span(expression->span, &ordinal) || ordinal == 0U ||
            ordinal > plan->output_count) {
            char *reference = copy_span_text(expression->span.text, expression->span.length);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)set_error_message(database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = set_select_unknown_order_column_error(database, reference);
            free(reference);
            return status;
        }
        order_key.kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
        order_key.output_index = (size_t)(ordinal - 1U);
        order_key.expression = NULL;
        mark_select_output_order_reference(plan, order_key.output_index);
        return add_select_order_key(plan, &order_key);
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status = resolve_select_order_reference(database, plan, expression, &kind, &index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            order_key.kind = kind;
            order_key.output_index = index;
            order_key.expression = NULL;
            mark_select_output_order_reference(plan, order_key.output_index);
            return add_select_order_key(plan, &order_key);
        }
    }

    {
        int status = bind_select_order_expression(database, expression, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return add_select_order_key(plan, &order_key);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_expression(mylite_db *database,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_select_plan *plan)
{
    if (expression == NULL) {
        return set_select_unsupported_order_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;
        int status = resolve_select_order_reference(database, plan, expression, &kind, &index);

        if (status == MYLITE_OK && kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            mark_select_output_order_reference(plan, index);
        }
        return status;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_select_order_expression(database, child, plan);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        return set_select_unsupported_order_error(database);
    }

    return set_select_unsupported_order_error(database);
}

static int validate_select_expression_outputs(mylite_db *database,
                                              const struct mylite_select_plan *plan)
{
    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];

        if (output->kind == MYLITE_SELECT_OUTPUT_EXPRESSION && !output->referenced_by_order) {
            return set_select_unsupported_projection_error(database);
        }
    }
    return MYLITE_OK;
}

static int append_select_item_outputs(mylite_db *database,
                                      const struct mylite_sql_ast_node *select_item,
                                      bool allow_expression_outputs,
                                      struct mylite_select_plan *plan)
{
    const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
    const struct mylite_sql_ast_node *alias = child_at(select_item, 1U);

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
        expression == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (expression->kind == MYLITE_SQL_AST_WILDCARD) {
        if (alias != NULL) {
            return set_select_unsupported_projection_error(database);
        }
        return append_select_wildcard_outputs(database, expression, plan);
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return append_select_column_output(database, expression, alias, plan);
    }
    if (allow_expression_outputs) {
        return append_select_expression_output(database, expression, alias, plan);
    }
    return set_select_unsupported_projection_error(database);
}

static int append_select_wildcard_outputs(mylite_db *database,
                                          const struct mylite_sql_ast_node *wildcard,
                                          struct mylite_select_plan *plan)
{
    bool matches = false;
    int status = resolve_select_wildcard(&plan->table, wildcard, &matches);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!matches) {
        char *qualifier = copy_select_wildcard_qualifier_name(wildcard);

        if (qualifier == NULL) {
            (void)set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        status = set_select_unknown_table_error(database, qualifier);
        free(qualifier);
        return status;
    }

    for (size_t index = 0U; index < plan->table.column_count; ++index) {
        char *label = NULL;
        struct mylite_select_output_column output = {0};

        if (!plan->table.columns[index].visible) {
            continue;
        }

        label = copy_span_text(plan->table.columns[index].name,
                               strlen(plan->table.columns[index].name));
        if (label == NULL) {
            (void)set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }

        output = (struct mylite_select_output_column){
            .kind = MYLITE_SELECT_OUTPUT_COLUMN,
            .column_index = index,
            .label = label,
        };
        status = add_select_output_column(plan, &output);
        if (status != MYLITE_OK) {
            free(label);
            if (status == MYLITE_NOMEM) {
                (void)set_error_message(database, "out of memory");
            }
            return status;
        }
    }
    return MYLITE_OK;
}

static int append_select_column_output(mylite_db *database,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_sql_ast_node *alias,
                                       struct mylite_select_plan *plan)
{
    size_t column_index = plan->table.column_count;
    char *label = NULL;
    int status = resolve_select_column_reference(&plan->table, expression, &column_index);

    if (status != MYLITE_OK) {
        return status;
    }
    if (column_index == plan->table.column_count) {
        char *reference = copy_select_reference_name(expression);

        if (reference == NULL) {
            (void)set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        status = set_select_unknown_column_error(database, reference);
        free(reference);
        return status;
    }

    label =
        alias == NULL ? copy_select_final_identifier_label(expression) : copy_select_alias(alias);
    if (label == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = add_select_output_column(plan, &(const struct mylite_select_output_column){
                                                .kind = MYLITE_SELECT_OUTPUT_COLUMN,
                                                .column_index = column_index,
                                                .label = label,
                                            });
    if (status != MYLITE_OK) {
        free(label);
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(database, "out of memory");
        }
        return status;
    }
    return MYLITE_OK;
}

static int append_select_expression_output(mylite_db *database,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_sql_ast_node *alias,
                                           struct mylite_select_plan *plan)
{
    char *label = NULL;
    int status = MYLITE_OK;

    if (expression == NULL || expression->kind == MYLITE_SQL_AST_WILDCARD) {
        return set_select_unsupported_projection_error(database);
    }
    status = bind_select_predicate_expression(database, expression, plan);
    if (status != MYLITE_OK) {
        return status;
    }

    label = alias == NULL ? copy_span_text(expression->span.text, expression->span.length)
                          : copy_select_alias(alias);
    if (label == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = add_select_output_column(plan, &(const struct mylite_select_output_column){
                                                .kind = MYLITE_SELECT_OUTPUT_EXPRESSION,
                                                .expression = expression,
                                                .label = label,
                                            });
    if (status != MYLITE_OK) {
        free(label);
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(database, "out of memory");
        }
        return status;
    }
    return MYLITE_OK;
}

static int add_select_output_column(struct mylite_select_plan *plan,
                                    const struct mylite_select_output_column *output)
{
    struct mylite_select_output_column *outputs =
        realloc(plan->outputs, (plan->output_count + 1U) * sizeof(*plan->outputs));

    if (outputs == NULL) {
        return MYLITE_NOMEM;
    }

    plan->outputs = outputs;
    plan->outputs[plan->output_count++] = *output;
    return MYLITE_OK;
}

static int add_select_order_key(struct mylite_select_plan *plan,
                                const struct mylite_select_order_key *order_key)
{
    struct mylite_select_order_key *order_keys =
        realloc(plan->order_keys, (plan->order_key_count + 1U) * sizeof(*plan->order_keys));

    if (order_keys == NULL) {
        return MYLITE_NOMEM;
    }

    plan->order_keys = order_keys;
    plan->order_keys[plan->order_key_count++] = *order_key;
    return MYLITE_OK;
}

static void mark_select_output_order_reference(struct mylite_select_plan *plan, size_t output_index)
{
    if (plan != NULL && output_index < plan->output_count) {
        plan->outputs[output_index].referenced_by_order = true;
    }
}

static int resolve_select_wildcard(const struct mylite_select_table *table,
                                   const struct mylite_sql_ast_node *wildcard, bool *out_matches)
{
    const struct mylite_sql_ast_node *first = child_at(wildcard, 0U);
    const struct mylite_sql_ast_node *second = child_at(wildcard, 1U);
    char *first_name = NULL;
    char *second_name = NULL;

    *out_matches = false;
    if (first == NULL) {
        *out_matches = true;
        return MYLITE_OK;
    }

    first_name = copy_identifier_span(first);
    if (first_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (second == NULL) {
        const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

        if (strcmp(first_name, visible_table) == 0) {
            *out_matches = true;
        }
        free(first_name);
        return MYLITE_OK;
    }

    second_name = copy_identifier_span(second);
    if (second_name == NULL) {
        free(first_name);
        return MYLITE_NOMEM;
    }
    if (table->alias == NULL && strcmp(first_name, table->schema_name) == 0 &&
        strcmp(second_name, table->table_name) == 0) {
        *out_matches = true;
    }
    free(first_name);
    free(second_name);
    return MYLITE_OK;
}

static int resolve_select_column_reference(const struct mylite_select_table *table,
                                           const struct mylite_sql_ast_node *expression,
                                           size_t *out_index)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = copy_select_identifier_parts(expression, parts, &part_count);

    *out_index = table->column_count;
    if (status != MYLITE_OK) {
        return status;
    }

    if (part_count >= 1U && part_count <= 3U &&
        select_reference_qualifiers_match(table, parts, part_count)) {
        *out_index = select_column_index(table, parts[part_count - 1U]);
    }

    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return MYLITE_OK;
}

static int resolve_select_order_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_order_key_kind *out_kind,
                                          size_t *out_index)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = copy_select_identifier_parts(expression, parts, &part_count);

    *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    *out_index = 0U;
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(database, "out of memory");
        }
        return status;
    }

    if (part_count == 1U) {
        size_t output_index = 0U;
        size_t output_matches = select_output_label_count(plan, parts[0], &output_index);

        if (output_matches > 1U) {
            status = set_select_ambiguous_order_column_error(database, parts[0]);
            goto cleanup;
        }
        if (output_matches == 1U) {
            *out_kind = MYLITE_SELECT_ORDER_KEY_OUTPUT;
            *out_index = output_index;
            goto cleanup;
        }
    }

    if (part_count >= 1U && part_count <= 3U &&
        select_reference_qualifiers_match(&plan->table, parts, part_count)) {
        size_t column_index = select_column_index(&plan->table, parts[part_count - 1U]);

        if (column_index != plan->table.column_count) {
            *out_kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
            *out_index = column_index;
            goto cleanup;
        }
    }

    {
        char *reference = copy_select_reference_name(expression);

        if (reference == NULL) {
            (void)set_error_message(database, "out of memory");
            status = MYLITE_NOMEM;
            goto cleanup;
        }
        status = set_select_unknown_order_column_error(database, reference);
        free(reference);
    }

cleanup:
    for (size_t index = 0U; index < part_count && index < 3U; ++index) {
        free(parts[index]);
    }
    return status;
}

static size_t select_output_label_count(const struct mylite_select_plan *plan, const char *label,
                                        size_t *out_index)
{
    size_t count = 0U;

    *out_index = plan->output_count;
    for (size_t index = 0U; index < plan->output_count; ++index) {
        if (plan->outputs[index].label != NULL &&
            ascii_case_equal(plan->outputs[index].label, label)) {
            if (count == 0U) {
                *out_index = index;
            }
            ++count;
        }
    }
    return count;
}

static bool select_reference_qualifiers_match(const struct mylite_select_table *table, char **parts,
                                              size_t part_count)
{
    if (part_count == 1U) {
        return true;
    }
    if (part_count == 2U) {
        const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

        if (strcmp(parts[0], visible_table) == 0) {
            return true;
        }
        return false;
    }
    if (part_count == 3U && table->alias == NULL) {
        if (strcmp(parts[0], table->schema_name) == 0 && strcmp(parts[1], table->table_name) == 0) {
            return true;
        }
        return false;
    }
    return false;
}

static size_t select_column_index(const struct mylite_select_table *table, const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static bool parse_uint64_span(struct mylite_sql_source_span span, uint64_t *out_value)
{
    enum { decimal_radix = 10U };
    uint64_t value = 0U;

    *out_value = 0U;
    if (span.text == NULL || span.length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        unsigned char byte = (unsigned char)span.text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return false;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + digit;
    }
    *out_value = value;
    return true;
}

static int copy_select_identifier_parts(const struct mylite_sql_ast_node *identifier, char **parts,
                                        size_t *part_count)
{
    const struct mylite_sql_ast_node *segments[3] = {0};
    const struct mylite_sql_ast_node *current = identifier;
    size_t segment_count = 0U;

    *part_count = 0U;
    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (segment_count >= 3U) {
            return MYLITE_UNSUPPORTED;
        }
        segments[segment_count++] = child_at(current, 1U);
        current = child_at(current, 0U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER || segment_count >= 3U) {
        return MYLITE_UNSUPPORTED;
    }
    segments[segment_count++] = current;

    for (size_t index = 0U; index < segment_count; ++index) {
        const struct mylite_sql_ast_node *segment = segments[segment_count - index - 1U];

        if (segment == NULL || segment->kind != MYLITE_SQL_AST_IDENTIFIER) {
            return MYLITE_UNSUPPORTED;
        }
        parts[index] = copy_identifier_span(segment);
        if (parts[index] == NULL) {
            for (size_t previous = 0U; previous < index; ++previous) {
                free(parts[previous]);
                parts[previous] = NULL;
            }
            *part_count = 0U;
            return MYLITE_NOMEM;
        }
        *part_count += 1U;
    }
    return MYLITE_OK;
}

static char *copy_select_alias(const struct mylite_sql_ast_node *alias)
{
    if (alias == NULL) {
        return NULL;
    }
    if (alias->kind == MYLITE_SQL_AST_LITERAL &&
        alias->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return copy_string_literal_span(alias);
    }
    if (alias->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_identifier_span(alias);
    }
    return NULL;
}

static char *copy_select_final_identifier_label(const struct mylite_sql_ast_node *identifier)
{
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = child_at(current, 1U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return NULL;
    }
    return copy_identifier_span(current);
}

static char *copy_select_reference_name(const struct mylite_sql_ast_node *identifier)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    size_t length = 0U;
    char *name = NULL;
    int status = copy_select_identifier_parts(identifier, parts, &part_count);

    if (status != MYLITE_OK) {
        for (size_t index = 0U; index < part_count; ++index) {
            free(parts[index]);
        }
        if (status == MYLITE_NOMEM) {
            return NULL;
        }
        return copy_span_text(identifier->span.text, identifier->span.length);
    }
    if (part_count == 0U) {
        return copy_span_text(identifier->span.text, identifier->span.length);
    }

    for (size_t index = 0U; index < part_count; ++index) {
        length += strlen(parts[index]);
        if (index != 0U) {
            length += 1U;
        }
    }

    name = malloc(length + 1U);
    if (name != NULL) {
        size_t offset = 0U;

        for (size_t index = 0U; index < part_count; ++index) {
            size_t part_length = strlen(parts[index]);

            if (index != 0U) {
                name[offset++] = '.';
            }
            memcpy(name + offset, parts[index], part_length);
            offset += part_length;
        }
        name[offset] = '\0';
    }

    for (size_t index = 0U; index < part_count; ++index) {
        free(parts[index]);
    }
    return name;
}

static char *copy_select_wildcard_qualifier_name(const struct mylite_sql_ast_node *wildcard)
{
    const struct mylite_sql_ast_node *first = child_at(wildcard, 0U);
    const struct mylite_sql_ast_node *second = child_at(wildcard, 1U);
    char *first_name = NULL;
    char *second_name = NULL;
    char *name = NULL;

    if (first == NULL) {
        return copy_span_text("*", 1U);
    }

    first_name = copy_identifier_span(first);
    if (first_name == NULL) {
        return NULL;
    }
    if (second == NULL) {
        return first_name;
    }

    second_name = copy_identifier_span(second);
    if (second_name == NULL) {
        free(first_name);
        return NULL;
    }

    name = malloc(strlen(first_name) + strlen(second_name) + 2U);
    if (name != NULL) {
        size_t first_length = strlen(first_name);
        size_t second_length = strlen(second_name);

        memcpy(name, first_name, first_length);
        name[first_length] = '.';
        memcpy(name + first_length + 1U, second_name, second_length);
        name[first_length + 1U + second_length] = '\0';
    }
    free(first_name);
    free(second_name);
    return name;
}

static int set_select_unknown_column_error(mylite_db *database, const char *column_name)
{
    int status =
        set_error_message_parts(database, "Unknown column '", column_name, "' in 'field list'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_unknown_where_column_error(mylite_db *database, const char *column_name)
{
    int status =
        set_error_message_parts(database, "Unknown column '", column_name, "' in 'where clause'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = append_database_warning(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                     mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_unknown_order_column_error(mylite_db *database, const char *column_name)
{
    int status =
        set_error_message_parts(database, "Unknown column '", column_name, "' in 'order clause'");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = append_database_warning(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                     mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_ambiguous_order_column_error(mylite_db *database, const char *column_name)
{
    int status = set_error_message_parts(database, "Column '", column_name,
                                         "' in order clause is ambiguous");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = append_database_warning(database, MYLITE_MYSQL_ER_NON_UNIQ_ERROR,
                                     mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_unknown_table_error(mylite_db *database, const char *table_name)
{
    int status = set_error_message_parts(database, "Unknown table '", table_name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_unsupported_projection_error(mylite_db *database)
{
    if (set_error_message(database, "Unsupported SELECT projection") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_where_error(mylite_db *database)
{
    if (set_error_message(database, "Unsupported WHERE predicate") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_order_error(mylite_db *database)
{
    if (set_error_message(database, "Unsupported ORDER BY expression") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static char *build_select_physical_sql(mylite_db *database, const struct mylite_select_plan *plan)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT ", (int)strlen("SELECT "));
    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_select_output_column *output = &plan->outputs[index];
        const struct mylite_select_column *column = &plan->table.columns[output->column_index];

        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" AS \"%w\"", column->name, output->label);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"", plan->table.physical_name);
    return sqlite3_str_finish(sql);
}

static char *build_select_scan_sql(mylite_db *database, const struct mylite_select_plan *plan)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT ", (int)strlen("SELECT "));
    for (size_t index = 0U; index < plan->table.column_count; ++index) {
        const struct mylite_select_column *column = &plan->table.columns[index];

        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", column->name);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"", plan->table.physical_name);
    return sqlite3_str_finish(sql);
}

static int clone_table_select_expressions(mylite_stmt *stmt,
                                          const struct mylite_sql_ast_node *where_clause,
                                          const char *sql, size_t sql_length)
{
    int status = MYLITE_OK;

    stmt->select_sql_text = copy_span_text(sql, sql_length);
    if (stmt->select_sql_text == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (where_clause != NULL) {
        const struct mylite_sql_ast_node *predicate = child_at(where_clause, 0U);
        struct mylite_sql_ast_node *clone = NULL;

        status = clone_table_select_expression_node(stmt, predicate, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_predicate = clone;
    }

    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;

        if (stmt->select_plan.outputs[index].kind != MYLITE_SELECT_OUTPUT_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.outputs[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.outputs[index].expression = clone;
    }

    for (size_t index = 0U; index < stmt->select_plan.order_key_count; ++index) {
        struct mylite_sql_ast_node *clone = NULL;

        if (stmt->select_plan.order_keys[index].kind != MYLITE_SELECT_ORDER_KEY_EXPRESSION) {
            continue;
        }
        status = clone_table_select_expression_node(
            stmt, stmt->select_plan.order_keys[index].expression, sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->select_plan.order_keys[index].expression = clone;
    }

    return MYLITE_OK;
}

static int clone_table_select_expression_node(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *expression,
                                              const char *source_sql, size_t sql_length,
                                              struct mylite_sql_ast_node **out_node)
{
    int status = clone_sql_ast_subtree(&stmt->select_predicate_ast, expression, source_sql,
                                       stmt->select_sql_text, sql_length, out_node);

    if (status == MYLITE_NOMEM) {
        (void)set_error_message(stmt->database, "out of memory");
    }
    return status;
}

static int clone_update_plan_nodes(mylite_stmt *stmt, const struct mylite_sql_ast_node *statement,
                                   const char *sql, size_t sql_length)
{
    const struct mylite_sql_ast_node *assignments = child_at(statement, 1U);
    size_t assignment_index = 0U;
    int status = MYLITE_OK;

    stmt->update_sql_text = copy_span_text(sql, sql_length);
    if (stmt->update_sql_text == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (const struct mylite_sql_ast_node *assignment =
             assignments == NULL ? NULL : assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling, ++assignment_index) {
        const struct mylite_sql_ast_node *clone = NULL;

        if (assignment_index >= stmt->update.assignment_count) {
            return MYLITE_UNSUPPORTED;
        }
        status = clone_update_ast_node(stmt, child_at(assignment, 1U), sql, sql_length, &clone);
        if (status != MYLITE_OK) {
            return status;
        }
        stmt->update.assignments[assignment_index].value = clone;
    }
    if (assignment_index != stmt->update.assignment_count) {
        return MYLITE_UNSUPPORTED;
    }

    status = clone_update_ast_node(stmt, find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE),
                                   sql, sql_length, &stmt->update.where_clause);
    if (status != MYLITE_OK) {
        return status;
    }
    status = clone_update_ast_node(stmt, find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE),
                                   sql, sql_length, &stmt->update.order_by_clause);
    if (status != MYLITE_OK) {
        return status;
    }
    return clone_update_ast_node(stmt,
                                 find_child_kind(statement, MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE),
                                 sql, sql_length, &stmt->update.limit_clause);
}

static int clone_update_ast_node(mylite_stmt *stmt, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, size_t sql_length,
                                 const struct mylite_sql_ast_node **out_node)
{
    struct mylite_sql_ast_node *clone = NULL;
    int status = clone_sql_ast_subtree(&stmt->update_ast, node, source_sql, stmt->update_sql_text,
                                       sql_length, &clone);

    if (status == MYLITE_NOMEM) {
        (void)set_error_message(stmt->database, "out of memory");
    }
    *out_node = clone;
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int clone_sql_ast_subtree(struct mylite_sql_ast *ast, const struct mylite_sql_ast_node *node,
                                 const char *source_sql, const char *sql_copy, size_t sql_length,
                                 struct mylite_sql_ast_node **out_node)
{
    struct mylite_sql_ast_node *clone = NULL;

    *out_node = NULL;
    if (node == NULL) {
        return MYLITE_OK;
    }

    clone = mylite_sql_ast_new_node(
        ast, node->kind, remap_source_span(node->span, source_sql, sql_copy, sql_length));
    if (clone == NULL) {
        return MYLITE_NOMEM;
    }

    {
        struct mylite_sql_ast_node *next_allocated = clone->next_allocated;

        *clone = *node;
        clone->first_child = NULL;
        clone->last_child = NULL;
        clone->next_sibling = NULL;
        clone->next_allocated = next_allocated;
        clone->span = remap_source_span(node->span, source_sql, sql_copy, sql_length);
        clone->column_character_set =
            remap_source_span(node->column_character_set, source_sql, sql_copy, sql_length);
        clone->column_collation =
            remap_source_span(node->column_collation, source_sql, sql_copy, sql_length);
    }

    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        struct mylite_sql_ast_node *child_clone = NULL;
        int status =
            clone_sql_ast_subtree(ast, child, source_sql, sql_copy, sql_length, &child_clone);

        if (status != MYLITE_OK) {
            return status;
        }
        mylite_sql_ast_node_append_child(clone, child_clone);
    }

    *out_node = clone;
    return MYLITE_OK;
}

static struct mylite_sql_source_span remap_source_span(struct mylite_sql_source_span span,
                                                       const char *source_sql, const char *sql_copy,
                                                       size_t sql_length)
{
    uintptr_t base = (uintptr_t)source_sql;
    uintptr_t end = base + sql_length;
    uintptr_t text = (uintptr_t)span.text;

    if (span.text == NULL || source_sql == NULL || sql_copy == NULL) {
        return span;
    }
    if (text < base || text > end || span.length > (size_t)(end - text)) {
        return span;
    }

    span.text = sql_copy + (text - base);
    return span;
}

static int prepare_sqlite_statement(mylite_db *database, const char *sqlite_sql,
                                    mylite_stmt **out_stmt)
{
    sqlite3_stmt *sqlite_stmt = NULL;
    mylite_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v3(database->sqlite, sqlite_sql, -1, SQLITE_PREPARE_PERSISTENT,
                                &sqlite_stmt, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    stmt = malloc(sizeof(*stmt));
    if (stmt == NULL) {
        sqlite3_finalize(sqlite_stmt);
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = MYLITE_STMT_SQLITE,
        .sqlite_stmt = sqlite_stmt,
        .affected_rows = -1,
    };
    *out_stmt = stmt;
    return MYLITE_OK;
}

static int prepare_custom_statement(mylite_db *database, enum mylite_stmt_kind kind,
                                    const struct mylite_sql_ast_node *statement,
                                    mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = kind,
        .affected_rows = 0,
    };

    switch (kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
        status = copy_statement_schema_name(statement, kind, &stmt->schema_name);
        if (status == MYLITE_OK) {
            status = copy_schema_options(statement, kind, &stmt->options);
        }
        break;
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
        status = copy_connection_charset_statement(statement, stmt);
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = copy_create_table_statement(statement, stmt);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = copy_drop_table_statement(statement, stmt);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = copy_insert_values_statement(statement, stmt);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = copy_insert_set_statement(statement, stmt);
        break;
    case MYLITE_STMT_UPDATE:
        status = MYLITE_UNSUPPORTED;
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        status = copy_scalar_select_statement(statement, stmt);
        break;
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_SQLITE:
        status = MYLITE_UNSUPPORTED;
        break;
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    if (kind == MYLITE_STMT_CREATE_SCHEMA &&
        find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_CREATE_TABLE &&
        find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_SCHEMA &&
        find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_TABLE &&
        find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

static int execute_custom_statement(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return execute_scalar_select_statement(stmt);
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT) {
        return execute_table_select_statement(stmt);
    }
    if (stmt->executed) {
        return MYLITE_DONE;
    }
    stmt->executed = true;

    switch (stmt->kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
        status = execute_create_schema_statement(stmt);
        break;
    case MYLITE_STMT_ALTER_SCHEMA:
        status = execute_alter_schema_statement(stmt);
        break;
    case MYLITE_STMT_DROP_SCHEMA:
        status = execute_drop_schema_statement(stmt);
        break;
    case MYLITE_STMT_USE_SCHEMA:
        status = execute_use_schema_statement(stmt);
        break;
    case MYLITE_STMT_SET_NAMES:
        status = execute_set_names_statement(stmt);
        break;
    case MYLITE_STMT_SET_CHARACTER_SET:
        status = execute_set_character_set_statement(stmt);
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = execute_create_table_statement(stmt);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = execute_drop_table_statement(stmt);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = execute_insert_values_statement(stmt);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = execute_insert_set_statement(stmt);
        break;
    case MYLITE_STMT_UPDATE:
        status = execute_update_statement(stmt);
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        return execute_scalar_select_statement(stmt);
    case MYLITE_STMT_TABLE_SELECT:
        return execute_table_select_statement(stmt);
    case MYLITE_STMT_SQLITE:
        status = MYLITE_MISUSE;
        break;
    }

    return status == MYLITE_OK ? MYLITE_DONE : status;
}

static int execute_create_schema_statement(mylite_stmt *stmt)
{
    struct mylite_schema_presence presence;
    int status = normalize_schema_options(stmt->database, &stmt->options);

    if (status != MYLITE_OK) {
        return status;
    }
    status = schema_exists(stmt->database, stmt->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.exists) {
        if (stmt->if_not_exists) {
            return MYLITE_OK;
        }
        (void)set_error_message_parts(stmt->database, "Can't create database '", stmt->schema_name,
                                      "'; database exists");
        return MYLITE_EXEC_ERROR;
    }

    return insert_schema(stmt->database, stmt->schema_name, &stmt->options);
}

static int execute_alter_schema_statement(mylite_stmt *stmt)
{
    const char *schema_name =
        stmt->schema_name == NULL ? stmt->database->selected_schema : stmt->schema_name;
    struct mylite_schema_presence presence;
    int status = normalize_schema_options(stmt->database, &stmt->options);

    if (status != MYLITE_OK) {
        return status;
    }
    if (schema_name == NULL) {
        (void)set_error_message(stmt->database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = schema_exists(stmt->database, schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)set_error_message_parts(stmt->database, "Database '", schema_name, "' doesn't exist");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)set_error_message_parts(stmt->database, "Access to system schema '", schema_name,
                                      "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    return update_schema(stmt->database, schema_name, &stmt->options);
}

static int execute_drop_schema_statement(mylite_stmt *stmt)
{
    struct mylite_schema_presence presence;
    int status = schema_exists(stmt->database, stmt->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        if (stmt->if_exists) {
            return MYLITE_OK;
        }
        (void)set_error_message_parts(stmt->database, "Can't drop database '", stmt->schema_name,
                                      "'; database doesn't exist");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)set_error_message_parts(stmt->database, "Access to system schema '",
                                      stmt->schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = delete_schema(stmt->database, stmt->schema_name);
    if (status == MYLITE_OK) {
        clear_selected_schema_if_matches(stmt->database, stmt->schema_name);
    }
    return status;
}

static int execute_use_schema_statement(mylite_stmt *stmt)
{
    struct mylite_schema_presence presence;
    int status = MYLITE_OK;

    if (span_contains_newline(stmt->schema_name, strlen(stmt->schema_name))) {
        (void)set_error_message(stmt->database, "USE database names must be single-line");
        return MYLITE_EXEC_ERROR;
    }

    status = schema_exists(stmt->database, stmt->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)set_error_message_parts(stmt->database, "Unknown database '", stmt->schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }

    return set_selected_schema(stmt->database, stmt->schema_name);
}

static int execute_set_names_statement(mylite_stmt *stmt)
{
    if (stmt->use_default_connection_charset) {
        return set_default_connection_state(stmt->database);
    }
    return set_names_connection_state(stmt->database,
                                      (struct mylite_connection_charset_request){
                                          .character_set_name = stmt->character_set_name,
                                          .collation_name = stmt->collation_name,
                                      });
}

static int execute_set_character_set_statement(mylite_stmt *stmt)
{
    if (stmt->use_default_connection_charset) {
        return set_character_set_connection_state(stmt->database, mylite_charset_default_name());
    }
    return set_character_set_connection_state(stmt->database, stmt->character_set_name);
}

static int execute_create_table_statement(mylite_stmt *stmt)
{
    const char *schema_name = stmt->create_table.schema_name == NULL
                                  ? stmt->database->selected_schema
                                  : stmt->create_table.schema_name;
    struct mylite_schema_default schema_default;
    int status = MYLITE_OK;

    if (schema_name == NULL) {
        (void)set_error_message(stmt->database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = validate_create_table_plan(stmt, schema_name, &schema_default);
    if (status != MYLITE_OK) {
        return status;
    }

    return create_table_transaction(stmt, schema_name, &schema_default);
}

static int execute_drop_table_statement(mylite_stmt *stmt)
{
    int status = validate_drop_table_plan(stmt);

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->drop_table.temporary) {
        if (stmt->if_exists) {
            return MYLITE_OK;
        }
        return set_unknown_table_error(stmt->database, stmt->drop_table.targets[0].schema_name,
                                       stmt->drop_table.targets[0].table_name);
    }

    return drop_table_transaction(stmt);
}

static int validate_create_table_plan(mylite_stmt *stmt, const char *schema_name,
                                      struct mylite_schema_default *schema_default)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = schema_exists(stmt->database, schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)set_error_message_parts(stmt->database, "Unknown database '", schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)set_error_message_parts(stmt->database, "Access to system schema '", schema_name,
                                      "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = table_exists(stmt->database, schema_name, stmt->create_table.table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (exists) {
        if (stmt->if_not_exists) {
            return MYLITE_DONE;
        }
        (void)set_error_message_parts(stmt->database, "Table '", stmt->create_table.table_name,
                                      "' already exists");
        return MYLITE_EXEC_ERROR;
    }

    status = schema_default_by_name(stmt->database, schema_name, schema_default);
    if (status != MYLITE_OK) {
        return status;
    }
    status = normalize_create_table_options(stmt->database, schema_name, schema_default,
                                            &stmt->create_table.options);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_column_names(stmt->database, &stmt->create_table)) {
        return MYLITE_EXEC_ERROR;
    }
    status = assign_generated_index_names(stmt->database, &stmt->create_table);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!validate_create_table_indexes(stmt->database, &stmt->create_table)) {
        return MYLITE_EXEC_ERROR;
    }
    apply_create_table_primary_key_nullability(&stmt->create_table);
    return MYLITE_OK;
}

static int create_table_transaction(mylite_stmt *stmt, const char *schema_name,
                                    const struct mylite_schema_default *schema_default)
{
    int status = begin_sqlite_transaction(stmt->database);

    if (status != MYLITE_OK) {
        return status;
    }

    status = create_physical_table(stmt, schema_name, schema_default);
    if (status == MYLITE_OK) {
        status = insert_table_catalog_row(stmt, schema_name, schema_default);
    }
    if (status == MYLITE_OK) {
        status = insert_column_catalog_rows(stmt, schema_name, schema_default);
    }
    if (status == MYLITE_OK) {
        status = insert_index_catalog_rows(stmt, schema_name);
    }
    if (status == MYLITE_OK) {
        status = commit_sqlite_transaction(stmt->database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    rollback_sqlite_transaction(stmt->database);
    return status;
}

static int create_physical_table(mylite_stmt *stmt, const char *schema_name,
                                 const struct mylite_schema_default *schema_default)
{
    char *physical_name = physical_table_name(schema_name, stmt->create_table.table_name);
    char *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = build_create_physical_table_sql(stmt, physical_name, schema_default);
    free(physical_name);
    if (sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(stmt->database->sqlite, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }
    return MYLITE_OK;
}

static int insert_table_catalog_row(mylite_stmt *stmt, const char *schema_name,
                                    const struct mylite_schema_default *schema_default)
{
    enum {
        bind_auto_increment = 4,
        bind_table_collation = 5,
        bind_table_comment = 6,
    };
    sqlite3_stmt *insert = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_table_catalog("
        "table_catalog, table_schema, table_name, table_type, engine, version, row_format, "
        "table_rows, avg_row_length, data_length, max_data_length, index_length, data_free, "
        "auto_increment, create_time, update_time, check_time, table_collation, checksum, "
        "create_options, table_comment)"
        " VALUES('def', ?, ?, 'BASE TABLE', ?, 10, NULL, 0, NULL, NULL, NULL, NULL, NULL, "
        "?, '1970-01-01 00:00:00', NULL, NULL, ?, NULL, '', ?)";
    const char *collation = stmt->create_table.options.collation == NULL
                                ? schema_default->collation
                                : stmt->create_table.options.collation;
    const char *comment =
        stmt->create_table.options.comment == NULL ? "" : stmt->create_table.options.comment;
    int rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert,
                                NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    sqlite3_bind_text(insert, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 2, stmt->create_table.table_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, 3, "InnoDB", -1, SQLITE_STATIC);
    if (stmt->create_table.options.has_auto_increment) {
        sqlite3_bind_int64(insert, bind_auto_increment,
                           (sqlite3_int64)stmt->create_table.options.auto_increment);
    } else {
        sqlite3_bind_null(insert, bind_auto_increment);
    }
    sqlite3_bind_text(insert, bind_table_collation, collation, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_comment, comment, -1, sqlite_transient_destructor());

    rc = sqlite3_step(insert);
    sqlite3_finalize(insert);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(stmt->database);
    }
    return MYLITE_OK;
}

static int insert_column_catalog_rows(mylite_stmt *stmt, const char *schema_name,
                                      const struct mylite_schema_default *schema_default)
{
    sqlite3_stmt *insert = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_column_catalog("
        "table_catalog, table_schema, table_name, column_name, ordinal_position, column_default, "
        "is_nullable, data_type, character_maximum_length, character_octet_length, "
        "numeric_precision, numeric_scale, datetime_precision, character_set_name, "
        "collation_name, column_type, column_key, extra, privileges, column_comment, "
        "generation_expression, srs_id)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "'select,insert,update,references', ?, '', NULL)";
    int rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert,
                                NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    for (size_t index = 0U; index < stmt->create_table.column_count; ++index) {
        int status = insert_column_catalog_row(stmt, insert, schema_name, schema_default,
                                               &stmt->create_table.columns[index], index);
        if (status != MYLITE_OK) {
            sqlite3_finalize(insert);
            return status;
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_column_catalog_row(mylite_stmt *stmt, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_schema_default *schema_default,
                                     const struct mylite_create_table_column *column,
                                     size_t column_index)
{
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_column_name = 3,
        bind_ordinal_position = 4,
        bind_column_default = 5,
        bind_is_nullable = 6,
        bind_data_type = 7,
        bind_character_maximum_length = 8,
        bind_character_octet_length = 9,
        bind_numeric_precision = 10,
        bind_numeric_scale = 11,
        bind_datetime_precision = 12,
        bind_character_set_name = 13,
        bind_collation_name = 14,
        bind_column_type = 15,
        bind_column_key = 16,
        bind_extra = 17,
        bind_column_comment = 18,
    };
    struct mylite_column_type_descriptor descriptor;
    const char *column_key = create_table_column_key(&stmt->create_table, column->name);
    const char *extra = create_table_column_extra(column);
    const char *is_nullable = "NO";
    const char *comment = "";
    int status = describe_create_table_column(column, schema_default, &stmt->create_table.options,
                                              &descriptor);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }
    if (column->nullable) {
        is_nullable = "YES";
    }
    if (column->comment != NULL) {
        comment = column->comment;
    }

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, stmt->create_table.table_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_column_name, column->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_ordinal_position, (sqlite3_int64)column_index + 1);
    if (column->default_text == NULL) {
        sqlite3_bind_null(insert, bind_column_default);
    } else {
        sqlite3_bind_text(insert, bind_column_default, column->default_text, -1,
                          sqlite_transient_destructor());
    }
    sqlite3_bind_text(insert, bind_is_nullable, is_nullable, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_data_type, descriptor.data_type, -1, SQLITE_STATIC);
    if (descriptor.is_character_string || descriptor.is_binary_string) {
        sqlite3_bind_int64(insert, bind_character_maximum_length,
                           (sqlite3_int64)descriptor.character_maximum_length);
        sqlite3_bind_int64(insert, bind_character_octet_length,
                           (sqlite3_int64)descriptor.character_octet_length);
    } else {
        sqlite3_bind_null(insert, bind_character_maximum_length);
        sqlite3_bind_null(insert, bind_character_octet_length);
    }
    if (descriptor.numeric_precision != 0U) {
        sqlite3_bind_int(insert, bind_numeric_precision, (int)descriptor.numeric_precision);
    } else {
        sqlite3_bind_null(insert, bind_numeric_precision);
    }
    if (descriptor.has_numeric_scale) {
        sqlite3_bind_int(insert, bind_numeric_scale, (int)descriptor.numeric_scale);
    } else {
        sqlite3_bind_null(insert, bind_numeric_scale);
    }
    if (descriptor.has_datetime_precision) {
        sqlite3_bind_int(insert, bind_datetime_precision, (int)descriptor.datetime_precision);
    } else {
        sqlite3_bind_null(insert, bind_datetime_precision);
    }
    if (descriptor.character_set_name == NULL) {
        sqlite3_bind_null(insert, bind_character_set_name);
    } else {
        sqlite3_bind_text(insert, bind_character_set_name, descriptor.character_set_name, -1,
                          SQLITE_STATIC);
    }
    if (descriptor.collation_name == NULL) {
        sqlite3_bind_null(insert, bind_collation_name);
    } else {
        sqlite3_bind_text(insert, bind_collation_name, descriptor.collation_name, -1,
                          SQLITE_STATIC);
    }
    sqlite3_bind_text(insert, bind_column_type, descriptor.column_type, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_column_key, column_key, -1, SQLITE_STATIC);
    if (extra == NULL || extra[0] == '\0') {
        sqlite3_bind_text(insert, bind_extra, "", -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(insert, bind_extra, extra, -1, SQLITE_STATIC);
    }
    sqlite3_bind_text(insert, bind_column_comment, comment, -1, sqlite_transient_destructor());

    rc = sqlite3_step(insert);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(stmt->database);
    }
    return MYLITE_OK;
}

static int insert_index_catalog_rows(mylite_stmt *stmt, const char *schema_name)
{
    sqlite3_stmt *insert = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_index_catalog("
        "table_catalog, table_schema, table_name, non_unique, index_schema, index_name, "
        "seq_in_index, column_name, collation, cardinality, sub_part, packed, nullable, "
        "index_type, comment, index_comment, is_visible, expression)"
        " VALUES('def', ?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, NULL, ?, ?, '', ?, ?, NULL)";
    int rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &insert,
                                NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    for (size_t index_index = 0U; index_index < stmt->create_table.index_count; ++index_index) {
        const struct mylite_create_table_index *index = &stmt->create_table.indexes[index_index];

        for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
            int status = insert_index_catalog_part(stmt, insert, schema_name, index,
                                                   &index->parts[part_index], part_index);
            if (status != MYLITE_OK) {
                sqlite3_finalize(insert);
                return status;
            }
        }
    }

    sqlite3_finalize(insert);
    return MYLITE_OK;
}

static int insert_index_catalog_part(mylite_stmt *stmt, sqlite3_stmt *insert,
                                     const char *schema_name,
                                     const struct mylite_create_table_index *index,
                                     const struct mylite_create_table_key_part *part,
                                     size_t part_index)
{
    enum {
        bind_table_schema = 1,
        bind_table_name = 2,
        bind_non_unique = 3,
        bind_index_schema = 4,
        bind_index_name = 5,
        bind_seq_in_index = 6,
        bind_column_name = 7,
        bind_collation = 8,
        bind_sub_part = 9,
        bind_nullable = 10,
        bind_index_type = 11,
        bind_index_comment = 12,
        bind_is_visible = 13,
    };
    const struct mylite_create_table_column *column =
        find_create_table_column(&stmt->create_table, part->column_name);
    int non_unique = 1;
    const char *nullable = "";
    const char *index_type = "BTREE";
    const char *is_visible = "NO";
    int rc = SQLITE_OK;

    if (index->is_unique) {
        non_unique = 0;
    }
    if (column != NULL && column->nullable) {
        nullable = "YES";
    }
    if (index->algorithm == MYLITE_SQL_AST_INDEX_ALGORITHM_HASH) {
        index_type = "HASH";
    }
    if (index->is_visible) {
        is_visible = "YES";
    }

    sqlite3_reset(insert);
    sqlite3_clear_bindings(insert);
    sqlite3_bind_text(insert, bind_table_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_table_name, stmt->create_table.table_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_int(insert, bind_non_unique, non_unique);
    sqlite3_bind_text(insert, bind_index_schema, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_index_name, index->name, -1, sqlite_transient_destructor());
    sqlite3_bind_int64(insert, bind_seq_in_index, (sqlite3_int64)part_index + 1);
    sqlite3_bind_text(insert, bind_column_name, part->column_name, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_collation, index_collation_for_order(part->order), -1,
                      SQLITE_STATIC);
    if (part->has_prefix_length) {
        sqlite3_bind_int64(insert, bind_sub_part, (sqlite3_int64)part->prefix_length);
    } else {
        sqlite3_bind_null(insert, bind_sub_part);
    }
    sqlite3_bind_text(insert, bind_nullable, nullable, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_index_type, index_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(insert, bind_index_comment, index->comment == NULL ? "" : index->comment, -1,
                      sqlite_transient_destructor());
    sqlite3_bind_text(insert, bind_is_visible, is_visible, -1, SQLITE_STATIC);

    rc = sqlite3_step(insert);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(stmt->database);
    }
    return MYLITE_OK;
}

static int validate_drop_table_plan(mylite_stmt *stmt)
{
    for (size_t index = 0U; index < stmt->drop_table.target_count; ++index) {
        struct mylite_drop_table_target *target = &stmt->drop_table.targets[index];

        if (target->schema_name == NULL) {
            if (stmt->database->selected_schema == NULL) {
                (void)set_error_message(stmt->database, "No database selected");
                return MYLITE_EXEC_ERROR;
            }
            target->schema_name = copy_span_text(stmt->database->selected_schema,
                                                 strlen(stmt->database->selected_schema));
            if (target->schema_name == NULL) {
                (void)set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
        }

        if (drop_table_target_is_duplicate(&stmt->drop_table, index)) {
            (void)set_error_message_parts(stmt->database, "Not unique table/alias: '",
                                          target->table_name, "'");
            return MYLITE_EXEC_ERROR;
        }
    }

    if (stmt->drop_table.temporary) {
        for (size_t index = 0U; index < stmt->drop_table.target_count; ++index) {
            int status =
                validate_drop_table_temporary_target(stmt, &stmt->drop_table.targets[index]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < stmt->drop_table.target_count; ++index) {
        struct mylite_drop_table_target *target = &stmt->drop_table.targets[index];
        int status = validate_drop_table_target(stmt, target);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int validate_drop_table_temporary_target(mylite_stmt *stmt,
                                                const struct mylite_drop_table_target *target)
{
    struct mylite_schema_presence presence;
    int status = schema_exists(stmt->database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        (void)set_error_message_parts(stmt->database, "Access to system schema '",
                                      target->schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int validate_drop_table_target(mylite_stmt *stmt, struct mylite_drop_table_target *target)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = schema_exists(stmt->database, target->schema_name, &presence);

    if (status != MYLITE_OK) {
        return status;
    }
    if (presence.is_system) {
        (void)set_error_message_parts(stmt->database, "Access to system schema '",
                                      target->schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }
    if (!presence.exists) {
        if (stmt->if_exists) {
            return MYLITE_OK;
        }
        return set_unknown_table_error(stmt->database, target->schema_name, target->table_name);
    }

    status = table_exists(stmt->database, target->schema_name, target->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists && !stmt->if_exists) {
        return set_unknown_table_error(stmt->database, target->schema_name, target->table_name);
    }
    target->exists = exists;
    return MYLITE_OK;
}

static bool drop_table_target_is_duplicate(const struct mylite_drop_table_plan *plan,
                                           size_t target_index)
{
    const struct mylite_drop_table_target *target = &plan->targets[target_index];

    for (size_t index = 0U; index < target_index; ++index) {
        if (strcmp(plan->targets[index].schema_name, target->schema_name) == 0 &&
            strcmp(plan->targets[index].table_name, target->table_name) == 0) {
            return true;
        }
    }
    return false;
}

static int drop_table_transaction(mylite_stmt *stmt)
{
    int status = begin_sqlite_transaction(stmt->database);

    if (status != MYLITE_OK) {
        return status;
    }

    for (size_t index = 0U; index < stmt->drop_table.target_count && status == MYLITE_OK; ++index) {
        if (!stmt->drop_table.targets[index].exists) {
            continue;
        }
        status = drop_physical_table(stmt, &stmt->drop_table.targets[index]);
        if (status == MYLITE_OK) {
            status = delete_table_catalog_rows(stmt, &stmt->drop_table.targets[index]);
        }
    }

    if (status == MYLITE_OK) {
        status = commit_sqlite_transaction(stmt->database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    rollback_sqlite_transaction(stmt->database);
    return status;
}

static int drop_physical_table(mylite_stmt *stmt, const struct mylite_drop_table_target *target)
{
    char *physical_name = physical_table_name(target->schema_name, target->table_name);
    char *drop_sql = NULL;
    sqlite3_str *sql = NULL;
    int rc = SQLITE_OK;

    if (physical_name == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    sql = sqlite3_str_new(stmt->database->sqlite);
    if (sql == NULL) {
        free(physical_name);
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    sqlite3_str_appendf(sql, "DROP TABLE \"%w\"", physical_name);
    free(physical_name);

    drop_sql = sqlite3_str_finish(sql);
    if (drop_sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_exec(stmt->database->sqlite, drop_sql, NULL, NULL, NULL);
    sqlite3_free(drop_sql);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }
    return MYLITE_OK;
}

static int delete_table_catalog_rows(mylite_stmt *stmt,
                                     const struct mylite_drop_table_target *target)
{
    static const char delete_indexes[] =
        "DELETE FROM __mylite_index_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_columns[] =
        "DELETE FROM __mylite_column_catalog WHERE table_schema = ? AND table_name = ?";
    static const char delete_tables[] =
        "DELETE FROM __mylite_table_catalog WHERE table_schema = ? AND table_name = ?";
    int status = delete_table_catalog_row(stmt->database, delete_indexes, target);

    if (status == MYLITE_OK) {
        status = delete_table_catalog_row(stmt->database, delete_columns, target);
    }
    if (status == MYLITE_OK) {
        status = delete_table_catalog_row(stmt->database, delete_tables, target);
    }
    return status;
}

static int delete_table_catalog_row(mylite_db *database, const char *sql,
                                    const struct mylite_drop_table_target *target)
{
    sqlite3_stmt *delete_stmt = NULL;
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &delete_stmt,
                                NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(delete_stmt, 1, target->schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(delete_stmt, 2, target->table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(delete_stmt);
    sqlite3_finalize(delete_stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int execute_insert_values_statement(mylite_stmt *stmt)
{
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    size_t *column_indexes = NULL;
    int status = validate_insert_values_target(stmt, &schema_name);

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = load_insert_table(stmt, schema_name, &table);
    if (status == MYLITE_OK) {
        status = validate_insert_column_list(stmt, &table, &column_indexes);
    }
    if (status == MYLITE_OK) {
        status = execute_insert_values_transaction(stmt, schema_name, &table, column_indexes);
    }

    free(column_indexes);
    insert_table_deinit(&table);
    return status;
}

static int execute_insert_set_statement(mylite_stmt *stmt)
{
    const char *schema_name = NULL;
    struct mylite_insert_table table = {0};
    size_t *column_indexes = NULL;
    int status = validate_insert_values_target(stmt, &schema_name);

    stmt->affected_rows = 0;
    if (status != MYLITE_OK) {
        return status;
    }

    status = load_insert_table(stmt, schema_name, &table);
    if (status == MYLITE_OK) {
        status = validate_insert_set_assignments(stmt, &table, schema_name, &column_indexes);
    }
    if (status == MYLITE_OK) {
        status = execute_insert_set_transaction(stmt, schema_name, &table, column_indexes);
    }

    free(column_indexes);
    insert_table_deinit(&table);
    return status;
}

static int execute_update_statement(mylite_stmt *stmt)
{
    struct mylite_select_table table = {0};
    struct mylite_insert_table write_table = {0};
    struct mylite_update_order_plan order_plan = {0};
    struct mylite_update_bound_assignment *assignments = NULL;
    struct mylite_update_rowset rowset = {0};
    int status = MYLITE_OK;

    stmt->affected_rows = 0;
    stmt->matched_rows = 0U;

    status = copy_update_target_to_select_table(stmt, &table);
    if (status == MYLITE_OK) {
        status = resolve_select_table_target(stmt->database, &table);
    }
    if (status == MYLITE_OK) {
        status = load_select_columns(stmt->database, &table);
    }
    if (status == MYLITE_OK) {
        status = load_write_table(stmt, table.schema_name, table.table_name, &write_table);
    }
    if (status == MYLITE_OK) {
        status = bind_update_subset(stmt, &table, &assignments);
    }
    if (status == MYLITE_OK) {
        status = bind_update_order_by_clause(stmt, &table, &order_plan);
    }
    if (status == MYLITE_OK) {
        status = materialize_update_rows(stmt, &table, &order_plan, &rowset);
    }
    if (status == MYLITE_OK) {
        status = sort_update_rowset(&rowset, &order_plan);
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(stmt->database, "out of memory");
        }
    }
    if (status == MYLITE_OK) {
        apply_update_limit(stmt->update.limit_clause, &rowset);
        stmt->matched_rows = rowset.row_count;
        status = execute_update_rows_transaction(stmt, &table, &write_table, assignments,
                                                 stmt->update.assignment_count, &rowset);
    }

    free(assignments);
    update_rowset_deinit(&rowset);
    update_order_plan_deinit(&order_plan);
    insert_table_deinit(&write_table);
    select_table_deinit(&table);
    if (status != MYLITE_OK) {
        stmt->affected_rows = -1;
    }
    return status;
}

static int copy_update_target_to_select_table(mylite_stmt *stmt, struct mylite_select_table *table)
{
    const struct mylite_update_target *target = &stmt->update.target;

    if (target->schema_name != NULL) {
        table->schema_name = copy_span_text(target->schema_name, strlen(target->schema_name));
        if (table->schema_name == NULL) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    table->table_name = copy_span_text(target->table_name, strlen(target->table_name));
    if (table->table_name == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    if (target->alias != NULL) {
        table->alias = copy_span_text(target->alias, strlen(target->alias));
        if (table->alias == NULL) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int bind_update_subset(mylite_stmt *stmt, const struct mylite_select_table *table,
                              struct mylite_update_bound_assignment **out_assignments)
{
    size_t assignment_count = stmt->update.assignment_count;
    struct mylite_update_bound_assignment *assignments = NULL;
    int status = reject_deferred_update_clauses(stmt);

    *out_assignments = NULL;
    if (status != MYLITE_OK) {
        return status;
    }
    if (assignment_count == 0U) {
        return set_update_unsupported_assignment_error(stmt->database);
    }

    assignments = calloc(assignment_count, sizeof(*assignments));
    if (assignments == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = bind_update_assignment_targets(stmt, table, assignments, assignment_count);
    if (status == MYLITE_OK) {
        status = bind_update_assignment_values(stmt, table, assignments, assignment_count);
    }
    if (status == MYLITE_OK) {
        status = bind_update_where_clause(stmt, table);
    }
    if (status != MYLITE_OK) {
        free(assignments);
        return status;
    }

    *out_assignments = assignments;
    return MYLITE_OK;
}

static int reject_deferred_update_clauses(mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *limit = stmt->update.limit_clause;

    if (limit == NULL) {
        return MYLITE_OK;
    }
    if (limit->kind != MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE || child_at(limit, 0U) == NULL ||
        child_at(limit, 0U)->kind != MYLITE_SQL_AST_LIMIT_BOUND ||
        !child_at(limit, 0U)->has_limit_bound_value) {
        return set_update_unsupported_clause_error(stmt->database);
    }
    return MYLITE_OK;
}

static int bind_update_assignment_targets(mylite_stmt *stmt,
                                          const struct mylite_select_table *table,
                                          struct mylite_update_bound_assignment *assignments,
                                          size_t assignment_count)
{
    for (size_t index = 0U; index < assignment_count; ++index) {
        const struct mylite_update_assignment *assignment = &stmt->update.assignments[index];
        size_t column_index = update_column_reference_index(table, &assignment->target);

        if (column_index == table->column_count) {
            char *reference = copy_update_column_reference_name(&assignment->target);
            int status = MYLITE_OK;

            if (reference == NULL) {
                (void)set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = set_update_unknown_field_error(stmt->database, reference);
            free(reference);
            return status;
        }
        assignments[index] = (struct mylite_update_bound_assignment){
            .column_index = column_index,
            .value = assignment->value,
        };
    }
    return MYLITE_OK;
}

static int bind_update_assignment_values(mylite_stmt *stmt, const struct mylite_select_table *table,
                                         struct mylite_update_bound_assignment *assignments,
                                         size_t assignment_count)
{
    for (size_t index = 0U; index < assignment_count; ++index) {
        int status = bind_update_assignment_expression(stmt, table, assignments[index].value);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_update_assignment_expression(mylite_stmt *stmt,
                                             const struct mylite_select_table *table,
                                             const struct mylite_sql_ast_node *expression)
{
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_DEFAULT) {
        return MYLITE_OK;
    }
    return bind_update_predicate_expression(stmt, table, expression, "field list");
}

static int bind_update_where_clause(mylite_stmt *stmt, const struct mylite_select_table *table)
{
    const struct mylite_sql_ast_node *predicate = child_at(stmt->update.where_clause, 0U);

    if (stmt->update.where_clause == NULL) {
        return MYLITE_OK;
    }
    if (stmt->update.where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE || predicate == NULL) {
        return set_update_unsupported_clause_error(stmt->database);
    }
    return bind_update_predicate_expression(stmt, table, predicate, "where clause");
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_update_predicate_expression(mylite_stmt *stmt,
                                            const struct mylite_select_table *table,
                                            const struct mylite_sql_ast_node *expression,
                                            const char *clause_context)
{
    if (expression == NULL) {
        return set_update_unsupported_clause_error(stmt->database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER: {
        size_t column_index = table->column_count;
        int status = resolve_select_column_reference(table, expression, &column_index);

        if (status != MYLITE_OK) {
            return status;
        }
        if (column_index == table->column_count) {
            char *reference = copy_select_reference_name(expression);

            if (reference == NULL) {
                (void)set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
            status = set_update_unknown_column_error(stmt->database, reference, clause_context);
            free(reference);
            return status;
        }
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = bind_update_predicate_expression(stmt, table, child, clause_context);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        return set_update_unsupported_expression_error(stmt->database, clause_context);
    }

    return set_update_unsupported_expression_error(stmt->database, clause_context);
}

static int bind_update_order_by_clause(mylite_stmt *stmt, const struct mylite_select_table *table,
                                       struct mylite_update_order_plan *order_plan)
{
    const struct mylite_sql_ast_node *items = child_at(stmt->update.order_by_clause, 0U);

    if (stmt->update.order_by_clause == NULL) {
        return MYLITE_OK;
    }
    if (stmt->update.order_by_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE || items == NULL ||
        items->kind != MYLITE_SQL_AST_ORDER_ITEM_LIST) {
        return set_update_unsupported_clause_error(stmt->database);
    }

    for (const struct mylite_sql_ast_node *item = items->first_child; item != NULL;
         item = item->next_sibling) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        struct mylite_select_order_key order_key = {
            .kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION,
            .direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC,
            .expression = expression,
        };
        int status = MYLITE_OK;

        if (item->kind != MYLITE_SQL_AST_ORDER_ITEM || expression == NULL) {
            return set_update_unsupported_clause_error(stmt->database);
        }
        if (item->key_part_order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
            order_key.direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
        }
        status = bind_update_order_expression(stmt, table, expression);
        if (status == MYLITE_OK) {
            status = add_update_order_key(order_plan, &order_key);
            if (status == MYLITE_NOMEM) {
                (void)set_error_message(stmt->database, "out of memory");
            }
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return order_plan->order_key_count == 0U ? set_update_unsupported_clause_error(stmt->database)
                                             : MYLITE_OK;
}

static int bind_update_order_expression(mylite_stmt *stmt, const struct mylite_select_table *table,
                                        const struct mylite_sql_ast_node *expression)
{
    return bind_update_predicate_expression(stmt, table, expression, "order clause");
}

static int add_update_order_key(struct mylite_update_order_plan *plan,
                                const struct mylite_select_order_key *order_key)
{
    struct mylite_select_order_key *keys =
        realloc(plan->order_keys, (plan->order_key_count + 1U) * sizeof(*plan->order_keys));

    if (keys == NULL) {
        return MYLITE_NOMEM;
    }
    plan->order_keys = keys;
    plan->order_keys[plan->order_key_count++] = *order_key;
    return MYLITE_OK;
}

static int materialize_update_rows(mylite_stmt *stmt, const struct mylite_select_table *table,
                                   const struct mylite_update_order_plan *order_plan,
                                   struct mylite_update_rowset *rowset)
{
    sqlite3_stmt *scan = NULL;
    char *scan_sql = build_update_scan_sql(stmt->database, table);
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    if (scan_sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, scan_sql, -1, SQLITE_PREPARE_PERSISTENT, &scan,
                            NULL);
    sqlite3_free(scan_sql);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    while ((rc = sqlite3_step(scan)) == SQLITE_ROW) {
        struct mylite_update_row row = {0};
        bool matches = false;

        status = copy_update_sqlite_row(stmt, table, scan, &row);
        if (status == MYLITE_OK) {
            status = evaluate_update_row_matches(stmt, table, &row, &matches);
        }
        if (status == MYLITE_OK && matches) {
            status = evaluate_update_order_values(stmt, table, order_plan, &row);
        }
        if (status == MYLITE_OK && matches) {
            status = append_update_row(stmt, rowset, &row);
        }
        update_row_deinit(&row);
        if (status != MYLITE_OK) {
            sqlite3_finalize(scan);
            return status;
        }
    }
    sqlite3_finalize(scan);
    return rc == SQLITE_DONE ? MYLITE_OK : set_sqlite_error(stmt->database);
}

static char *build_update_scan_sql(mylite_db *database, const struct mylite_select_table *table)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_append(sql, "SELECT rowid", (int)strlen("SELECT rowid"));
    for (size_t index = 0U; index < table->column_count; ++index) {
        sqlite3_str_appendf(sql, ",\"%w\"", table->columns[index].name);
    }
    sqlite3_str_appendf(sql, " FROM \"%w\"", table->physical_name);
    return sqlite3_str_finish(sql);
}

static int copy_update_sqlite_row(mylite_stmt *stmt, const struct mylite_select_table *table,
                                  sqlite3_stmt *scan, struct mylite_update_row *out_row)
{
    out_row->rowid = sqlite3_column_int64(scan, 0);
    out_row->values = calloc(table->column_count, sizeof(*out_row->values));
    if (out_row->values == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->value_count = table->column_count;

    for (size_t index = 0U; index < table->column_count; ++index) {
        if (copy_update_sqlite_column_value(scan, (int)index + 1, &out_row->values[index]) != 0) {
            update_row_deinit(out_row);
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_update_sqlite_column_value(sqlite3_stmt *scan, int column,
                                           struct mylite_expression_value *out_value)
{
    int sqlite_type = sqlite3_column_type(scan, column);

    switch (sqlite_type) {
    case SQLITE_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    case SQLITE_INTEGER:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = sqlite3_column_int64(scan, column),
        };
        return 0;
    case SQLITE_FLOAT:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = sqlite3_column_double(scan, column),
        };
        return 0;
    case SQLITE_TEXT:
    case SQLITE_BLOB: {
        const unsigned char *text = sqlite3_column_text(scan, column);
        int bytes = sqlite3_column_bytes(scan, column);

        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = copy_span_text((const char *)text, bytes < 0 ? 0U : (size_t)bytes);
        return out_value->text_value == NULL ? -1 : 0;
    }
    default:
        break;
    }
    return -1;
}

static int evaluate_update_row_matches(mylite_stmt *stmt, const struct mylite_select_table *table,
                                       const struct mylite_update_row *row, bool *out_matches)
{
    struct mylite_update_expression_context user_context = {
        .table = table,
        .row = row,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = resolve_update_expression_identifier,
    };
    struct mylite_expression_value value = {0};
    int truth = -1;
    int status = 0;

    *out_matches = true;
    if (stmt->update.where_clause == NULL) {
        return MYLITE_OK;
    }

    status = mylite_expression_eval_with_context(child_at(stmt->update.where_clause, 0U), &context,
                                                 &stmt->database->warnings, &value);
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return set_where_predicate_eval_error(stmt);
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

static int evaluate_update_order_values(mylite_stmt *stmt, const struct mylite_select_table *table,
                                        const struct mylite_update_order_plan *order_plan,
                                        struct mylite_update_row *row)
{
    if (order_plan->order_key_count == 0U) {
        return MYLITE_OK;
    }

    row->order_values = calloc(order_plan->order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = order_plan->order_key_count;

    for (size_t index = 0U; index < order_plan->order_key_count; ++index) {
        int status = evaluate_update_order_key(stmt, table, row, &order_plan->order_keys[index],
                                               &row->order_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_update_order_key(mylite_stmt *stmt, const struct mylite_select_table *table,
                                     const struct mylite_update_row *row,
                                     const struct mylite_select_order_key *order_key,
                                     struct mylite_expression_value *out_value)
{
    struct mylite_update_expression_context user_context = {
        .table = table,
        .row = row,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = resolve_update_expression_identifier,
    };
    int status = mylite_expression_eval_with_context(order_key->expression, &context,
                                                     &stmt->database->warnings, out_value);

    return status == 0 ? MYLITE_OK : set_update_unsupported_clause_error(stmt->database);
}

static int append_update_row(mylite_stmt *stmt, struct mylite_update_rowset *rowset,
                             struct mylite_update_row *row)
{
    struct mylite_update_row *rows =
        realloc(rowset->rows, (rowset->row_count + 1U) * sizeof(*rowset->rows));

    if (rows == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rowset->rows = rows;
    rowset->rows[rowset->row_count++] = *row;
    *row = (struct mylite_update_row){0};
    return MYLITE_OK;
}

static int sort_update_rowset(struct mylite_update_rowset *rowset,
                              const struct mylite_update_order_plan *order_plan)
{
    struct mylite_update_row *scratch = NULL;
    int status = MYLITE_OK;

    if (order_plan->order_key_count == 0U || rowset->row_count < 2U) {
        return MYLITE_OK;
    }

    scratch = calloc(rowset->row_count, sizeof(*scratch));
    if (scratch == NULL) {
        return MYLITE_NOMEM;
    }
    status = merge_sort_update_rows(rowset->rows, scratch, 0U, rowset->row_count, order_plan);
    free(scratch);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int merge_sort_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                                  size_t first, size_t last,
                                  const struct mylite_update_order_plan *order_plan)
{
    size_t count = last - first;
    size_t middle = first + (count / 2U);
    int status = MYLITE_OK;

    if (count < 2U) {
        return MYLITE_OK;
    }

    status = merge_sort_update_rows(rows, scratch, first, middle, order_plan);
    if (status == MYLITE_OK) {
        status = merge_sort_update_rows(rows, scratch, middle, last, order_plan);
    }
    if (status == MYLITE_OK) {
        merge_update_rows(rows, scratch, first, middle, last, order_plan);
    }
    return status;
}

static void merge_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                              size_t first, size_t middle, size_t last,
                              const struct mylite_update_order_plan *order_plan)
{
    size_t left = first;
    size_t right = middle;
    size_t output = first;

    while (left < middle && right < last) {
        if (compare_update_rows(&rows[left], &rows[right], order_plan) <= 0) {
            scratch[output++] = rows[left++];
        } else {
            scratch[output++] = rows[right++];
        }
    }
    while (left < middle) {
        scratch[output++] = rows[left++];
    }
    while (right < last) {
        scratch[output++] = rows[right++];
    }
    for (size_t index = first; index < last; ++index) {
        rows[index] = scratch[index];
    }
}

static int compare_update_rows(const struct mylite_update_row *left,
                               const struct mylite_update_row *right,
                               const struct mylite_update_order_plan *order_plan)
{
    for (size_t index = 0U; index < order_plan->order_key_count; ++index) {
        int comparison =
            compare_table_select_values(&left->order_values[index], &right->order_values[index]);

        if (comparison != 0) {
            if (order_plan->order_keys[index].direction == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
                comparison = -comparison;
            }
            return comparison;
        }
    }
    return 0;
}

static void apply_update_limit(const struct mylite_sql_ast_node *limit_clause,
                               struct mylite_update_rowset *rowset)
{
    const struct mylite_sql_ast_node *bound = child_at(limit_clause, 0U);
    size_t keep_count = 0U;

    if (limit_clause == NULL) {
        return;
    }
    if (bound == NULL || !bound->has_limit_bound_value) {
        return;
    }
    if (bound->limit_bound_value > (uint64_t)SIZE_MAX) {
        return;
    }

    keep_count = (size_t)bound->limit_bound_value;
    if (keep_count >= rowset->row_count) {
        return;
    }
    for (size_t index = keep_count; index < rowset->row_count; ++index) {
        update_row_deinit(&rowset->rows[index]);
    }
    rowset->row_count = keep_count;
}

static int execute_update_rows_transaction(mylite_stmt *stmt,
                                           const struct mylite_select_table *table,
                                           const struct mylite_insert_table *write_table,
                                           const struct mylite_update_bound_assignment *assignments,
                                           size_t assignment_count,
                                           const struct mylite_update_rowset *rowset)
{
    sqlite3_stmt *update = NULL;
    char *update_sql = NULL;
    uint64_t next_auto_increment = write_table->next_auto_increment;
    int status = begin_sqlite_transaction(stmt->database);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }

    update_sql = build_update_physical_sql(stmt->database, table);
    if (update_sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        rollback_sqlite_transaction(stmt->database);
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, update_sql, -1, SQLITE_PREPARE_PERSISTENT,
                            &update, NULL);
    sqlite3_free(update_sql);
    if (rc != SQLITE_OK) {
        rollback_sqlite_transaction(stmt->database);
        return set_sqlite_error(stmt->database);
    }

    for (size_t index = 0U; index < rowset->row_count; ++index) {
        status = execute_update_row(stmt, update, table, write_table, assignments, assignment_count,
                                    &rowset->rows[index], &next_auto_increment);
        if (status != MYLITE_OK) {
            break;
        }
    }
    sqlite3_finalize(update);

    if (status == MYLITE_OK && write_table->has_auto_increment &&
        next_auto_increment > write_table->next_auto_increment) {
        status = update_table_auto_increment(stmt, table->schema_name, table->table_name,
                                             next_auto_increment);
    }
    if (status == MYLITE_OK) {
        status = commit_sqlite_transaction(stmt->database);
        if (status == MYLITE_OK) {
            return MYLITE_OK;
        }
    }

    rollback_sqlite_transaction(stmt->database);
    stmt->affected_rows = 0;
    return status;
}

static int execute_update_row(mylite_stmt *stmt, sqlite3_stmt *update,
                              const struct mylite_select_table *table,
                              const struct mylite_insert_table *write_table,
                              const struct mylite_update_bound_assignment *assignments,
                              size_t assignment_count, const struct mylite_update_row *stored,
                              uint64_t *next_auto_increment)
{
    struct mylite_update_row candidate = {0};
    int status = copy_update_candidate_values(stmt, stored, &candidate);

    if (status == MYLITE_OK) {
        status = apply_update_assignments(stmt, table, write_table, assignments, assignment_count,
                                          &candidate);
    }
    if (status == MYLITE_OK) {
        status = validate_update_unique_indexes(stmt, table, write_table, &candidate);
    }
    if (status == MYLITE_OK && update_row_changed(stored, &candidate)) {
        status = write_update_candidate(stmt, update, table, write_table, &candidate,
                                        next_auto_increment);
    }

    update_row_deinit(&candidate);
    return status;
}

static int write_update_candidate(mylite_stmt *stmt, sqlite3_stmt *update,
                                  const struct mylite_select_table *table,
                                  const struct mylite_insert_table *write_table,
                                  const struct mylite_update_row *candidate,
                                  uint64_t *next_auto_increment)
{
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    sqlite3_reset(update);
    sqlite3_clear_bindings(update);
    status = bind_update_row_values(stmt->database, update, candidate);
    if (status != MYLITE_OK) {
        return status;
    }

    rc = sqlite3_bind_int64(update, (int)candidate->value_count + 1, candidate->rowid);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    rc = sqlite3_step(update);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(stmt->database);
    }

    ++stmt->affected_rows;
    return advance_update_auto_increment(stmt, table, write_table, candidate, next_auto_increment);
}

static char *build_update_physical_sql(mylite_db *database, const struct mylite_select_table *table)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "UPDATE \"%w\" SET ", table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" = ?", table->columns[index].name);
    }
    sqlite3_str_append(sql, " WHERE rowid = ?", (int)strlen(" WHERE rowid = ?"));
    return sqlite3_str_finish(sql);
}

static int copy_update_candidate_values(mylite_stmt *stmt, const struct mylite_update_row *row,
                                        struct mylite_update_row *candidate)
{
    candidate->rowid = row->rowid;
    candidate->values = calloc(row->value_count, sizeof(*candidate->values));
    if (candidate->values == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    candidate->value_count = row->value_count;

    for (size_t index = 0U; index < row->value_count; ++index) {
        if (mylite_expression_value_copy(&row->values[index], &candidate->values[index]) != 0) {
            update_row_deinit(candidate);
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int apply_update_assignments(mylite_stmt *stmt, const struct mylite_select_table *table,
                                    const struct mylite_insert_table *write_table,
                                    const struct mylite_update_bound_assignment *assignments,
                                    size_t assignment_count, struct mylite_update_row *candidate)
{
    for (size_t index = 0U; index < assignment_count; ++index) {
        size_t column_index = assignments[index].column_index;
        struct mylite_expression_value value = {0};
        int status = evaluate_update_assignment_value(
            stmt, table, write_table, candidate, column_index, assignments[index].value, &value);

        if (status != MYLITE_OK) {
            mylite_expression_value_deinit(&value);
            return status;
        }

        mylite_expression_value_deinit(&candidate->values[column_index]);
        candidate->values[column_index] = value;
    }
    return MYLITE_OK;
}

static int evaluate_update_assignment_value(mylite_stmt *stmt,
                                            const struct mylite_select_table *table,
                                            const struct mylite_insert_table *write_table,
                                            const struct mylite_update_row *candidate,
                                            size_t target_column,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_expression_value *out_value)
{
    const struct mylite_insert_table_column *column = &write_table->columns[target_column];
    struct mylite_update_expression_context user_context = {
        .table = table,
        .row = candidate,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = resolve_update_expression_identifier,
    };
    int status = MYLITE_OK;

    if (expression != NULL && expression->kind == MYLITE_SQL_AST_DEFAULT) {
        status = resolve_update_default_value(stmt, column, out_value);
    } else {
        status = mylite_expression_eval_with_context(expression, &context,
                                                     &stmt->database->warnings, out_value) == 0
                     ? MYLITE_OK
                     : set_update_unsupported_assignment_error(stmt->database);
    }
    if (status == MYLITE_OK) {
        status = validate_update_assignment_value(stmt, column, out_value);
    }
    return status;
}

static int resolve_update_default_value(mylite_stmt *stmt,
                                        const struct mylite_insert_table_column *column,
                                        struct mylite_expression_value *out_value)
{
    struct mylite_insert_bound_value value = {0};
    int status = MYLITE_OK;

    if (column->auto_increment) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = 0,
        };
        return MYLITE_OK;
    }
    if (column->default_text == NULL) {
        if (column->nullable) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return MYLITE_OK;
        }
        return set_insert_no_default_error(stmt->database, column->name);
    }
    if (column_default_is_current_timestamp(column->default_text)) {
        char *timestamp = insert_current_timestamp_text();

        if (timestamp == NULL) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    }
    if (column->generated_default) {
        return set_insert_unsupported_generated_default_error(stmt->database, column->name);
    }

    status = resolve_insert_text_value(stmt, column, column->default_text, NULL, &value);
    if (status == MYLITE_OK) {
        status = copy_insert_bound_value_to_expression(&value, out_value);
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(stmt->database, "out of memory");
        }
    }
    insert_bound_value_deinit(&value);
    return status;
}

static int copy_insert_bound_value_to_expression(const struct mylite_insert_bound_value *value,
                                                 struct mylite_expression_value *out_value)
{
    switch (value->kind) {
    case MYLITE_INSERT_BOUND_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_INTEGER:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = value->integer_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_REAL:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = value->real_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_BOUND_TEXT:
        out_value->text_value = copy_span_text(
            value->text_value, value->text_value == NULL ? 0U : strlen(value->text_value));
        if (out_value->text_value == NULL) {
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int validate_update_assignment_value(mylite_stmt *stmt,
                                            const struct mylite_insert_table_column *column,
                                            struct mylite_expression_value *value)
{
    int64_t integer_value = 0;

    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        if (column->nullable) {
            return MYLITE_OK;
        }
        return set_insert_null_error(stmt->database, column->name);
    }
    if (!column->auto_increment) {
        return MYLITE_OK;
    }

    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64 &&
        value->uint64_value <= (uint64_t)INT64_MAX) {
        value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        value->int64_value = (int64_t)value->uint64_value;
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT &&
        parse_insert_integer_text(value->text_value, &integer_value)) {
        mylite_expression_value_deinit(value);
        *value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = integer_value,
        };
        return MYLITE_OK;
    }
    return set_update_unsupported_assignment_error(stmt->database);
}

static int validate_update_unique_indexes(mylite_stmt *stmt,
                                          const struct mylite_select_table *table,
                                          const struct mylite_insert_table *write_table,
                                          const struct mylite_update_row *candidate)
{
    for (size_t index = 0U; index < write_table->unique_index_count; ++index) {
        bool conflicts = false;
        int status = update_unique_index_conflicts(
            stmt, table, write_table, &write_table->unique_indexes[index], candidate, &conflicts);

        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            return set_update_duplicate_entry_error(stmt->database, table->table_name,
                                                    &write_table->unique_indexes[index], candidate);
        }
    }
    return MYLITE_OK;
}

static int update_unique_index_conflicts(mylite_stmt *stmt, const struct mylite_select_table *table,
                                         const struct mylite_insert_table *write_table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_update_row *candidate,
                                         bool *out_conflicts)
{
    char *sql = NULL;
    sqlite3_stmt *check = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_conflicts = false;
    for (size_t part = 0U; part < index->column_count; ++part) {
        if (candidate->values[index->column_indexes[part]].kind == MYLITE_EXPRESSION_VALUE_NULL) {
            return MYLITE_OK;
        }
    }

    sql = build_update_unique_check_sql(stmt->database, table, write_table, index);
    if (sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check,
                            NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    status = bind_update_unique_check_values(stmt->database, check, index, candidate);
    if (status == MYLITE_OK) {
        rc = sqlite3_step(check);
        if (rc == SQLITE_ROW) {
            *out_conflicts = true;
        } else if (rc != SQLITE_DONE) {
            status = set_sqlite_error(stmt->database);
        }
    }
    sqlite3_finalize(check);
    return status;
}

static char *build_update_unique_check_sql(mylite_db *database,
                                           const struct mylite_select_table *table,
                                           const struct mylite_insert_table *write_table,
                                           const struct mylite_insert_unique_index *index)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT 1 FROM \"%w\" WHERE ", table->physical_name);
    for (size_t part = 0U; part < index->column_count; ++part) {
        size_t column_index = index->column_indexes[part];

        if (part != 0U) {
            sqlite3_str_append(sql, " AND ", (int)strlen(" AND "));
        }
        sqlite3_str_appendf(sql, "\"%w\" = ?", write_table->columns[column_index].name);
    }
    sqlite3_str_append(sql, " AND rowid <> ? LIMIT 1", (int)strlen(" AND rowid <> ? LIMIT 1"));
    return sqlite3_str_finish(sql);
}

static int bind_update_unique_check_values(mylite_db *database, sqlite3_stmt *check,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_update_row *candidate)
{
    for (size_t part = 0U; part < index->column_count; ++part) {
        int rc = bind_update_value(check, (int)part + 1,
                                   &candidate->values[index->column_indexes[part]]);

        if (rc != SQLITE_OK) {
            return set_sqlite_error(database);
        }
    }

    {
        int rc = sqlite3_bind_int64(check, (int)index->column_count + 1, candidate->rowid);

        if (rc != SQLITE_OK) {
            return set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static int bind_update_row_values(mylite_db *database, sqlite3_stmt *update,
                                  const struct mylite_update_row *candidate)
{
    for (size_t index = 0U; index < candidate->value_count; ++index) {
        int rc = bind_update_value(update, (int)index + 1, &candidate->values[index]);

        if (rc != SQLITE_OK) {
            return set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static int bind_update_value(sqlite3_stmt *stmt, int index,
                             const struct mylite_expression_value *value)
{
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return sqlite3_bind_null(stmt, index);
    case MYLITE_EXPRESSION_VALUE_INT64:
        return sqlite3_bind_int64(stmt, index, value->int64_value);
    case MYLITE_EXPRESSION_VALUE_UINT64:
        if (value->uint64_value > (uint64_t)INT64_MAX) {
            return SQLITE_RANGE;
        }
        return sqlite3_bind_int64(stmt, index, (sqlite3_int64)value->uint64_value);
    case MYLITE_EXPRESSION_VALUE_REAL:
        return sqlite3_bind_double(stmt, index, value->real_value);
    case MYLITE_EXPRESSION_VALUE_TEXT:
        return sqlite3_bind_text(stmt, index, value->text_value, -1, sqlite_transient_destructor());
    }
    return SQLITE_MISUSE;
}

static int advance_update_auto_increment(mylite_stmt *stmt, const struct mylite_select_table *table,
                                         const struct mylite_insert_table *write_table,
                                         const struct mylite_update_row *candidate,
                                         uint64_t *next_auto_increment)
{
    uint64_t value = 0U;

    (void)table;
    if (!write_table->has_auto_increment ||
        !update_expression_value_positive_uint64(
            &candidate->values[write_table->auto_increment_column_index], &value)) {
        return MYLITE_OK;
    }
    if (value == UINT64_MAX) {
        (void)set_error_message(stmt->database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    if (value >= *next_auto_increment) {
        *next_auto_increment = value + 1U;
    }
    return MYLITE_OK;
}

static bool update_expression_value_positive_uint64(const struct mylite_expression_value *value,
                                                    uint64_t *out_value)
{
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64 && value->int64_value > 0) {
        *out_value = (uint64_t)value->int64_value;
        return true;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64 && value->uint64_value > 0U) {
        *out_value = value->uint64_value;
        return true;
    }
    return false;
}

static bool update_row_changed(const struct mylite_update_row *stored,
                               const struct mylite_update_row *candidate)
{
    if (stored->value_count != candidate->value_count) {
        return true;
    }
    for (size_t index = 0U; index < stored->value_count; ++index) {
        if (!update_values_equal(&stored->values[index], &candidate->values[index])) {
            return true;
        }
    }
    return false;
}

static bool update_values_equal(const struct mylite_expression_value *left,
                                const struct mylite_expression_value *right)
{
    if (left->kind != right->kind) {
        return false;
    }
    switch (left->kind) {
    case MYLITE_EXPRESSION_VALUE_NULL:
        return true;
    case MYLITE_EXPRESSION_VALUE_INT64:
        return left->int64_value == right->int64_value;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        return left->uint64_value == right->uint64_value;
    case MYLITE_EXPRESSION_VALUE_REAL:
        return left->real_value == right->real_value;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        if (left->text_value == NULL || right->text_value == NULL) {
            return left->text_value == right->text_value;
        }
        return strcmp(left->text_value, right->text_value) == 0;
    }
    return false;
}

static int resolve_update_expression_identifier(void *user_data,
                                                const struct mylite_sql_ast_node *identifier,
                                                struct mylite_expression_value *out_value)
{
    struct mylite_update_expression_context *context = user_data;
    size_t column_index = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->table == NULL || context->row == NULL) {
        return -1;
    }

    status = resolve_select_column_reference(context->table, identifier, &column_index);
    if (status != MYLITE_OK || column_index == context->table->column_count ||
        column_index >= context->row->value_count) {
        return -1;
    }
    return mylite_expression_value_copy(&context->row->values[column_index], out_value);
}

static size_t update_column_reference_index(const struct mylite_select_table *table,
                                            const struct mylite_update_column_reference *reference)
{
    if (!update_column_reference_qualifiers_match(table, reference)) {
        return table->column_count;
    }
    return select_column_index(table, reference->column_name);
}

static bool
update_column_reference_qualifiers_match(const struct mylite_select_table *table,
                                         const struct mylite_update_column_reference *reference)
{
    if (reference->schema_name != NULL) {
        if (table->alias != NULL || reference->table_name == NULL) {
            return false;
        }
        if (strcmp(reference->schema_name, table->schema_name) != 0) {
            return false;
        }
        if (strcmp(reference->table_name, table->table_name) != 0) {
            return false;
        }
        return true;
    }
    if (reference->table_name != NULL) {
        const char *visible_table = table->alias == NULL ? table->table_name : table->alias;

        if (strcmp(reference->table_name, visible_table) != 0) {
            return false;
        }
        return true;
    }
    return true;
}

static char *
copy_update_column_reference_name(const struct mylite_update_column_reference *reference)
{
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }
    if (reference->schema_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->schema_name);
    }
    if (reference->table_name != NULL) {
        sqlite3_str_appendf(text, "%s.", reference->table_name);
    }
    sqlite3_str_append(text, reference->column_name == NULL ? "" : reference->column_name,
                       reference->column_name == NULL ? 0 : (int)strlen(reference->column_name));
    return sqlite3_str_finish(text);
}

static int set_update_unknown_column_error(mylite_db *database, const char *column_name,
                                           const char *clause_context)
{
    char *message = sqlite3_mprintf("Unknown column '%q' in '%q'", column_name,
                                    clause_context == NULL ? "field list" : clause_context);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_update_unknown_field_error(mylite_db *database, const char *column_name)
{
    return set_update_unknown_column_error(database, column_name, "field list");
}

static int set_update_duplicate_entry_error(mylite_db *database, const char *table_name,
                                            const struct mylite_insert_unique_index *index,
                                            const struct mylite_update_row *candidate)
{
    char *entry = copy_update_duplicate_entry_value(index, candidate);
    char *message = NULL;
    int status = MYLITE_OK;

    if (entry == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    message =
        sqlite3_mprintf("Duplicate entry '%q' for key '%q.%q'", entry, table_name, index->name);
    free(entry);
    if (message == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static char *copy_update_duplicate_entry_value(const struct mylite_insert_unique_index *index,
                                               const struct mylite_update_row *candidate)
{
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }

    for (size_t part = 0U; part < index->column_count; ++part) {
        const struct mylite_expression_value *value =
            &candidate->values[index->column_indexes[part]];

        if (part != 0U) {
            sqlite3_str_append(text, "-", 1);
        }
        switch (value->kind) {
        case MYLITE_EXPRESSION_VALUE_NULL:
            sqlite3_str_append(text, "NULL", (int)strlen("NULL"));
            break;
        case MYLITE_EXPRESSION_VALUE_INT64:
            sqlite3_str_appendf(text, "%lld", (long long)value->int64_value);
            break;
        case MYLITE_EXPRESSION_VALUE_UINT64:
            sqlite3_str_appendf(text, "%llu", (unsigned long long)value->uint64_value);
            break;
        case MYLITE_EXPRESSION_VALUE_REAL:
            sqlite3_str_appendf(text, "%.15g", value->real_value);
            break;
        case MYLITE_EXPRESSION_VALUE_TEXT:
            sqlite3_str_append(text, value->text_value == NULL ? "" : value->text_value,
                               value->text_value == NULL ? 0 : (int)strlen(value->text_value));
            break;
        }
    }
    return sqlite3_str_finish(text);
}

static int set_update_unsupported_expression_error(mylite_db *database, const char *clause_context)
{
    if (clause_context != NULL && strcmp(clause_context, "field list") == 0) {
        return set_update_unsupported_assignment_error(database);
    }
    return set_update_unsupported_clause_error(database);
}

static int set_update_unsupported_clause_error(mylite_db *database)
{
    if (set_error_message(database, "Unsupported UPDATE clause") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int set_update_unsupported_assignment_error(mylite_db *database)
{
    if (set_error_message(database, "Unsupported UPDATE assignment") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int execute_scalar_select_statement(mylite_stmt *stmt)
{
    if (stmt->scalar_result.has_row) {
        return MYLITE_DONE;
    }

    stmt->scalar_result.has_row = true;
    stmt->executed = true;
    stmt->database->warnings = stmt->scalar_result.warnings;
    stmt->scalar_result.warnings = (struct mylite_expression_warnings){0};
    stmt->affected_rows = -1;
    return MYLITE_ROW;
}

static int execute_table_select_statement(mylite_stmt *stmt)
{
    if (stmt->sqlite_stmt == NULL) {
        return MYLITE_MISUSE;
    }
    stmt->executed = true;
    stmt->affected_rows = -1;

    int status = materialize_table_select_result(stmt);

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_result.next_row >= stmt->select_result.row_count) {
        table_select_current_values_deinit(&stmt->select_result);
        stmt->select_result.has_current_row = false;
        return MYLITE_DONE;
    }

    status =
        set_table_select_current_row(stmt, &stmt->select_result.rows[stmt->select_result.next_row]);
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

static int materialize_table_select_result(mylite_stmt *stmt)
{
    int status = MYLITE_OK;

    if (stmt->select_result.materialized) {
        return MYLITE_OK;
    }
    if (stmt->select_plan.order_key_count != 0U) {
        status = materialize_ordered_table_select_result(stmt);
    } else {
        status = materialize_unordered_table_select_result(stmt);
    }
    if (status == MYLITE_OK) {
        stmt->select_result.materialized = true;
    }
    return status;
}

static int materialize_ordered_table_select_result(mylite_stmt *stmt)
{
    int status = evaluate_table_select_constant_predicate(stmt);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = evaluate_table_select_row_matches(stmt, &matches);
        if (status != MYLITE_OK) {
            return status;
        }
        if (!matches) {
            continue;
        }

        status = copy_table_select_sqlite_row(stmt, &row);
        if (status == MYLITE_OK) {
            status = evaluate_table_select_order_values(stmt, &row);
        }
        if (status == MYLITE_OK) {
            status = append_table_select_result_row(stmt, &row);
        }
        if (status != MYLITE_OK) {
            table_select_row_deinit(&row);
            return status;
        }
    }
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(stmt->database);
    }

    status = sort_table_select_result_rows(stmt);
    if (status == MYLITE_OK) {
        status = apply_table_select_limit(stmt);
    }
    return status;
}

static int materialize_unordered_table_select_result(mylite_stmt *stmt)
{
    uint64_t matched_row = 0U;
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (stmt->select_plan.limit.has_limit && stmt->select_plan.limit.row_count == 0U) {
        return MYLITE_OK;
    }

    status = evaluate_table_select_constant_predicate(stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_constant_predicate_evaluated && !stmt->select_constant_predicate_matches) {
        return MYLITE_OK;
    }

    while ((rc = sqlite3_step(stmt->sqlite_stmt)) == SQLITE_ROW) {
        struct mylite_table_select_row row = {0};
        bool matches = false;

        status = evaluate_table_select_row_matches(stmt, &matches);
        if (status != MYLITE_OK) {
            return status;
        }
        if (!matches) {
            continue;
        }
        if (table_select_limit_row_is_kept(&stmt->select_plan.limit,
                                           (struct mylite_select_limit_position){
                                               .matched_row = matched_row,
                                               .kept_count = stmt->select_result.row_count,
                                           })) {
            status = copy_table_select_sqlite_row(stmt, &row);
            if (status == MYLITE_OK) {
                status = append_table_select_result_row(stmt, &row);
            }
            if (status != MYLITE_OK) {
                table_select_row_deinit(&row);
                return status;
            }
        }
        if (matched_row != UINT64_MAX) {
            ++matched_row;
        }
        if (table_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count)) {
            break;
        }
    }
    if (rc != SQLITE_DONE &&
        !table_select_limit_is_full(&stmt->select_plan.limit, stmt->select_result.row_count)) {
        return set_sqlite_error(stmt->database);
    }
    return MYLITE_OK;
}

static int evaluate_table_select_row_matches(mylite_stmt *stmt, bool *out_matches)
{
    *out_matches = true;
    if (stmt->select_predicate == NULL || stmt->select_constant_predicate_evaluated) {
        return MYLITE_OK;
    }
    return evaluate_table_select_row_predicate(stmt, out_matches);
}

static int append_table_select_result_row(mylite_stmt *stmt, struct mylite_table_select_row *row)
{
    struct mylite_table_select_row *rows =
        realloc(stmt->select_result.rows,
                (stmt->select_result.row_count + 1U) * sizeof(*stmt->select_result.rows));

    if (rows == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    stmt->select_result.rows = rows;
    stmt->select_result.rows[stmt->select_result.row_count++] = *row;
    *row = (struct mylite_table_select_row){0};
    return MYLITE_OK;
}

static int copy_table_select_sqlite_row(mylite_stmt *stmt, struct mylite_table_select_row *out_row)
{
    size_t column_count = stmt->select_plan.table.column_count;

    out_row->values = calloc(column_count, sizeof(*out_row->values));
    if (out_row->values == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->value_count = column_count;

    for (size_t index = 0U; index < column_count; ++index) {
        int status = copy_table_select_column_value(stmt, index, &out_row->values[index]);

        if (status != 0) {
            table_select_row_deinit(out_row);
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_order_values(mylite_stmt *stmt,
                                              struct mylite_table_select_row *row)
{
    size_t order_key_count = stmt->select_plan.order_key_count;

    row->order_values = calloc(order_key_count, sizeof(*row->order_values));
    if (row->order_values == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    row->order_value_count = order_key_count;

    for (size_t index = 0U; index < order_key_count; ++index) {
        int status = evaluate_table_select_order_key(
            stmt, row, &stmt->select_plan.order_keys[index], &row->order_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int evaluate_table_select_order_key(mylite_stmt *stmt,
                                           const struct mylite_table_select_row *row,
                                           const struct mylite_select_order_key *order_key,
                                           struct mylite_expression_value *out_value)
{
    if (order_key->kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
        return evaluate_table_select_output_value(stmt, row, order_key->output_index, out_value);
    }

    struct mylite_table_select_expression_context user_context = {
        .stmt = stmt,
        .row = row,
        .order_resolution = true,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = resolve_table_select_expression_identifier,
        .eval_constant = evaluate_table_select_cached_constant_expression,
    };
    int status = mylite_expression_eval_with_context(order_key->expression, &context,
                                                     &stmt->database->warnings, out_value);

    if (status != 0) {
        return set_where_predicate_eval_error(stmt);
    }
    return MYLITE_OK;
}

static int sort_table_select_result_rows(mylite_stmt *stmt)
{
    size_t row_count = stmt->select_result.row_count;
    struct mylite_table_select_row *scratch = NULL;
    int status = MYLITE_OK;

    if (row_count < 2U) {
        return MYLITE_OK;
    }

    scratch = calloc(row_count, sizeof(*scratch));
    if (scratch == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = merge_sort_table_select_rows(stmt->select_result.rows, scratch, 0U, row_count,
                                          &stmt->select_plan);
    free(scratch);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int merge_sort_table_select_rows(struct mylite_table_select_row *rows,
                                        struct mylite_table_select_row *scratch, size_t first,
                                        size_t last, const struct mylite_select_plan *plan)
{
    size_t count = last - first;
    size_t middle = first + (count / 2U);

    if (count < 2U) {
        return MYLITE_OK;
    }

    int status = merge_sort_table_select_rows(rows, scratch, first, middle, plan);

    if (status == MYLITE_OK) {
        status = merge_sort_table_select_rows(rows, scratch, middle, last, plan);
    }
    if (status == MYLITE_OK) {
        merge_table_select_rows(rows, scratch, first, middle, last, plan);
    }
    return status;
}

static void merge_table_select_rows(struct mylite_table_select_row *rows,
                                    struct mylite_table_select_row *scratch, size_t first,
                                    size_t middle, size_t last,
                                    const struct mylite_select_plan *plan)
{
    size_t left = first;
    size_t right = middle;
    size_t output = first;

    while (left < middle && right < last) {
        if (compare_table_select_rows(&rows[left], &rows[right], plan) <= 0) {
            scratch[output++] = rows[left++];
        } else {
            scratch[output++] = rows[right++];
        }
    }
    while (left < middle) {
        scratch[output++] = rows[left++];
    }
    while (right < last) {
        scratch[output++] = rows[right++];
    }
    for (size_t index = first; index < last; ++index) {
        rows[index] = scratch[index];
    }
}

static int compare_table_select_rows(const struct mylite_table_select_row *left,
                                     const struct mylite_table_select_row *right,
                                     const struct mylite_select_plan *plan)
{
    for (size_t index = 0U; index < plan->order_key_count; ++index) {
        int comparison =
            compare_table_select_values(&left->order_values[index], &right->order_values[index]);

        if (comparison != 0) {
            if (plan->order_keys[index].direction == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
                comparison = -comparison;
            }
            return comparison;
        }
    }
    return 0;
}

static int compare_table_select_values(const struct mylite_expression_value *left,
                                       const struct mylite_expression_value *right)
{
    bool left_null = left->kind == MYLITE_EXPRESSION_VALUE_NULL;
    bool right_null = right->kind == MYLITE_EXPRESSION_VALUE_NULL;

    if (left_null || right_null) {
        if (left_null == right_null) {
            return 0;
        }
        if (left_null) {
            return -1;
        }
        return 1;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_TEXT && right->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return compare_table_select_text_values(left->text_value, right->text_value);
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_REAL || right->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        double left_value = left->kind == MYLITE_EXPRESSION_VALUE_REAL
                                ? left->real_value
                                : (double)mylite_expression_value_to_int64(left);
        double right_value = right->kind == MYLITE_EXPRESSION_VALUE_REAL
                                 ? right->real_value
                                 : (double)mylite_expression_value_to_int64(right);

        return (left_value > right_value) - (left_value < right_value);
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_UINT64 ||
        right->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        uint64_t left_value = left->kind == MYLITE_EXPRESSION_VALUE_UINT64
                                  ? left->uint64_value
                                  : (uint64_t)mylite_expression_value_to_int64(left);
        uint64_t right_value = right->kind == MYLITE_EXPRESSION_VALUE_UINT64
                                   ? right->uint64_value
                                   : (uint64_t)mylite_expression_value_to_int64(right);

        return (left_value > right_value) - (left_value < right_value);
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_TEXT || right->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        char *left_text = mylite_expression_value_to_text(left);
        char *right_text = mylite_expression_value_to_text(right);
        int comparison = compare_table_select_text_values(left_text, right_text);

        free(left_text);
        free(right_text);
        return comparison;
    }
    return (left->int64_value > right->int64_value) - (left->int64_value < right->int64_value);
}

static int compare_table_select_text_values(const char *left, const char *right)
{
    size_t index = 0U;

    if (left == NULL) {
        left = "";
    }
    if (right == NULL) {
        right = "";
    }
    while (left[index] != '\0' && right[index] != '\0') {
        unsigned char left_byte = (unsigned char)left[index];
        unsigned char right_byte = (unsigned char)right[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return (left_byte > right_byte) - (left_byte < right_byte);
        }
        ++index;
    }
    return ((unsigned char)left[index] > (unsigned char)right[index]) -
           ((unsigned char)left[index] < (unsigned char)right[index]);
}

static int apply_table_select_limit(mylite_stmt *stmt)
{
    const struct mylite_select_limit *limit = &stmt->select_plan.limit;
    size_t kept = 0U;

    if (!limit->has_limit) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < stmt->select_result.row_count; ++index) {
        if (table_select_limit_row_is_kept(limit, (struct mylite_select_limit_position){
                                                      .matched_row = (uint64_t)index,
                                                      .kept_count = kept,
                                                  })) {
            if (kept != index) {
                stmt->select_result.rows[kept] = stmt->select_result.rows[index];
                stmt->select_result.rows[index] = (struct mylite_table_select_row){0};
            }
            ++kept;
        } else {
            table_select_row_deinit(&stmt->select_result.rows[index]);
        }
    }
    stmt->select_result.row_count = kept;
    return MYLITE_OK;
}

static bool table_select_limit_row_is_kept(const struct mylite_select_limit *limit,
                                           struct mylite_select_limit_position position)
{
    if (!limit->has_limit) {
        return true;
    }
    if (position.matched_row < limit->offset) {
        return false;
    }
    if (table_select_limit_is_full(limit, position.kept_count)) {
        return false;
    }
    return true;
}

static bool table_select_limit_is_full(const struct mylite_select_limit *limit, size_t kept_count)
{
    if (!limit->has_limit) {
        return false;
    }
    if (limit->row_count > (uint64_t)SIZE_MAX) {
        return false;
    }
    return kept_count >= (size_t)limit->row_count;
}

static int set_table_select_current_row(mylite_stmt *stmt,
                                        const struct mylite_table_select_row *row)
{
    table_select_current_values_deinit(&stmt->select_result);

    stmt->select_result.current_values =
        calloc(stmt->select_plan.output_count, sizeof(*stmt->select_result.current_values));
    stmt->select_result.current_texts =
        (char **)calloc(stmt->select_plan.output_count, sizeof(*stmt->select_result.current_texts));
    if (stmt->select_result.current_values == NULL || stmt->select_result.current_texts == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    stmt->select_result.current_value_count = stmt->select_plan.output_count;

    for (size_t index = 0U; index < stmt->select_plan.output_count; ++index) {
        int status = evaluate_table_select_output_value(stmt, row, index,
                                                        &stmt->select_result.current_values[index]);

        if (status != MYLITE_OK) {
            return status;
        }
        if (stmt->select_result.current_values[index].kind != MYLITE_EXPRESSION_VALUE_NULL) {
            stmt->select_result.current_texts[index] =
                mylite_expression_value_to_text(&stmt->select_result.current_values[index]);
            if (stmt->select_result.current_texts[index] == NULL) {
                (void)set_error_message(stmt->database, "out of memory");
                return MYLITE_NOMEM;
            }
        }
    }
    stmt->select_result.has_current_row = true;
    return MYLITE_OK;
}

static int evaluate_table_select_output_value(mylite_stmt *stmt,
                                              const struct mylite_table_select_row *row,
                                              size_t output_index,
                                              struct mylite_expression_value *out_value)
{
    const struct mylite_select_output_column *output = &stmt->select_plan.outputs[output_index];

    if (output->kind == MYLITE_SELECT_OUTPUT_COLUMN) {
        if (copy_table_select_row_value(row, output->column_index, out_value) != 0) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }

    struct mylite_table_select_expression_context user_context = {
        .stmt = stmt,
        .row = row,
        .order_resolution = false,
    };
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = resolve_table_select_expression_identifier,
        .eval_constant = evaluate_table_select_cached_constant_expression,
    };
    int status = mylite_expression_eval_with_context(output->expression, &context,
                                                     &stmt->database->warnings, out_value);

    if (status != 0) {
        return set_where_predicate_eval_error(stmt);
    }
    return MYLITE_OK;
}

static int copy_table_select_row_value(const struct mylite_table_select_row *row,
                                       size_t column_index,
                                       struct mylite_expression_value *out_value)
{
    if (row == NULL || column_index >= row->value_count) {
        return -1;
    }
    return mylite_expression_value_copy(&row->values[column_index], out_value);
}

static const struct mylite_expression_value *
table_select_current_output_value(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL || stmt->kind != MYLITE_STMT_TABLE_SELECT || column < 0 ||
        (size_t)column >= stmt->select_result.current_value_count ||
        !stmt->select_result.has_current_row) {
        return NULL;
    }
    return &stmt->select_result.current_values[column];
}

static const char *table_select_current_output_text(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL || stmt->kind != MYLITE_STMT_TABLE_SELECT || column < 0 ||
        (size_t)column >= stmt->select_result.current_value_count ||
        !stmt->select_result.has_current_row) {
        return NULL;
    }
    return stmt->select_result.current_texts[column];
}

static void table_select_current_values_deinit(struct mylite_table_select_result *result)
{
    if (result == NULL) {
        return;
    }
    for (size_t index = 0U; index < result->current_value_count; ++index) {
        mylite_expression_value_deinit(&result->current_values[index]);
        free(result->current_texts[index]);
    }
    free(result->current_values);
    free((void *)result->current_texts);
    result->current_values = NULL;
    result->current_texts = NULL;
    result->current_value_count = 0U;
    result->has_current_row = false;
}

static int evaluate_table_select_constant_predicate(mylite_stmt *stmt)
{
    struct mylite_expression_value value = {0};
    int truth = -1;
    int status = 0;

    if (stmt->select_constant_predicate_evaluated ||
        !mylite_expression_is_supported_no_table(stmt->select_predicate)) {
        return MYLITE_OK;
    }

    status = mylite_expression_eval(stmt->select_predicate, &stmt->database->warnings, &value);
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return set_where_predicate_eval_error(stmt);
    }

    stmt->select_constant_predicate_evaluated = true;
    stmt->select_constant_predicate_matches = truth == 1;
    mylite_expression_value_deinit(&value);
    return MYLITE_OK;
}

static int evaluate_table_select_row_predicate(mylite_stmt *stmt, bool *out_matches)
{
    struct mylite_table_select_expression_context user_context = {.stmt = stmt};
    struct mylite_expression_eval_context context = {
        .user_data = &user_context,
        .resolve_identifier = resolve_table_select_expression_identifier,
        .eval_constant = evaluate_table_select_cached_constant_expression,
    };
    struct mylite_expression_value value = {0};
    int truth = -1;
    int status = mylite_expression_eval_with_context(stmt->select_predicate, &context,
                                                     &stmt->database->warnings, &value);

    *out_matches = false;
    if (status == 0) {
        status = mylite_expression_value_truth(&value, &stmt->database->warnings, &truth);
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return set_where_predicate_eval_error(stmt);
    }

    *out_matches = truth == 1;
    return MYLITE_OK;
}

static int evaluate_table_select_cached_constant_expression(
    void *user_data, const struct mylite_sql_ast_node *expression,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    struct mylite_table_select_expression_context *context = user_data;
    mylite_stmt *stmt = context == NULL ? NULL : context->stmt;
    struct mylite_cached_expression_value *entry = NULL;

    if (stmt == NULL || expression == NULL) {
        return -1;
    }

    for (size_t index = 0U; index < stmt->select_constant_value_count; ++index) {
        if (stmt->select_constant_values[index].expression == expression) {
            entry = &stmt->select_constant_values[index];
            break;
        }
    }
    if (entry == NULL) {
        struct mylite_cached_expression_value *values =
            realloc(stmt->select_constant_values, (stmt->select_constant_value_count + 1U) *
                                                      sizeof(*stmt->select_constant_values));

        if (values == NULL) {
            return -1;
        }
        stmt->select_constant_values = values;
        entry = &stmt->select_constant_values[stmt->select_constant_value_count++];
        *entry = (struct mylite_cached_expression_value){.expression = expression};
    }

    if (!entry->evaluated) {
        entry->status = mylite_expression_eval(expression, warnings, &entry->value);
        entry->evaluated = true;
    }
    if (entry->status != 0) {
        return entry->status;
    }
    return mylite_expression_value_copy(&entry->value, out_value);
}

static int resolve_table_select_expression_identifier(void *user_data,
                                                      const struct mylite_sql_ast_node *identifier,
                                                      struct mylite_expression_value *out_value)
{
    struct mylite_table_select_expression_context *context = user_data;
    size_t column_index = 0U;
    int status = MYLITE_OK;

    if (context == NULL || context->stmt == NULL) {
        return -1;
    }

    if (context->order_resolution) {
        enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
        size_t index = 0U;

        status = resolve_select_order_reference(
            context->stmt->database, &context->stmt->select_plan, identifier, &kind, &index);
        if (status != MYLITE_OK) {
            return -1;
        }
        if (kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
            return evaluate_table_select_output_value(context->stmt, context->row, index,
                                                      out_value) == MYLITE_OK
                       ? 0
                       : -1;
        }
    }

    status = resolve_select_column_reference(&context->stmt->select_plan.table, identifier,
                                             &column_index);
    if (status != MYLITE_OK || column_index == context->stmt->select_plan.table.column_count) {
        return -1;
    }
    if (context->row != NULL) {
        return copy_table_select_row_value(context->row, column_index, out_value);
    }
    return copy_table_select_column_value(context->stmt, column_index, out_value);
}

static int copy_table_select_column_value(mylite_stmt *stmt, size_t column_index,
                                          struct mylite_expression_value *out_value)
{
    int sqlite_type = SQLITE_NULL;

    if (stmt == NULL || stmt->sqlite_stmt == NULL ||
        column_index >= stmt->select_plan.table.column_count) {
        return -1;
    }

    sqlite_type = sqlite3_column_type(stmt->sqlite_stmt, (int)column_index);
    switch (sqlite_type) {
    case SQLITE_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    case SQLITE_INTEGER:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = sqlite3_column_int64(stmt->sqlite_stmt, (int)column_index)};
        return 0;
    case SQLITE_FLOAT:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = sqlite3_column_double(stmt->sqlite_stmt, (int)column_index)};
        return 0;
    case SQLITE_TEXT:
    case SQLITE_BLOB: {
        const unsigned char *text = sqlite3_column_text(stmt->sqlite_stmt, (int)column_index);
        int bytes = sqlite3_column_bytes(stmt->sqlite_stmt, (int)column_index);

        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = copy_span_text((const char *)text, bytes < 0 ? 0U : (size_t)bytes);
        return out_value->text_value == NULL ? -1 : 0;
    }
    default:
        break;
    }
    return -1;
}

static int set_where_predicate_eval_error(mylite_stmt *stmt)
{
    mylite_db *database = stmt->database;

    if (database->warnings.count != 0U) {
        const struct mylite_expression_warning *warning =
            &database->warnings.items[database->warnings.count - 1U];

        if (warning->code == MYLITE_MYSQL_ER_WRONG_ARGUMENTS) {
            int status = set_error_message(database, warning->message);

            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
    }
    return set_select_unsupported_where_error(database);
}

static int validate_insert_values_target(mylite_stmt *stmt, const char **out_schema_name)
{
    const char *schema_name = stmt->insert_values.schema_name == NULL
                                  ? stmt->database->selected_schema
                                  : stmt->insert_values.schema_name;
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = MYLITE_OK;

    *out_schema_name = NULL;
    if (schema_name == NULL) {
        (void)set_error_message(stmt->database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = schema_exists(stmt->database, schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)set_error_message_parts(stmt->database, "Unknown database '", schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)set_error_message_parts(stmt->database, "Access to system schema '", schema_name,
                                      "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = table_exists(stmt->database, schema_name, stmt->insert_values.table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return set_table_doesnt_exist_error(stmt->database, schema_name,
                                            stmt->insert_values.table_name);
    }

    *out_schema_name = schema_name;
    return MYLITE_OK;
}

static int load_insert_table(mylite_stmt *stmt, const char *schema_name,
                             struct mylite_insert_table *out_table)
{
    return load_write_table(stmt, schema_name, stmt->insert_values.table_name, out_table);
}

static int load_write_table(mylite_stmt *stmt, const char *schema_name, const char *table_name,
                            struct mylite_insert_table *out_table)
{
    sqlite3_stmt *select = NULL;
    uint64_t catalog_auto_increment = 0U;
    bool has_catalog_auto_increment = false;
    static const char sql[] =
        "SELECT auto_increment FROM __mylite_table_catalog WHERE table_schema = ? "
        "AND table_name = ?";
    int rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select,
                                NULL);
    int status = MYLITE_OK;

    *out_table = (struct mylite_insert_table){0};
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW && sqlite3_column_type(select, 0) != SQLITE_NULL) {
        sqlite3_int64 value = sqlite3_column_int64(select, 0);

        if (value > 0) {
            catalog_auto_increment = (uint64_t)value;
            has_catalog_auto_increment = true;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_ROW) {
        return rc == SQLITE_DONE
                   ? set_table_doesnt_exist_error(stmt->database, schema_name, table_name)
                   : set_sqlite_error(stmt->database);
    }

    out_table->physical_name = physical_table_name(schema_name, table_name);
    if (out_table->physical_name == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = load_insert_columns(stmt, schema_name, table_name, out_table);
    if (status == MYLITE_OK) {
        status = load_insert_unique_indexes(stmt, schema_name, table_name, out_table);
    }
    if (status == MYLITE_OK) {
        status = initialize_insert_auto_increment(stmt, out_table, catalog_auto_increment,
                                                  has_catalog_auto_increment);
    }
    return status;
}

static int load_insert_columns(mylite_stmt *stmt, const char *schema_name, const char *table_name,
                               struct mylite_insert_table *table)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] =
        "SELECT column_name, column_default, is_nullable, data_type, extra "
        "FROM __mylite_column_catalog WHERE table_schema = ? AND table_name = ? "
        "ORDER BY ordinal_position";
    int rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select,
                                NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        int status = load_insert_column_from_catalog_row(stmt, select, table);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }
    sqlite3_finalize(select);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(stmt->database);
    }
    if (table->column_count == 0U) {
        (void)set_error_message(stmt->database, "INSERT target table has no columns");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
}

static int load_insert_column_from_catalog_row(mylite_stmt *stmt, sqlite3_stmt *select,
                                               struct mylite_insert_table *table)
{
    const char *name = (const char *)sqlite3_column_text(select, 0);
    const char *default_text = (const char *)sqlite3_column_text(select, 1);
    const char *is_nullable = (const char *)sqlite3_column_text(select, 2);
    const char *data_type = (const char *)sqlite3_column_text(select, 3);
    const char *extra = (const char *)sqlite3_column_text(select, 4);
    struct mylite_insert_table_column column = {0};
    int status = MYLITE_OK;

    if (is_nullable != NULL && ascii_case_equal(is_nullable, "YES")) {
        column.nullable = true;
    }
    column.name = copy_span_text(name, name == NULL ? 0U : strlen(name));
    if (default_text != NULL) {
        column.default_text = copy_span_text(default_text, strlen(default_text));
    }
    column.data_type = copy_span_text(data_type == NULL ? "" : data_type,
                                      data_type == NULL ? 0U : strlen(data_type));
    column.extra = copy_span_text(extra == NULL ? "" : extra, extra == NULL ? 0U : strlen(extra));
    if (column.name == NULL || (default_text != NULL && column.default_text == NULL) ||
        column.data_type == NULL || column.extra == NULL) {
        insert_table_column_deinit(&column);
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    column.auto_increment = text_contains_word(column.extra, "auto_increment");
    column.generated_default = text_contains_word(column.extra, "DEFAULT_GENERATED");
    if (column.auto_increment) {
        table->has_auto_increment = true;
        table->auto_increment_column_index = table->column_count;
    }

    status = add_insert_table_column(table, column);
    if (status != MYLITE_OK) {
        insert_table_column_deinit(&column);
        if (status == MYLITE_NOMEM) {
            (void)set_error_message(stmt->database, "out of memory");
        }
    }
    return status;
}

static int add_insert_table_column(struct mylite_insert_table *table,
                                   struct mylite_insert_table_column column)
{
    struct mylite_insert_table_column *columns =
        realloc(table->columns, (table->column_count + 1U) * sizeof(*table->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    table->columns = columns;
    table->columns[table->column_count++] = column;
    return MYLITE_OK;
}

static int load_insert_unique_indexes(mylite_stmt *stmt, const char *schema_name,
                                      const char *table_name, struct mylite_insert_table *table)
{
    sqlite3_stmt *select = NULL;
    static const char sql[] = "SELECT index_name, column_name FROM __mylite_index_catalog "
                              "WHERE table_schema = ? AND table_name = ? AND non_unique = 0 "
                              "ORDER BY index_name, seq_in_index";
    int rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &select,
                                NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    sqlite3_bind_text(select, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(select, 2, table_name, -1, sqlite_transient_destructor());
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const struct mylite_insert_unique_index_part_name part = {
            .index_name = (const char *)sqlite3_column_text(select, 0),
            .column_name = (const char *)sqlite3_column_text(select, 1),
        };
        int status = add_insert_unique_index_part(stmt->database, table, &part);

        if (status != MYLITE_OK) {
            sqlite3_finalize(select);
            return status;
        }
    }
    sqlite3_finalize(select);
    return rc == SQLITE_DONE ? MYLITE_OK : set_sqlite_error(stmt->database);
}

static int add_insert_unique_index_part(struct mylite_db *database,
                                        struct mylite_insert_table *table,
                                        const struct mylite_insert_unique_index_part_name *part)
{
    struct mylite_insert_unique_index *index = NULL;
    size_t column_index = insert_table_column_index(table, part->column_name);

    if (column_index == table->column_count) {
        (void)set_error_message_parts(database, "Index references unknown column '",
                                      part->column_name, "'");
        return MYLITE_EXEC_ERROR;
    }

    for (size_t current = 0U; current < table->unique_index_count; ++current) {
        if (ascii_case_equal(table->unique_indexes[current].name, part->index_name)) {
            int status =
                append_insert_unique_index_part(&table->unique_indexes[current], column_index);

            if (status == MYLITE_NOMEM) {
                (void)set_error_message(database, "out of memory");
            }
            return status;
        }
    }

    index = realloc(table->unique_indexes,
                    (table->unique_index_count + 1U) * sizeof(*table->unique_indexes));
    if (index == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    table->unique_indexes = index;
    index = &table->unique_indexes[table->unique_index_count++];
    *index = (struct mylite_insert_unique_index){
        .is_primary = ascii_case_equal(part->index_name, "PRIMARY"),
    };
    index->name =
        copy_span_text(part->index_name, part->index_name == NULL ? 0U : strlen(part->index_name));
    if (index->name == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    int status = append_insert_unique_index_part(index, column_index);
    if (status == MYLITE_NOMEM) {
        (void)set_error_message(database, "out of memory");
    }
    return status;
}

static int append_insert_unique_index_part(struct mylite_insert_unique_index *index,
                                           size_t column_index)
{
    size_t *column_indexes =
        realloc(index->column_indexes, (index->column_count + 1U) * sizeof(*index->column_indexes));

    if (column_indexes == NULL) {
        return MYLITE_NOMEM;
    }

    index->column_indexes = column_indexes;
    index->column_indexes[index->column_count++] = column_index;
    return MYLITE_OK;
}

static int initialize_insert_auto_increment(mylite_stmt *stmt, struct mylite_insert_table *table,
                                            uint64_t catalog_auto_increment,
                                            bool has_catalog_auto_increment)
{
    uint64_t max_next_auto_increment = 1U;
    int status = MYLITE_OK;

    if (!table->has_auto_increment) {
        return MYLITE_OK;
    }

    status = read_insert_auto_increment_max(stmt, table, &max_next_auto_increment);
    if (status != MYLITE_OK) {
        return status;
    }
    table->next_auto_increment = max_next_auto_increment;
    if (has_catalog_auto_increment && catalog_auto_increment > table->next_auto_increment) {
        table->next_auto_increment = catalog_auto_increment;
    }
    if (table->next_auto_increment == 0U) {
        table->next_auto_increment = 1U;
    }
    return MYLITE_OK;
}

static int read_insert_auto_increment_max(mylite_stmt *stmt,
                                          const struct mylite_insert_table *table,
                                          uint64_t *out_next_auto_increment)
{
    sqlite3_stmt *select = NULL;
    sqlite3_str *sql = sqlite3_str_new(stmt->database->sqlite);
    char *select_sql = NULL;
    int rc = SQLITE_OK;

    *out_next_auto_increment = 1U;
    if (sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    sqlite3_str_appendf(sql, "SELECT max(\"%w\") FROM \"%w\"",
                        table->columns[table->auto_increment_column_index].name,
                        table->physical_name);
    select_sql = sqlite3_str_finish(sql);
    if (select_sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, select_sql, -1, SQLITE_PREPARE_PERSISTENT,
                            &select, NULL);
    sqlite3_free(select_sql);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    rc = sqlite3_step(select);
    if (rc == SQLITE_ROW && sqlite3_column_type(select, 0) != SQLITE_NULL) {
        sqlite3_int64 max_value = sqlite3_column_int64(select, 0);

        if (max_value >= 0) {
            *out_next_auto_increment = (uint64_t)max_value + 1U;
        }
    }
    sqlite3_finalize(select);
    return rc == SQLITE_ROW ? MYLITE_OK : set_sqlite_error(stmt->database);
}

static int validate_insert_column_list(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                       size_t **out_column_indexes)
{
    size_t *column_indexes = NULL;

    *out_column_indexes = NULL;
    if (!stmt->insert_values.has_column_list) {
        return MYLITE_OK;
    }
    if (stmt->insert_values.column_count == 0U) {
        return MYLITE_OK;
    }

    column_indexes = calloc(stmt->insert_values.column_count, sizeof(*column_indexes));
    if (column_indexes == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < stmt->insert_values.column_count; ++index) {
        size_t column_index = insert_table_column_index(table, stmt->insert_values.columns[index]);

        if (column_index == table->column_count) {
            int status =
                set_error_message_parts(stmt->database, "Unknown column '",
                                        stmt->insert_values.columns[index], "' in 'field list'");
            free(column_indexes);
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (column_indexes[previous] == column_index) {
                int status = set_error_message_parts(stmt->database, "Column '",
                                                     stmt->insert_values.columns[index],
                                                     "' specified twice");
                free(column_indexes);
                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
        column_indexes[index] = column_index;
    }

    *out_column_indexes = column_indexes;
    return MYLITE_OK;
}

static int execute_insert_values_transaction(mylite_stmt *stmt, const char *schema_name,
                                             const struct mylite_insert_table *table,
                                             const size_t *column_indexes)
{
    struct mylite_insert_execution_state state = {
        .next_auto_increment = table->next_auto_increment,
    };
    sqlite3_stmt *insert = NULL;
    char *insert_sql = NULL;
    int status = begin_sqlite_transaction(stmt->database);
    int rc = SQLITE_OK;

    if (status != MYLITE_OK) {
        return status;
    }

    insert_sql = build_insert_physical_sql(stmt->database, table);
    if (insert_sql == NULL) {
        rollback_sqlite_transaction(stmt->database);
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, insert_sql, -1, SQLITE_PREPARE_PERSISTENT,
                            &insert, NULL);
    sqlite3_free(insert_sql);
    if (rc != SQLITE_OK) {
        rollback_sqlite_transaction(stmt->database);
        return set_sqlite_error(stmt->database);
    }

    for (size_t row_index = 0U; row_index < stmt->insert_values.row_count; ++row_index) {
        status = execute_insert_row(stmt, insert, table, column_indexes, &state, row_index);
        if (status != MYLITE_OK) {
            break;
        }
    }
    sqlite3_finalize(insert);

    if (status != MYLITE_OK) {
        return finish_failed_insert_values_transaction(stmt, schema_name, table, &state, status);
    }

    if (table->has_auto_increment) {
        status = update_insert_auto_increment(stmt, schema_name,
                                              insert_auto_increment_next_value(&state));
    }
    if (status == MYLITE_OK) {
        status = commit_sqlite_transaction(stmt->database);
        if (status == MYLITE_OK) {
            stmt->affected_rows = (int64_t)stmt->insert_values.row_count;
            if (state.generated_insert_id) {
                stmt->database->last_insert_id = state.first_insert_id;
            }
            return MYLITE_OK;
        }
    }

    rollback_sqlite_transaction(stmt->database);
    return status;
}

static int finish_failed_insert_values_transaction(
    mylite_stmt *stmt, const char *schema_name, const struct mylite_insert_table *table,
    const struct mylite_insert_execution_state *state, int original_status)
{
    uint64_t next_auto_increment = insert_auto_increment_next_value(state);
    int status = MYLITE_OK;

    rollback_sqlite_transaction(stmt->database);
    if (table->has_auto_increment && next_auto_increment > table->next_auto_increment) {
        status = update_insert_auto_increment(stmt, schema_name, next_auto_increment);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (state->generated_insert_id) {
        stmt->database->last_insert_id = state->first_insert_id;
    }
    return original_status;
}

static char *build_insert_physical_sql(mylite_db *database, const struct mylite_insert_table *table)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "INSERT INTO \"%w\"(", table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\"", table->columns[index].name);
    }
    sqlite3_str_append(sql, ") VALUES(", (int)strlen(") VALUES("));
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_append(sql, "?", 1);
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

static int execute_insert_row(mylite_stmt *stmt, sqlite3_stmt *insert,
                              const struct mylite_insert_table *table, const size_t *column_indexes,
                              struct mylite_insert_execution_state *state, size_t row_index)
{
    struct mylite_insert_bound_value *values = NULL;
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (table->column_count == 0U) {
        (void)set_error_message(stmt->database, "INSERT target table has no columns");
        return MYLITE_EXEC_ERROR;
    }

    values = calloc(table->column_count, sizeof(*values));
    if (values == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = resolve_insert_row_values(stmt, table, column_indexes, state, row_index, values);
    if (status == MYLITE_OK) {
        status = validate_insert_unique_indexes(stmt, table, values);
    }
    if (status == MYLITE_OK) {
        sqlite3_reset(insert);
        sqlite3_clear_bindings(insert);
        status = bind_insert_row_values(stmt->database, insert, values, table->column_count);
    }
    if (status == MYLITE_OK) {
        rc = sqlite3_step(insert);
        if (rc != SQLITE_DONE) {
            status = set_sqlite_error(stmt->database);
        }
    }
    if (status == MYLITE_OK) {
        record_insert_row_auto_increment_id(table, values, state);
        status = advance_insert_row_auto_increment(table, values, state);
    }

    insert_bound_values_deinit(values, table->column_count);
    return status;
}

static void record_insert_row_auto_increment_id(const struct mylite_insert_table *table,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_execution_state *state)
{
    const struct mylite_insert_bound_value *auto_value = NULL;

    if (!table->has_auto_increment || state->generated_insert_id) {
        return;
    }

    auto_value = &values[table->auto_increment_column_index];
    if (auto_value->generated_auto_increment && auto_value->kind == MYLITE_INSERT_BOUND_INTEGER &&
        auto_value->integer_value > 0) {
        state->first_insert_id = (uint64_t)auto_value->integer_value;
        state->generated_insert_id = true;
    }
}

static int advance_insert_row_auto_increment(const struct mylite_insert_table *table,
                                             const struct mylite_insert_bound_value *values,
                                             struct mylite_insert_execution_state *state)
{
    const struct mylite_insert_bound_value *auto_value = NULL;

    if (!table->has_auto_increment) {
        return MYLITE_OK;
    }

    auto_value = &values[table->auto_increment_column_index];
    if (auto_value->kind == MYLITE_INSERT_BOUND_INTEGER && auto_value->integer_value > 0 &&
        (uint64_t)auto_value->integer_value >= state->next_auto_increment) {
        state->next_auto_increment = (uint64_t)auto_value->integer_value + 1U;
    }
    return MYLITE_OK;
}

static int resolve_insert_row_values(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                     const size_t *column_indexes,
                                     struct mylite_insert_execution_state *state, size_t row_index,
                                     struct mylite_insert_bound_value *values)
{
    const struct mylite_insert_row *row = &stmt->insert_values.rows[row_index];
    size_t expected_count = insert_row_target_column_count(&stmt->insert_values, table, row_index);

    if (row->value_count != expected_count) {
        return set_insert_wrong_value_count_error(stmt->database, row_index);
    }

    for (size_t column = 0U; column < table->column_count; ++column) {
        const struct mylite_insert_value *explicit_value = NULL;

        if (stmt->insert_values.has_column_list) {
            for (size_t target = 0U; target < stmt->insert_values.column_count; ++target) {
                if (column_indexes[target] == column) {
                    explicit_value = &row->values[target];
                    break;
                }
            }
        } else if (!insert_row_uses_all_defaults(&stmt->insert_values, row_index)) {
            explicit_value = &row->values[column];
        }

        if (explicit_value == NULL) {
            int status =
                resolve_insert_default_value(stmt, &table->columns[column], state, &values[column]);

            if (status != MYLITE_OK) {
                return status;
            }
        } else {
            int status = resolve_insert_explicit_value(stmt, &table->columns[column],
                                                       explicit_value, state, &values[column]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
    }
    return MYLITE_OK;
}

static int validate_insert_set_assignments(mylite_stmt *stmt,
                                           const struct mylite_insert_table *table,
                                           const char *schema_name, size_t **out_column_indexes)
{
    size_t assignment_count = stmt->insert_set.assignment_count;
    size_t *column_indexes = NULL;

    *out_column_indexes = NULL;
    if (assignment_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    column_indexes = calloc(assignment_count, sizeof(*column_indexes));
    if (column_indexes == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        const struct mylite_insert_column_reference *target =
            &stmt->insert_set.assignments[index].target;
        size_t column_index = insert_table_column_reference_index(
            table, schema_name, stmt->insert_values.table_name, target);

        if (column_index == table->column_count) {
            int status = set_error_message_parts(stmt->database, "Unknown column '",
                                                 target->column_name, "' in 'field list'");

            free(column_indexes);
            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        column_indexes[index] = column_index;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        for (size_t previous = 0U; previous < index; ++previous) {
            if (column_indexes[previous] == column_indexes[index]) {
                int status = set_error_message_parts(
                    stmt->database, "Column '",
                    stmt->insert_set.assignments[index].target.column_name, "' specified twice");

                free(column_indexes);
                return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
            }
        }
    }

    *out_column_indexes = column_indexes;
    return MYLITE_OK;
}

static int execute_insert_set_transaction(mylite_stmt *stmt, const char *schema_name,
                                          const struct mylite_insert_table *table,
                                          const size_t *column_indexes)
{
    struct mylite_insert_execution_state state = {
        .next_auto_increment = table->next_auto_increment,
    };
    struct mylite_insert_set_row_state row_state = {0};
    struct mylite_insert_bound_value *values = NULL;
    sqlite3_stmt *insert = NULL;
    char *insert_sql = NULL;
    int status = MYLITE_OK;
    int rc = SQLITE_OK;

    if (table->column_count == 0U) {
        (void)set_error_message(stmt->database, "INSERT target table has no columns");
        return MYLITE_EXEC_ERROR;
    }

    status = begin_sqlite_transaction(stmt->database);
    if (status != MYLITE_OK) {
        return status;
    }

    values = calloc(table->column_count, sizeof(*values));
    row_state.generate_auto_increment =
        calloc(table->column_count, sizeof(*row_state.generate_auto_increment));
    row_state.assigned_columns = calloc(table->column_count, sizeof(*row_state.assigned_columns));
    if (values == NULL || row_state.generate_auto_increment == NULL ||
        row_state.assigned_columns == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
        goto cleanup;
    }

    insert_sql = build_insert_physical_sql(stmt->database, table);
    if (insert_sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        status = MYLITE_NOMEM;
        goto cleanup;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, insert_sql, -1, SQLITE_PREPARE_PERSISTENT,
                            &insert, NULL);
    sqlite3_free(insert_sql);
    insert_sql = NULL;
    if (rc != SQLITE_OK) {
        status = set_sqlite_error(stmt->database);
        goto cleanup;
    }

    status = initialize_insert_set_row_values(stmt, table, &state, values, &row_state);
    if (status == MYLITE_OK) {
        status = apply_insert_set_assignments(stmt, table, column_indexes, values, &row_state);
    }
    if (status == MYLITE_OK) {
        status = finish_insert_set_row_values(stmt, table, &state, values, &row_state);
    }
    if (status == MYLITE_OK) {
        status = validate_insert_unique_indexes(stmt, table, values);
    }
    if (status == MYLITE_OK) {
        status = bind_insert_row_values(stmt->database, insert, values, table->column_count);
    }
    if (status == MYLITE_OK) {
        rc = sqlite3_step(insert);
        if (rc != SQLITE_DONE) {
            status = set_sqlite_error(stmt->database);
        }
    }
    if (status == MYLITE_OK) {
        record_insert_row_auto_increment_id(table, values, &state);
        status = advance_insert_row_auto_increment(table, values, &state);
    }

cleanup:
    sqlite3_free(insert_sql);
    sqlite3_finalize(insert);
    insert_bound_values_deinit(values, table->column_count);
    free(row_state.generate_auto_increment);
    free(row_state.assigned_columns);

    if (status != MYLITE_OK) {
        return finish_failed_insert_values_transaction(stmt, schema_name, table, &state, status);
    }

    if (table->has_auto_increment) {
        status = update_insert_auto_increment(stmt, schema_name,
                                              insert_auto_increment_next_value(&state));
    }
    if (status == MYLITE_OK) {
        status = commit_sqlite_transaction(stmt->database);
        if (status == MYLITE_OK) {
            stmt->affected_rows = 1;
            if (state.generated_insert_id) {
                stmt->database->last_insert_id = state.first_insert_id;
            }
            return MYLITE_OK;
        }
    }

    rollback_sqlite_transaction(stmt->database);
    return status;
}

static int initialize_insert_set_row_values(mylite_stmt *stmt,
                                            const struct mylite_insert_table *table,
                                            struct mylite_insert_execution_state *state,
                                            struct mylite_insert_bound_value *values,
                                            struct mylite_insert_set_row_state *row_state)
{
    for (size_t column = 0U; column < table->column_count; ++column) {
        int status = MYLITE_OK;

        if (table->columns[column].auto_increment) {
            status = set_insert_set_candidate_auto_value(&values[column]);
            row_state->generate_auto_increment[column] = true;
        } else if (table->columns[column].default_text != NULL || table->columns[column].nullable) {
            status =
                resolve_insert_default_value(stmt, &table->columns[column], state, &values[column]);
        } else {
            status = resolve_insert_implicit_expression_default(stmt, &table->columns[column],
                                                                &values[column]);
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_insert_set_assignments(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                        const size_t *column_indexes,
                                        struct mylite_insert_bound_value *values,
                                        struct mylite_insert_set_row_state *row_state)
{
    for (size_t index = 0U; index < stmt->insert_set.assignment_count; ++index) {
        size_t column_index = column_indexes[index];
        struct mylite_insert_bound_value value = {0};
        bool generate_auto = false;
        int status = evaluate_insert_set_assignment_value(
            stmt, table, column_index, &stmt->insert_set.assignments[index].value, values,
            &generate_auto, &value);

        if (status != MYLITE_OK) {
            insert_bound_value_deinit(&value);
            return status;
        }

        insert_bound_value_deinit(&values[column_index]);
        values[column_index] = value;
        row_state->generate_auto_increment[column_index] = generate_auto;
        row_state->assigned_columns[column_index] = true;
    }
    return MYLITE_OK;
}

static int finish_insert_set_row_values(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                        struct mylite_insert_execution_state *state,
                                        struct mylite_insert_bound_value *values,
                                        const struct mylite_insert_set_row_state *row_state)
{
    for (size_t column = 0U; column < table->column_count; ++column) {
        const struct mylite_insert_table_column *table_column = &table->columns[column];

        if (!table_column->auto_increment && !table_column->nullable &&
            table_column->default_text == NULL && !row_state->assigned_columns[column]) {
            return set_insert_no_default_error(stmt->database, table_column->name);
        }
        if (table_column->auto_increment && row_state->generate_auto_increment[column]) {
            insert_bound_value_deinit(&values[column]);
            int status = allocate_insert_auto_increment(stmt, state, &values[column]);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        if (!table_column->auto_increment && !table_column->nullable &&
            values[column].kind == MYLITE_INSERT_BOUND_NULL) {
            return set_insert_null_error(stmt->database, table_column->name);
        }
    }
    return MYLITE_OK;
}

static int evaluate_insert_set_assignment_value(
    mylite_stmt *stmt, const struct mylite_insert_table *table, size_t target_column,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *values,
    bool *out_generate_auto_increment, struct mylite_insert_bound_value *out_value)
{
    const struct mylite_insert_table_column *column = &table->columns[target_column];
    int status = MYLITE_OK;

    *out_generate_auto_increment = false;
    if (value->kind == MYLITE_INSERT_VALUE_DEFAULT) {
        if (column->auto_increment) {
            *out_generate_auto_increment = true;
            return set_insert_set_candidate_auto_value(out_value);
        }
        return resolve_insert_default_value(stmt, column, NULL, out_value);
    }

    status = evaluate_insert_set_expression(stmt, table, value, values, out_value);
    if (status != MYLITE_OK) {
        return status;
    }

    if (column->auto_increment) {
        if (out_value->kind == MYLITE_INSERT_BOUND_NULL ||
            (out_value->kind == MYLITE_INSERT_BOUND_INTEGER && out_value->integer_value == 0)) {
            insert_bound_value_deinit(out_value);
            *out_generate_auto_increment = true;
            return set_insert_set_candidate_auto_value(out_value);
        }
        if (out_value->kind != MYLITE_INSERT_BOUND_INTEGER || out_value->integer_value < 0) {
            insert_bound_value_deinit(out_value);
            return set_insert_unsupported_expression_error(stmt->database);
        }
    } else if (!column->nullable && out_value->kind == MYLITE_INSERT_BOUND_NULL) {
        insert_bound_value_deinit(out_value);
        return set_insert_null_error(stmt->database, column->name);
    }
    return MYLITE_OK;
}

static int evaluate_insert_set_expression(mylite_stmt *stmt,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_value *value,
                                          const struct mylite_insert_bound_value *values,
                                          struct mylite_insert_bound_value *out_value)
{
    if (value->kind == MYLITE_INSERT_VALUE_UNARY_EXPRESSION) {
        return evaluate_insert_set_unary_expression(stmt, table, value, values, out_value);
    }
    if (value->kind == MYLITE_INSERT_VALUE_BINARY_EXPRESSION) {
        return evaluate_insert_set_binary_expression(stmt, table, value, values, out_value);
    }
    return evaluate_insert_set_simple_expression(stmt, table, value, values, out_value);
}

static int evaluate_insert_set_unary_expression(mylite_stmt *stmt,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_value *value,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value)
{
    struct mylite_insert_bound_value operand = {0};
    double numeric_value = 0.0;
    bool is_integer = false;
    int status = evaluate_insert_set_simple_expression(stmt, table, value->left, values, &operand);

    if (status != MYLITE_OK) {
        insert_bound_value_deinit(&operand);
        return status;
    }
    if (operand.kind == MYLITE_INSERT_BOUND_NULL) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        insert_bound_value_deinit(&operand);
        return MYLITE_OK;
    }
    if (!insert_bound_value_is_numeric(&operand, &numeric_value, &is_integer)) {
        insert_bound_value_deinit(&operand);
        return set_insert_unsupported_expression_error(stmt->database);
    }

    if (is_integer) {
        int64_t integer_value = operand.kind == MYLITE_INSERT_BOUND_INTEGER
                                    ? operand.integer_value
                                    : (int64_t)numeric_value;

        out_value->kind = MYLITE_INSERT_BOUND_INTEGER;
        out_value->integer_value = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE
                                       ? -integer_value
                                       : integer_value;
    } else {
        out_value->kind = MYLITE_INSERT_BOUND_REAL;
        out_value->real_value = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE
                                    ? -numeric_value
                                    : numeric_value;
    }
    insert_bound_value_deinit(&operand);
    return MYLITE_OK;
}

static int evaluate_insert_set_binary_expression(mylite_stmt *stmt,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value)
{
    struct mylite_insert_bound_value left = {0};
    struct mylite_insert_bound_value right = {0};
    double left_number = 0.0;
    double right_number = 0.0;
    bool left_is_integer = false;
    bool right_is_integer = false;
    int status = evaluate_insert_set_simple_expression(stmt, table, value->left, values, &left);

    if (status == MYLITE_OK) {
        status = evaluate_insert_set_simple_expression(stmt, table, value->right, values, &right);
    }
    if (status != MYLITE_OK) {
        insert_bound_value_deinit(&left);
        insert_bound_value_deinit(&right);
        return status;
    }
    if (left.kind == MYLITE_INSERT_BOUND_NULL || right.kind == MYLITE_INSERT_BOUND_NULL) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        insert_bound_value_deinit(&left);
        insert_bound_value_deinit(&right);
        return MYLITE_OK;
    }
    if (!insert_bound_value_is_numeric(&left, &left_number, &left_is_integer) ||
        !insert_bound_value_is_numeric(&right, &right_number, &right_is_integer)) {
        insert_bound_value_deinit(&left);
        insert_bound_value_deinit(&right);
        return set_insert_unsupported_expression_error(stmt->database);
    }

    if (left_is_integer && right_is_integer &&
        value->operator_kind != MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        int64_t left_int =
            left.kind == MYLITE_INSERT_BOUND_INTEGER ? left.integer_value : (int64_t)left_number;
        int64_t right_int =
            right.kind == MYLITE_INSERT_BOUND_INTEGER ? right.integer_value : (int64_t)right_number;

        out_value->kind = MYLITE_INSERT_BOUND_INTEGER;
        switch (value->operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->integer_value = left_int + right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->integer_value = left_int - right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->integer_value = left_int * right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        case MYLITE_SQL_AST_OPERATOR_NONE:
        case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        default:
            insert_bound_value_deinit(&left);
            insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(stmt->database);
        }
    } else {
        if (value->operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE && right_number == 0.0) {
            insert_bound_value_deinit(&left);
            insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(stmt->database);
        }
        out_value->kind = MYLITE_INSERT_BOUND_REAL;
        switch (value->operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->real_value = left_number + right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->real_value = left_number - right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->real_value = left_number * right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_DIVIDE:
            out_value->real_value = left_number / right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_NONE:
        case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        default:
            insert_bound_value_deinit(&left);
            insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(stmt->database);
        }
    }

    insert_bound_value_deinit(&left);
    insert_bound_value_deinit(&right);
    return MYLITE_OK;
}

static int evaluate_insert_set_simple_expression(mylite_stmt *stmt,
                                                 const struct mylite_insert_table *table,
                                                 const struct mylite_insert_value *value,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_bound_value *out_value)
{
    int64_t integer_value = 0;
    double real_value = 0.0;
    char *timestamp = NULL;

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_NULL:
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_INTEGER:
        if (!parse_insert_integer_text(value->text, &integer_value)) {
            return set_insert_unsupported_expression_error(stmt->database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_REAL:
        if (!parse_insert_real_text(value->text, &real_value)) {
            return set_insert_unsupported_expression_error(stmt->database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_TEXT:
        out_value->text_value =
            copy_span_text(value->text, value->text == NULL ? 0U : strlen(value->text));
        if (out_value->text_value == NULL) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_INSERT_BOUND_TEXT;
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        timestamp = insert_current_timestamp_text();
        if (timestamp == NULL) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE:
        return evaluate_insert_set_column_reference(stmt, table, &value->column_reference, values,
                                                    out_value);
    case MYLITE_INSERT_VALUE_DEFAULT:
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION:
        return set_insert_unsupported_expression_error(stmt->database);
    }

    return set_insert_unsupported_expression_error(stmt->database);
}

static int evaluate_insert_set_column_reference(mylite_stmt *stmt,
                                                const struct mylite_insert_table *table,
                                                const struct mylite_insert_column_reference *ref,
                                                const struct mylite_insert_bound_value *values,
                                                struct mylite_insert_bound_value *out_value)
{
    size_t column_index = insert_table_column_reference_index(
        table,
        stmt->insert_values.schema_name == NULL ? stmt->database->selected_schema
                                                : stmt->insert_values.schema_name,
        stmt->insert_values.table_name, ref);

    if (column_index == table->column_count) {
        int status = set_error_message_parts(stmt->database, "Unknown column '", ref->column_name,
                                             "' in 'field list'");

        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    int status = copy_insert_bound_value(&values[column_index], out_value);

    if (status == MYLITE_NOMEM) {
        (void)set_error_message(stmt->database, "out of memory");
    }
    return status;
}

static int copy_insert_bound_value(const struct mylite_insert_bound_value *value,
                                   struct mylite_insert_bound_value *out_value)
{
    *out_value = *value;
    out_value->text_value = NULL;
    if (value->kind == MYLITE_INSERT_BOUND_TEXT && value->text_value != NULL) {
        out_value->text_value = copy_span_text(value->text_value, strlen(value->text_value));
        if (out_value->text_value == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static bool insert_bound_value_is_numeric(const struct mylite_insert_bound_value *value,
                                          double *out_value, bool *out_is_integer)
{
    int64_t integer_value = 0;

    *out_value = 0.0;
    *out_is_integer = false;
    if (value->kind == MYLITE_INSERT_BOUND_INTEGER) {
        *out_value = (double)value->integer_value;
        *out_is_integer = true;
        return true;
    }
    if (value->kind == MYLITE_INSERT_BOUND_REAL) {
        *out_value = value->real_value;
        return true;
    }
    if (value->kind == MYLITE_INSERT_BOUND_TEXT &&
        parse_insert_integer_text(value->text_value, &integer_value)) {
        *out_value = (double)integer_value;
        *out_is_integer = true;
        return true;
    }
    if (value->kind == MYLITE_INSERT_BOUND_TEXT &&
        parse_insert_real_text(value->text_value, out_value)) {
        return true;
    }
    return false;
}

static int
resolve_insert_implicit_expression_default(mylite_stmt *stmt,
                                           const struct mylite_insert_table_column *column,
                                           struct mylite_insert_bound_value *out_value)
{
    if (insert_column_uses_numeric_implicit_default(column)) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = 0,
        };
        return MYLITE_OK;
    }

    out_value->text_value = copy_span_text("", 0U);
    if (out_value->text_value == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    return MYLITE_OK;
}

static int set_insert_set_candidate_auto_value(struct mylite_insert_bound_value *out_value)
{
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_INTEGER,
        .integer_value = 0,
    };
    return MYLITE_OK;
}

static int resolve_insert_explicit_value(mylite_stmt *stmt,
                                         const struct mylite_insert_table_column *column,
                                         const struct mylite_insert_value *value,
                                         struct mylite_insert_execution_state *state,
                                         struct mylite_insert_bound_value *out_value)
{
    char *timestamp = NULL;

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_DEFAULT:
        return resolve_insert_default_value(stmt, column, state, out_value);
    case MYLITE_INSERT_VALUE_NULL:
        if (column->auto_increment) {
            return allocate_insert_auto_increment(stmt, state, out_value);
        }
        if (!column->nullable) {
            return set_insert_null_error(stmt->database, column->name);
        }
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_INTEGER:
        return resolve_insert_text_value(stmt, column, value->text, state, out_value);
    case MYLITE_INSERT_VALUE_REAL:
        if (column->auto_increment) {
            return set_insert_unsupported_expression_error(stmt->database);
        }
        return resolve_insert_text_value(stmt, column, value->text, state, out_value);
    case MYLITE_INSERT_VALUE_TEXT:
        return resolve_insert_text_value(stmt, column, value->text, state, out_value);
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        if (column->auto_increment) {
            return set_insert_unsupported_expression_error(stmt->database);
        }
        timestamp = insert_current_timestamp_text();
        if (timestamp == NULL) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE:
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION:
        return set_insert_unsupported_expression_error(stmt->database);
    }

    return set_insert_unsupported_expression_error(stmt->database);
}

static int resolve_insert_default_value(mylite_stmt *stmt,
                                        const struct mylite_insert_table_column *column,
                                        struct mylite_insert_execution_state *state,
                                        struct mylite_insert_bound_value *out_value)
{
    if (column->auto_increment) {
        return allocate_insert_auto_increment(stmt, state, out_value);
    }
    if (column->default_text == NULL) {
        if (column->nullable) {
            *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
            return MYLITE_OK;
        }
        return set_insert_no_default_error(stmt->database, column->name);
    }
    if (column_default_is_current_timestamp(column->default_text)) {
        char *timestamp = insert_current_timestamp_text();

        if (timestamp == NULL) {
            (void)set_error_message(stmt->database, "out of memory");
            return MYLITE_NOMEM;
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_TEXT,
            .text_value = timestamp,
        };
        return MYLITE_OK;
    }
    if (column->generated_default) {
        return set_insert_unsupported_generated_default_error(stmt->database, column->name);
    }
    return resolve_insert_text_value(stmt, column, column->default_text, state, out_value);
}

static int resolve_insert_text_value(mylite_stmt *stmt,
                                     const struct mylite_insert_table_column *column,
                                     const char *text, struct mylite_insert_execution_state *state,
                                     struct mylite_insert_bound_value *out_value)
{
    int64_t integer_value = 0;
    double real_value = 0.0;

    if (text == NULL) {
        if (!column->nullable) {
            return set_insert_null_error(stmt->database, column->name);
        }
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    }
    if (parse_insert_integer_text(text, &integer_value)) {
        if (column->auto_increment && integer_value == 0) {
            return allocate_insert_auto_increment(stmt, state, out_value);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    }
    if (column->auto_increment) {
        return set_insert_unsupported_expression_error(stmt->database);
    }
    if (parse_insert_real_text(text, &real_value)) {
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        return MYLITE_OK;
    }

    out_value->text_value = copy_span_text(text, strlen(text));
    if (out_value->text_value == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_value->kind = MYLITE_INSERT_BOUND_TEXT;
    return MYLITE_OK;
}

static int allocate_insert_auto_increment(mylite_stmt *stmt,
                                          struct mylite_insert_execution_state *state,
                                          struct mylite_insert_bound_value *out_value)
{
    uint64_t value = state->next_auto_increment == 0U ? 1U : state->next_auto_increment;
    int status = MYLITE_OK;

    if (value > (uint64_t)INT64_MAX) {
        (void)set_error_message(stmt->database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    status = reserve_insert_auto_increment(stmt, state, value);
    if (status != MYLITE_OK) {
        return status;
    }
    state->next_auto_increment = value + 1U;
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_INTEGER,
        .integer_value = (int64_t)value,
        .generated_auto_increment = true,
    };
    return MYLITE_OK;
}

static int reserve_insert_auto_increment(mylite_stmt *stmt,
                                         struct mylite_insert_execution_state *state,
                                         uint64_t first_value)
{
    uint64_t row_count = insert_statement_row_count(stmt);

    if (state->reserved_auto_increment_end != 0U) {
        return MYLITE_OK;
    }
    if (row_count > (uint64_t)INT64_MAX - first_value) {
        (void)set_error_message(stmt->database, "AUTO_INCREMENT value is out of range");
        return MYLITE_EXEC_ERROR;
    }
    state->reserved_auto_increment_end = first_value + row_count;
    return MYLITE_OK;
}

static uint64_t insert_statement_row_count(const mylite_stmt *stmt)
{
    if (stmt->kind == MYLITE_STMT_INSERT_SET) {
        return 1U;
    }
    return (uint64_t)stmt->insert_values.row_count;
}

static uint64_t insert_auto_increment_next_value(const struct mylite_insert_execution_state *state)
{
    if (state->reserved_auto_increment_end > state->next_auto_increment) {
        return state->reserved_auto_increment_end;
    }
    return state->next_auto_increment;
}

static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference)
{
    if (!insert_column_reference_qualifiers_match(reference, schema_name, table_name)) {
        return table->column_count;
    }
    return insert_table_column_index(table, reference->column_name);
}

static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name)
{
    if (reference->schema_name != NULL && !ascii_case_equal(reference->schema_name, schema_name)) {
        return false;
    }
    if (reference->table_name != NULL && !ascii_case_equal(reference->table_name, table_name)) {
        return false;
    }
    return true;
}

static int bind_insert_row_values(mylite_db *database, sqlite3_stmt *insert,
                                  const struct mylite_insert_bound_value *values,
                                  size_t value_count)
{
    (void)database;
    for (size_t index = 0U; index < value_count; ++index) {
        int rc = bind_insert_bound_value(insert, (int)index + 1, &values[index]);

        if (rc != SQLITE_OK) {
            return set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static int bind_insert_bound_value(sqlite3_stmt *stmt, int index,
                                   const struct mylite_insert_bound_value *value)
{
    switch (value->kind) {
    case MYLITE_INSERT_BOUND_NULL:
        return sqlite3_bind_null(stmt, index);
    case MYLITE_INSERT_BOUND_INTEGER:
        return sqlite3_bind_int64(stmt, index, (sqlite3_int64)value->integer_value);
    case MYLITE_INSERT_BOUND_REAL:
        return sqlite3_bind_double(stmt, index, value->real_value);
    case MYLITE_INSERT_BOUND_TEXT:
        return sqlite3_bind_text(stmt, index, value->text_value, -1, sqlite_transient_destructor());
    }

    return SQLITE_MISUSE;
}

static int validate_insert_unique_indexes(mylite_stmt *stmt,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_bound_value *values)
{
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        bool conflicts = false;
        int status = insert_unique_index_conflicts(stmt, table, &table->unique_indexes[index],
                                                   values, &conflicts);

        if (status != MYLITE_OK) {
            return status;
        }
        if (conflicts) {
            return set_insert_duplicate_entry_error(stmt->database, stmt->insert_values.table_name,
                                                    &table->unique_indexes[index], values);
        }
    }
    return MYLITE_OK;
}

static int insert_unique_index_conflicts(mylite_stmt *stmt, const struct mylite_insert_table *table,
                                         const struct mylite_insert_unique_index *index,
                                         const struct mylite_insert_bound_value *values,
                                         bool *out_conflicts)
{
    char *sql = NULL;
    sqlite3_stmt *check = NULL;
    int rc = SQLITE_OK;
    int status = MYLITE_OK;

    *out_conflicts = false;
    for (size_t part = 0U; part < index->column_count; ++part) {
        if (values[index->column_indexes[part]].kind == MYLITE_INSERT_BOUND_NULL) {
            return MYLITE_OK;
        }
    }

    sql = build_insert_unique_check_sql(stmt->database, table, index, values);
    if (sql == NULL) {
        (void)set_error_message(stmt->database, "out of memory");
        return MYLITE_NOMEM;
    }

    rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &check,
                            NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    status = bind_insert_unique_check_values(stmt->database, check, index, values);
    if (status == MYLITE_OK) {
        rc = sqlite3_step(check);
        if (rc == SQLITE_ROW) {
            *out_conflicts = true;
        } else if (rc != SQLITE_DONE) {
            status = set_sqlite_error(stmt->database);
        }
    }
    sqlite3_finalize(check);
    return status;
}

static char *build_insert_unique_check_sql(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_insert_bound_value *values)
{
    sqlite3_str *sql = sqlite3_str_new(database->sqlite);

    (void)values;
    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "SELECT 1 FROM \"%w\" WHERE ", table->physical_name);
    for (size_t part = 0U; part < index->column_count; ++part) {
        size_t column_index = index->column_indexes[part];

        if (part != 0U) {
            sqlite3_str_append(sql, " AND ", (int)strlen(" AND "));
        }
        sqlite3_str_appendf(sql, "\"%w\" = ?", table->columns[column_index].name);
    }
    sqlite3_str_append(sql, " LIMIT 1", (int)strlen(" LIMIT 1"));
    return sqlite3_str_finish(sql);
}

static int bind_insert_unique_check_values(mylite_db *database, sqlite3_stmt *check,
                                           const struct mylite_insert_unique_index *index,
                                           const struct mylite_insert_bound_value *values)
{
    for (size_t part = 0U; part < index->column_count; ++part) {
        int rc =
            bind_insert_bound_value(check, (int)part + 1, &values[index->column_indexes[part]]);

        if (rc != SQLITE_OK) {
            return set_sqlite_error(database);
        }
    }
    return MYLITE_OK;
}

static int update_insert_auto_increment(mylite_stmt *stmt, const char *schema_name,
                                        uint64_t next_auto_increment)
{
    return update_table_auto_increment(stmt, schema_name, stmt->insert_values.table_name,
                                       next_auto_increment);
}

static int update_table_auto_increment(mylite_stmt *stmt, const char *schema_name,
                                       const char *table_name, uint64_t next_auto_increment)
{
    sqlite3_stmt *update = NULL;
    static const char sql[] = "UPDATE __mylite_table_catalog SET auto_increment = ? "
                              "WHERE table_schema = ? AND table_name = ?";
    int rc = sqlite3_prepare_v3(stmt->database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &update,
                                NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(stmt->database);
    }

    sqlite3_bind_int64(update, 1, (sqlite3_int64)next_auto_increment);
    sqlite3_bind_text(update, 2, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(update, 3, table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(update);
    sqlite3_finalize(update);
    return rc == SQLITE_DONE ? MYLITE_OK : set_sqlite_error(stmt->database);
}

static bool insert_row_uses_all_defaults(const struct mylite_insert_values_plan *plan,
                                         size_t row_index)
{
    if (plan->has_column_list) {
        return false;
    }
    if (plan->rows[row_index].value_count == 0U) {
        return true;
    }
    return false;
}

static size_t insert_row_target_column_count(const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             size_t row_index)
{
    if (plan->has_column_list) {
        return plan->column_count;
    }
    if (plan->rows[row_index].value_count == 0U) {
        return 0U;
    }
    return table->column_count;
}

static int set_insert_wrong_value_count_error(mylite_db *database, size_t row_index)
{
    enum { row_number_buffer_size = 64 };
    char buffer[row_number_buffer_size];

    (void)snprintf(buffer, sizeof(buffer), "%zu", row_index + 1U);
    if (set_error_message_parts(database, "Column count doesn't match value count at row ", buffer,
                                "") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int set_insert_no_default_error(mylite_db *database, const char *column_name)
{
    int status =
        set_error_message_parts(database, "Field '", column_name, "' doesn't have a default value");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_null_error(mylite_db *database, const char *column_name)
{
    int status = set_error_message_parts(database, "Column '", column_name, "' cannot be null");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_unsupported_generated_default_error(mylite_db *database,
                                                          const char *column_name)
{
    int status = set_error_message_parts(database, "Unsupported generated default expression for '",
                                         column_name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_unsupported_expression_error(mylite_db *database)
{
    if (set_error_message(database, "Unsupported INSERT value expression") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int set_insert_duplicate_entry_error(mylite_db *database, const char *table_name,
                                            const struct mylite_insert_unique_index *index,
                                            const struct mylite_insert_bound_value *values)
{
    char *entry = copy_insert_duplicate_entry_value(index, values);
    char *message = NULL;
    int status = MYLITE_OK;

    if (entry == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    message =
        sqlite3_mprintf("Duplicate entry '%q' for key '%q.%q'", entry, table_name, index->name);
    free(entry);
    if (message == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static char *copy_insert_duplicate_entry_value(const struct mylite_insert_unique_index *index,
                                               const struct mylite_insert_bound_value *values)
{
    sqlite3_str *text = sqlite3_str_new(NULL);

    if (text == NULL) {
        return NULL;
    }

    for (size_t part = 0U; part < index->column_count; ++part) {
        const struct mylite_insert_bound_value *value = &values[index->column_indexes[part]];

        if (part != 0U) {
            sqlite3_str_append(text, "-", 1);
        }
        switch (value->kind) {
        case MYLITE_INSERT_BOUND_NULL:
            sqlite3_str_append(text, "NULL", (int)strlen("NULL"));
            break;
        case MYLITE_INSERT_BOUND_INTEGER:
            sqlite3_str_appendf(text, "%lld", (long long)value->integer_value);
            break;
        case MYLITE_INSERT_BOUND_REAL:
            sqlite3_str_appendf(text, "%.15g", value->real_value);
            break;
        case MYLITE_INSERT_BOUND_TEXT:
            sqlite3_str_append(text, value->text_value == NULL ? "" : value->text_value,
                               value->text_value == NULL ? 0 : (int)strlen(value->text_value));
            break;
        }
    }
    return sqlite3_str_finish(text);
}

static int set_table_doesnt_exist_error(mylite_db *database, const char *schema_name,
                                        const char *table_name)
{
    char *message = sqlite3_mprintf("Table '%q.%q' doesn't exist", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_names_connection_state(mylite_db *database,
                                      struct mylite_connection_charset_request request)
{
    const struct mylite_charset *character_set = mylite_charset_lookup(request.character_set_name);
    const struct mylite_collation *collation = NULL;

    if (character_set == NULL) {
        return set_unknown_charset_error(database, request.character_set_name);
    }

    if (request.collation_name == NULL) {
        collation = mylite_collation_lookup(character_set->default_collation);
    } else {
        collation = mylite_collation_lookup(request.collation_name);
        if (collation == NULL) {
            return set_unknown_collation_error(database, request.collation_name);
        }
        if (!mylite_charset_collation_match(character_set, collation)) {
            return set_collation_charset_error(database, collation->name, character_set->name);
        }
    }

    database->character_set_client = character_set->name;
    database->character_set_connection = character_set->name;
    database->character_set_results = character_set->name;
    database->collation_connection = collation->name;
    return MYLITE_OK;
}

static int set_character_set_connection_state(mylite_db *database, const char *character_set_name)
{
    struct mylite_schema_default schema_default;
    const struct mylite_charset *character_set = mylite_charset_lookup(character_set_name);
    const struct mylite_collation *connection_collation = NULL;
    int status = MYLITE_OK;

    if (character_set == NULL) {
        return set_unknown_charset_error(database, character_set_name);
    }

    status = selected_schema_default(database, &schema_default);
    if (status != MYLITE_OK) {
        return status;
    }

    connection_collation = mylite_collation_lookup(schema_default.collation);
    if (connection_collation == NULL) {
        return set_unknown_collation_error(database, schema_default.collation);
    }

    database->character_set_client = character_set->name;
    database->character_set_connection = connection_collation->character_set;
    database->character_set_results = character_set->name;
    database->collation_connection = connection_collation->name;
    return MYLITE_OK;
}

static int set_default_connection_state(mylite_db *database)
{
    database->character_set_client = mylite_charset_default_name();
    database->character_set_connection = mylite_charset_default_name();
    database->character_set_results = mylite_charset_default_name();
    database->collation_connection = mylite_charset_default_collation_name();
    return MYLITE_OK;
}

static int selected_schema_default(mylite_db *database, struct mylite_schema_default *out_default)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT default_character_set, default_collation FROM __mylite_schema_catalog "
        "WHERE name = ?";
    int rc = SQLITE_OK;

    *out_default = (struct mylite_schema_default){
        .character_set = mylite_charset_default_name(),
        .collation = mylite_charset_default_collation_name(),
    };
    if (database->selected_schema == NULL) {
        return MYLITE_OK;
    }

    rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, database->selected_schema, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *character_set = (const char *)sqlite3_column_text(stmt, 0);
        const char *collation = (const char *)sqlite3_column_text(stmt, 1);
        const struct mylite_charset *character_set_entry = mylite_charset_lookup(character_set);
        const struct mylite_collation *collation_entry = mylite_collation_lookup(collation);

        if (character_set_entry == NULL) {
            int status = set_unknown_charset_error(database, character_set);
            sqlite3_finalize(stmt);
            return status;
        }
        if (collation_entry == NULL) {
            int status = set_unknown_collation_error(database, collation);
            sqlite3_finalize(stmt);
            return status;
        }
        sqlite3_finalize(stmt);
        *out_default = (struct mylite_schema_default){
            .character_set = character_set_entry->name,
            .collation = collation_entry->name,
        };
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }

    if (set_error_message(database, "Selected schema default charset is unavailable") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

static int schema_exists(mylite_db *database, const char *schema_name,
                         struct mylite_schema_presence *out_presence)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] = "SELECT is_system FROM __mylite_schema_catalog WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    *out_presence = (struct mylite_schema_presence){
        .exists = false,
        .is_system = false,
    };
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_presence = (struct mylite_schema_presence){
            .exists = true,
            .is_system = sqlite3_column_int(stmt, 0) != 0,
        };
        sqlite3_finalize(stmt);
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int table_exists(mylite_db *database, const char *schema_name, const char *table_name,
                        bool *out_exists)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT 1 FROM __mylite_table_catalog WHERE table_schema = ? AND table_name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    *out_exists = false;
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, table_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_exists = true;
        sqlite3_finalize(stmt);
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int schema_default_by_name(mylite_db *database, const char *schema_name,
                                  struct mylite_schema_default *out_default)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "SELECT default_character_set, default_collation FROM __mylite_schema_catalog "
        "WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    *out_default = (struct mylite_schema_default){
        .character_set = mylite_charset_default_name(),
        .collation = mylite_charset_default_collation_name(),
    };
    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *character_set = (const char *)sqlite3_column_text(stmt, 0);
        const char *collation = (const char *)sqlite3_column_text(stmt, 1);
        const struct mylite_charset *character_set_entry = mylite_charset_lookup(character_set);
        const struct mylite_collation *collation_entry = mylite_collation_lookup(collation);

        if (character_set_entry == NULL) {
            int status = set_unknown_charset_error(database, character_set);
            sqlite3_finalize(stmt);
            return status;
        }
        if (collation_entry == NULL) {
            int status = set_unknown_collation_error(database, collation);
            sqlite3_finalize(stmt);
            return status;
        }
        *out_default = (struct mylite_schema_default){
            .character_set = character_set_entry->name,
            .collation = collation_entry->name,
        };
        sqlite3_finalize(stmt);
        return MYLITE_OK;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    (void)set_error_message_parts(database, "Unknown database '", schema_name, "'");
    return MYLITE_EXEC_ERROR;
}

static int insert_schema(mylite_db *database, const char *schema_name,
                         const struct mylite_schema_options *options)
{
    enum { bind_read_only = 5 };
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "INSERT INTO __mylite_schema_catalog("
        "name, default_character_set, default_collation, default_encryption, read_only, is_system)"
        " VALUES(?, ?, ?, ?, ?, 0)";
    const char *character_set =
        options->character_set == NULL ? mylite_charset_default_name() : options->character_set;
    const char *collation =
        options->collation == NULL ? mylite_charset_default_collation_name() : options->collation;
    const char *encryption = options->encryption == NULL ? "N" : options->encryption;
    int read_only = 0;
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    if (options->has_read_only) {
        read_only = options->read_only;
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 2, character_set, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 3, collation, -1, sqlite_transient_destructor());
    sqlite3_bind_text(stmt, 4, encryption, -1, sqlite_transient_destructor());
    sqlite3_bind_int(stmt, bind_read_only, read_only);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int update_schema(mylite_db *database, const char *schema_name,
                         const struct mylite_schema_options *options)
{
    enum {
        bind_has_read_only = 4,
        bind_read_only = 5,
        bind_schema_name = 6,
    };
    sqlite3_stmt *stmt = NULL;
    int has_read_only = 0;
    static const char sql[] = "UPDATE __mylite_schema_catalog SET "
                              "default_character_set = COALESCE(?, default_character_set),"
                              "default_collation = COALESCE(?, default_collation),"
                              "default_encryption = COALESCE(?, default_encryption),"
                              "read_only = CASE WHEN ? THEN ? ELSE read_only END "
                              "WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    if (options->has_read_only) {
        has_read_only = 1;
    }

    if (options->character_set == NULL) {
        sqlite3_bind_null(stmt, 1);
    } else {
        sqlite3_bind_text(stmt, 1, options->character_set, -1, sqlite_transient_destructor());
    }
    if (options->collation == NULL) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_text(stmt, 2, options->collation, -1, sqlite_transient_destructor());
    }
    if (options->encryption == NULL) {
        sqlite3_bind_null(stmt, 3);
    } else {
        sqlite3_bind_text(stmt, 3, options->encryption, -1, sqlite_transient_destructor());
    }
    sqlite3_bind_int(stmt, bind_has_read_only, has_read_only);
    sqlite3_bind_int(stmt, bind_read_only, options->read_only);
    sqlite3_bind_text(stmt, bind_schema_name, schema_name, -1, sqlite_transient_destructor());

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int delete_schema(mylite_db *database, const char *schema_name)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] = "DELETE FROM __mylite_schema_catalog WHERE name = ?";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, schema_name, -1, sqlite_transient_destructor());
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return set_sqlite_error(database);
    }
    return MYLITE_OK;
}

static int set_selected_schema(mylite_db *database, const char *schema_name)
{
    char *copy = copy_span_text(schema_name, strlen(schema_name));

    if (copy == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(database->selected_schema);
    database->selected_schema = copy;
    return MYLITE_OK;
}

static void clear_selected_schema_if_matches(mylite_db *database, const char *schema_name)
{
    if (database->selected_schema != NULL && strcmp(database->selected_schema, schema_name) == 0) {
        free(database->selected_schema);
        database->selected_schema = NULL;
    }
}

static int information_schema_table_from_select(const struct mylite_sql_ast_node *statement,
                                                enum mylite_information_schema_table *out_table)
{
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    enum mylite_information_schema_table table = MYLITE_INFORMATION_SCHEMA_NONE;
    int status = information_schema_table_from_from_clause(from_clause, &table);

    *out_table = MYLITE_INFORMATION_SCHEMA_NONE;
    if (status != MYLITE_OK) {
        return status;
    }
    if (table == MYLITE_INFORMATION_SCHEMA_NONE) {
        return MYLITE_OK;
    }
    if (child_at(statement, 2U) != NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (!select_list_is_wildcard(select_list)) {
        return MYLITE_UNSUPPORTED;
    }

    *out_table = table;
    return MYLITE_OK;
}

static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list)
{
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_item == NULL || select_item->next_sibling != NULL ||
        select_item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL ||
        expression->kind != MYLITE_SQL_AST_WILDCARD) {
        return false;
    }
    return true;
}

static int
information_schema_table_from_from_clause(const struct mylite_sql_ast_node *from_clause,
                                          enum mylite_information_schema_table *out_table)
{
    const struct mylite_sql_ast_node *identifier = child_at(from_clause, 0U);

    *out_table = MYLITE_INFORMATION_SCHEMA_NONE;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }

    return information_schema_table_from_qualified_name(identifier, out_table);
}

static int
information_schema_table_from_qualified_name(const struct mylite_sql_ast_node *identifier,
                                             enum mylite_information_schema_table *out_table)
{
    const struct mylite_sql_ast_node *schema = child_at(identifier, 0U);
    const struct mylite_sql_ast_node *table = child_at(identifier, 1U);
    char *schema_name = NULL;
    char *table_name = NULL;

    *out_table = MYLITE_INFORMATION_SCHEMA_NONE;
    if (identifier == NULL || identifier->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER ||
        schema == NULL || schema->kind != MYLITE_SQL_AST_IDENTIFIER || table == NULL ||
        table->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    schema_name = copy_identifier_span(schema);
    table_name = copy_identifier_span(table);
    if (schema_name == NULL || table_name == NULL) {
        free(schema_name);
        free(table_name);
        return MYLITE_NOMEM;
    }

    if (ascii_case_equal(schema_name, "information_schema")) {
        *out_table = information_schema_table_from_name(table_name);
        if (*out_table == MYLITE_INFORMATION_SCHEMA_NONE) {
            free(schema_name);
            free(table_name);
            return MYLITE_UNSUPPORTED;
        }
    }

    free(schema_name);
    free(table_name);
    return MYLITE_OK;
}

static enum mylite_information_schema_table information_schema_table_from_name(const char *name)
{
    if (ascii_case_equal(name, "schemata")) {
        return MYLITE_INFORMATION_SCHEMA_SCHEMATA;
    }
    if (ascii_case_equal(name, "tables")) {
        return MYLITE_INFORMATION_SCHEMA_TABLES;
    }
    if (ascii_case_equal(name, "columns")) {
        return MYLITE_INFORMATION_SCHEMA_COLUMNS;
    }
    if (ascii_case_equal(name, "statistics")) {
        return MYLITE_INFORMATION_SCHEMA_STATISTICS;
    }
    return MYLITE_INFORMATION_SCHEMA_NONE;
}

static const char *information_schema_table_sql(enum mylite_information_schema_table table)
{
    switch (table) {
    case MYLITE_INFORMATION_SCHEMA_SCHEMATA:
        return information_schema_schemata_sql;
    case MYLITE_INFORMATION_SCHEMA_TABLES:
        return information_schema_tables_sql;
    case MYLITE_INFORMATION_SCHEMA_COLUMNS:
        return information_schema_columns_sql;
    case MYLITE_INFORMATION_SCHEMA_STATISTICS:
        return information_schema_statistics_sql;
    case MYLITE_INFORMATION_SCHEMA_NONE:
        return NULL;
    }

    return NULL;
}

static int copy_statement_schema_name(const struct mylite_sql_ast_node *statement,
                                      enum mylite_stmt_kind kind, char **out_schema_name)
{
    const struct mylite_sql_ast_node *schema_name = NULL;

    *out_schema_name = NULL;
    (void)kind;
    schema_name = find_child_kind(statement, MYLITE_SQL_AST_IDENTIFIER);

    if (schema_name == NULL) {
        return MYLITE_OK;
    }

    *out_schema_name = copy_identifier_span(schema_name);
    return *out_schema_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_schema_options(const struct mylite_sql_ast_node *statement,
                               enum mylite_stmt_kind kind, struct mylite_schema_options *options)
{
    const struct mylite_sql_ast_node *option_list = NULL;
    const struct mylite_sql_ast_node *option = NULL;
    int status = MYLITE_OK;

    switch (kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
        option_list = find_child_kind(statement, MYLITE_SQL_AST_SCHEMA_OPTION_LIST);
        break;
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
    case MYLITE_STMT_CREATE_TABLE:
    case MYLITE_STMT_DROP_TABLE:
    case MYLITE_STMT_INSERT_VALUES:
    case MYLITE_STMT_INSERT_SET:
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_SCALAR_SELECT:
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_SQLITE:
        return MYLITE_OK;
    }

    for (option = option_list == NULL ? NULL : option_list->first_child; option != NULL;
         option = option->next_sibling) {
        status = apply_schema_option(option, options);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int copy_connection_charset_statement(const struct mylite_sql_ast_node *statement,
                                             mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *character_set = child_at(statement, 0U);
    const struct mylite_sql_ast_node *collation = child_at(statement, 1U);

    if (character_set != NULL && character_set->kind == MYLITE_SQL_AST_DEFAULT) {
        stmt->use_default_connection_charset = true;
        return MYLITE_OK;
    }

    stmt->character_set_name = copy_schema_text_span(character_set);
    if (stmt->character_set_name == NULL) {
        return MYLITE_NOMEM;
    }

    if (collation != NULL) {
        stmt->collation_name = copy_schema_text_span(collation);
        if (stmt->collation_name == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_statement(const struct mylite_sql_ast_node *statement,
                                       mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *table_name = child_at(statement, 0U);
    const struct mylite_sql_ast_node *elements = child_at(statement, 1U);
    int status = copy_create_table_name(table_name, &stmt->create_table);

    if (status != MYLITE_OK) {
        return status;
    }
    status = copy_create_table_elements(elements, &stmt->create_table);
    if (status != MYLITE_OK) {
        return status;
    }
    return copy_create_table_options(statement, &stmt->create_table.options);
}

static int copy_drop_table_statement(const struct mylite_sql_ast_node *statement, mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *table_names = child_at(statement, 0U);

    stmt->drop_table.temporary = statement->drop_table_temporary;
    stmt->drop_table.restrict_mode = statement->drop_table_restrict;
    stmt->drop_table.cascade_mode = statement->drop_table_cascade;

    for (const struct mylite_sql_ast_node *table_name =
             table_names == NULL ? NULL : table_names->first_child;
         table_name != NULL; table_name = table_name->next_sibling) {
        struct mylite_drop_table_target target = {0};
        int status = copy_drop_table_target(table_name, &target);

        if (status == MYLITE_OK) {
            status = add_drop_table_target(&stmt->drop_table, target);
        }
        if (status != MYLITE_OK) {
            drop_table_target_deinit(&target);
            return status;
        }
    }
    return stmt->drop_table.target_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_create_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_create_table_plan *plan)
{
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->table_name = copy_identifier_span(table_name);
        return plan->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        child_at(table_name, 0U) != NULL && child_at(table_name, 1U) != NULL &&
        child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->schema_name = copy_identifier_span(child_at(table_name, 0U));
        plan->table_name = copy_identifier_span(child_at(table_name, 1U));
        if (plan->schema_name == NULL || plan->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_drop_table_target(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_drop_table_target *target)
{
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        target->table_name = copy_identifier_span(table_name);
        return target->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        child_at(table_name, 0U) != NULL && child_at(table_name, 1U) != NULL &&
        child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        target->schema_name = copy_identifier_span(child_at(table_name, 0U));
        if (target->schema_name == NULL) {
            return MYLITE_NOMEM;
        }
        target->table_name = copy_identifier_span(child_at(table_name, 1U));
        if (target->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_insert_values_statement(const struct mylite_sql_ast_node *statement,
                                        mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *table_name = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *columns = NULL;
    const struct mylite_sql_ast_node *rows = NULL;
    int status = copy_insert_table_name(table_name, &stmt->insert_values);

    if (second_child != NULL && second_child->kind == MYLITE_SQL_AST_INSERT_COLUMN_LIST) {
        columns = second_child;
        rows = child_at(statement, 2U);
    } else {
        rows = second_child;
    }

    if (status == MYLITE_OK) {
        status = copy_insert_column_list(columns, &stmt->insert_values);
    }
    if (status == MYLITE_OK) {
        status = copy_insert_rows(rows, &stmt->insert_values);
    }
    return status;
}

static int copy_insert_set_statement(const struct mylite_sql_ast_node *statement, mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *table_name = child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignments = child_at(statement, 1U);
    int status = copy_insert_table_name(table_name, &stmt->insert_values);

    if (status == MYLITE_OK) {
        status = copy_insert_set_assignments(assignments, &stmt->insert_set);
    }
    return status;
}

static int copy_update_statement(const struct mylite_sql_ast_node *statement, mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *target = child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignments = child_at(statement, 1U);
    int status = copy_update_target(target, &stmt->update.target);

    if (status == MYLITE_OK) {
        status = copy_update_assignments(assignments, &stmt->update);
    }
    return status;
}

static int copy_scalar_select_statement(const struct mylite_sql_ast_node *statement,
                                        mylite_stmt *stmt)
{
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    size_t column_count = 0U;

    for (const struct mylite_sql_ast_node *item = select_list == NULL ? NULL
                                                                      : select_list->first_child;
         item != NULL; item = item->next_sibling) {
        ++column_count;
    }
    if (column_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    stmt->scalar_result.values = calloc(column_count, sizeof(*stmt->scalar_result.values));
    stmt->scalar_result.texts = (char **)calloc(column_count, sizeof(*stmt->scalar_result.texts));
    stmt->result_metadata.columns = calloc(column_count, sizeof(*stmt->result_metadata.columns));
    if (stmt->scalar_result.values == NULL || stmt->scalar_result.texts == NULL ||
        stmt->result_metadata.columns == NULL) {
        return MYLITE_NOMEM;
    }
    stmt->scalar_result.value_count = column_count;
    stmt->result_metadata.column_count = column_count;
    stmt->affected_rows = -1;

    size_t index = 0U;
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling, ++index) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        const struct mylite_sql_ast_node *alias = child_at(item, 1U);
        int status = mylite_expression_eval(expression, &stmt->scalar_result.warnings,
                                            &stmt->scalar_result.values[index]);

        if (status != 0) {
            return MYLITE_UNSUPPORTED;
        }
        stmt->scalar_result.texts[index] =
            mylite_expression_value_to_text(&stmt->scalar_result.values[index]);
        if (stmt->scalar_result.values[index].kind != MYLITE_EXPRESSION_VALUE_NULL &&
            stmt->scalar_result.texts[index] == NULL) {
            return MYLITE_NOMEM;
        }
        if (alias != NULL) {
            stmt->result_metadata.columns[index].name = copy_select_alias(alias);
        } else {
            stmt->result_metadata.columns[index].name =
                copy_span_text(expression->span.text, expression->span.length);
        }
        if (stmt->result_metadata.columns[index].name == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_insert_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_insert_values_plan *plan)
{
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->table_name = copy_identifier_span(table_name);
        return plan->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        child_at(table_name, 0U) != NULL && child_at(table_name, 1U) != NULL &&
        child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        plan->schema_name = copy_identifier_span(child_at(table_name, 0U));
        plan->table_name = copy_identifier_span(child_at(table_name, 1U));
        if (plan->schema_name == NULL || plan->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_insert_column_list(const struct mylite_sql_ast_node *columns,
                                   struct mylite_insert_values_plan *plan)
{
    if (columns == NULL) {
        plan->has_column_list = false;
        return MYLITE_OK;
    }

    plan->has_column_list = true;
    for (const struct mylite_sql_ast_node *column = columns->first_child; column != NULL;
         column = column->next_sibling) {
        char *column_name = copy_identifier_span(column);
        int status = MYLITE_OK;

        if (column_name == NULL) {
            return MYLITE_NOMEM;
        }
        status = add_insert_column(plan, column_name);
        if (status != MYLITE_OK) {
            free(column_name);
            return status;
        }
    }
    return MYLITE_OK;
}

static int add_insert_column(struct mylite_insert_values_plan *plan, char *column_name)
{
    char **columns =
        (char **)realloc((void *)plan->columns, (plan->column_count + 1U) * sizeof(*plan->columns));

    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    plan->columns = columns;
    plan->columns[plan->column_count++] = column_name;
    return MYLITE_OK;
}

static int copy_insert_rows(const struct mylite_sql_ast_node *rows,
                            struct mylite_insert_values_plan *plan)
{
    if (rows == NULL || rows->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *row = rows->first_child; row != NULL;
         row = row->next_sibling) {
        int status = copy_insert_row(row, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->row_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_insert_row(const struct mylite_sql_ast_node *row,
                           struct mylite_insert_values_plan *plan)
{
    struct mylite_insert_row insert_row = {0};
    int status = MYLITE_OK;

    if (row == NULL || row->kind != MYLITE_SQL_AST_INSERT_ROW) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *value = row->first_child; value != NULL;
         value = value->next_sibling) {
        struct mylite_insert_value *values = NULL;
        struct mylite_insert_value insert_value = {0};

        status = copy_insert_value(value, &insert_value);
        if (status != MYLITE_OK) {
            insert_value_deinit(&insert_value);
            insert_row_deinit(&insert_row);
            return status;
        }

        values =
            realloc(insert_row.values, (insert_row.value_count + 1U) * sizeof(*insert_row.values));
        if (values == NULL) {
            insert_value_deinit(&insert_value);
            insert_row_deinit(&insert_row);
            return MYLITE_NOMEM;
        }
        insert_row.values = values;
        insert_row.values[insert_row.value_count++] = insert_value;
    }

    status = add_insert_row(plan, insert_row);
    if (status != MYLITE_OK) {
        insert_row_deinit(&insert_row);
    }
    return status;
}

static int add_insert_row(struct mylite_insert_values_plan *plan, struct mylite_insert_row row)
{
    struct mylite_insert_row *rows =
        realloc(plan->rows, (plan->row_count + 1U) * sizeof(*plan->rows));

    if (rows == NULL) {
        return MYLITE_NOMEM;
    }

    plan->rows = rows;
    plan->rows[plan->row_count++] = row;
    return MYLITE_OK;
}

static int copy_insert_value(const struct mylite_sql_ast_node *value_node,
                             struct mylite_insert_value *out_value)
{
    while (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        value_node = child_at(value_node, 0U);
    }
    if (value_node == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return copy_insert_unary_value(value_node, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return copy_insert_binary_value(value_node, out_value);
    }
    return copy_insert_simple_value(value_node, out_value);
}

static int copy_insert_simple_value(const struct mylite_sql_ast_node *value_node,
                                    struct mylite_insert_value *out_value)
{
    while (value_node != NULL && value_node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        value_node = child_at(value_node, 0U);
    }
    if (value_node == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    switch (value_node->kind) {
    case MYLITE_SQL_AST_DEFAULT:
        *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_DEFAULT};
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL:
        return copy_insert_literal_value(value_node, out_value);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP};
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        out_value->kind = MYLITE_INSERT_VALUE_COLUMN_REFERENCE;
        return copy_insert_column_reference(value_node, &out_value->column_reference);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        break;
    }

    *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_UNSUPPORTED};
    return MYLITE_OK;
}

static int copy_insert_column_reference(const struct mylite_sql_ast_node *identifier,
                                        struct mylite_insert_column_reference *out_reference)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = copy_insert_column_reference_parts(identifier, parts, &part_count);

    if (status != MYLITE_OK) {
        for (size_t index = 0U; index < part_count; ++index) {
            free(parts[index]);
        }
        return status;
    }

    if (part_count == 1U) {
        out_reference->column_name = parts[0];
        return MYLITE_OK;
    }
    if (part_count == 2U) {
        out_reference->table_name = parts[0];
        out_reference->column_name = parts[1];
        return MYLITE_OK;
    }
    if (part_count == 3U) {
        out_reference->schema_name = parts[0];
        out_reference->table_name = parts[1];
        out_reference->column_name = parts[2];
        return MYLITE_OK;
    }

    return MYLITE_UNSUPPORTED;
}

static int copy_insert_column_reference_parts(const struct mylite_sql_ast_node *identifier,
                                              char **parts, size_t *part_count)
{
    const struct mylite_sql_ast_node *segments[3] = {0};
    const struct mylite_sql_ast_node *current = identifier;
    size_t segment_count = 0U;

    *part_count = 0U;
    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (segment_count >= 3U) {
            return MYLITE_UNSUPPORTED;
        }
        segments[segment_count++] = child_at(current, 1U);
        current = child_at(current, 0U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER || segment_count >= 3U) {
        return MYLITE_UNSUPPORTED;
    }
    segments[segment_count++] = current;

    for (size_t index = 0U; index < segment_count; ++index) {
        const struct mylite_sql_ast_node *segment = segments[segment_count - index - 1U];

        if (segment == NULL || segment->kind != MYLITE_SQL_AST_IDENTIFIER) {
            return MYLITE_UNSUPPORTED;
        }
        parts[index] = copy_identifier_span(segment);
        if (parts[index] == NULL) {
            for (size_t previous = 0U; previous < index; ++previous) {
                free(parts[previous]);
                parts[previous] = NULL;
            }
            *part_count = 0U;
            return MYLITE_NOMEM;
        }
        *part_count += 1U;
    }
    return MYLITE_OK;
}

static int copy_insert_literal_value(const struct mylite_sql_ast_node *literal,
                                     struct mylite_insert_value *out_value)
{
    switch (literal->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        out_value->kind = MYLITE_INSERT_VALUE_INTEGER;
        out_value->text = copy_span_text(literal->span.text, literal->span.length);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        out_value->kind = MYLITE_INSERT_VALUE_REAL;
        out_value->text = copy_span_text(literal->span.text, literal->span.length);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_STRING:
        out_value->kind = MYLITE_INSERT_VALUE_TEXT;
        out_value->text = copy_string_literal_span(literal);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        out_value->kind = MYLITE_INSERT_VALUE_INTEGER;
        out_value->text = copy_span_text("1", 1U);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        out_value->kind = MYLITE_INSERT_VALUE_INTEGER;
        out_value->text = copy_span_text("0", 1U);
        return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        out_value->kind = MYLITE_INSERT_VALUE_NULL;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        break;
    }

    *out_value = (struct mylite_insert_value){.kind = MYLITE_INSERT_VALUE_UNSUPPORTED};
    return MYLITE_OK;
}

static int copy_insert_unary_value(const struct mylite_sql_ast_node *expression,
                                   struct mylite_insert_value *out_value)
{
    const struct mylite_sql_ast_node *operand = child_at(expression, 0U);
    const char *sign = expression->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? "-" : "+";
    size_t sign_length = strlen(sign);

    if (operand == NULL || operand->kind != MYLITE_SQL_AST_LITERAL ||
        (operand->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER &&
         operand->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL &&
         operand->literal_kind != MYLITE_SQL_AST_LITERAL_FLOAT)) {
        out_value->left = calloc(1U, sizeof(*out_value->left));
        if (out_value->left == NULL) {
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_INSERT_VALUE_UNARY_EXPRESSION;
        out_value->operator_kind = expression->operator_kind;
        return copy_insert_simple_value(operand, out_value->left);
    }

    out_value->kind = operand->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER
                          ? MYLITE_INSERT_VALUE_INTEGER
                          : MYLITE_INSERT_VALUE_REAL;
    out_value->text = malloc(sign_length + operand->span.length + 1U);
    if (out_value->text != NULL) {
        memcpy(out_value->text, sign, sign_length);
        memcpy(out_value->text + sign_length, operand->span.text, operand->span.length);
        out_value->text[sign_length + operand->span.length] = '\0';
    }
    return out_value->text == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_insert_binary_value(const struct mylite_sql_ast_node *expression,
                                    struct mylite_insert_value *out_value)
{
    out_value->left = calloc(1U, sizeof(*out_value->left));
    out_value->right = calloc(1U, sizeof(*out_value->right));
    if (out_value->left == NULL || out_value->right == NULL) {
        return MYLITE_NOMEM;
    }

    out_value->kind = MYLITE_INSERT_VALUE_BINARY_EXPRESSION;
    out_value->operator_kind = expression->operator_kind;

    int status = copy_insert_simple_value(child_at(expression, 0U), out_value->left);

    if (status != MYLITE_OK) {
        return status;
    }
    return copy_insert_simple_value(child_at(expression, 1U), out_value->right);
}

static int copy_insert_set_assignments(const struct mylite_sql_ast_node *assignments,
                                       struct mylite_insert_set_plan *plan)
{
    if (assignments == NULL || assignments->kind != MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment = assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling) {
        int status = copy_insert_set_assignment(assignment, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->assignment_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_insert_set_assignment(const struct mylite_sql_ast_node *assignment,
                                      struct mylite_insert_set_plan *plan)
{
    struct mylite_insert_set_assignment insert_assignment = {0};
    int status = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_insert_column_reference(child_at(assignment, 0U), &insert_assignment.target);
    if (status == MYLITE_OK) {
        status = copy_insert_value(child_at(assignment, 1U), &insert_assignment.value);
    }
    if (status == MYLITE_OK) {
        status = add_insert_set_assignment(plan, insert_assignment);
    }
    if (status != MYLITE_OK) {
        insert_set_assignment_deinit(&insert_assignment);
    }
    return status;
}

static int add_insert_set_assignment(struct mylite_insert_set_plan *plan,
                                     struct mylite_insert_set_assignment assignment)
{
    struct mylite_insert_set_assignment *assignments =
        realloc(plan->assignments, (plan->assignment_count + 1U) * sizeof(*plan->assignments));

    if (assignments == NULL) {
        return MYLITE_NOMEM;
    }

    plan->assignments = assignments;
    plan->assignments[plan->assignment_count++] = assignment;
    return MYLITE_OK;
}

static int copy_update_target(const struct mylite_sql_ast_node *target,
                              struct mylite_update_target *out_target)
{
    const struct mylite_sql_ast_node *table_name = child_at(target, 0U);
    const struct mylite_sql_ast_node *alias = child_at(target, 1U);
    int status = MYLITE_OK;

    if (target == NULL || target->kind != MYLITE_SQL_AST_UPDATE_TARGET) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_update_table_name(table_name, out_target);
    if (status != MYLITE_OK) {
        return status;
    }
    if (alias != NULL) {
        out_target->alias = copy_identifier_span(alias);
        if (out_target->alias == NULL) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

static int copy_update_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_update_target *target)
{
    if (table_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (table_name->kind == MYLITE_SQL_AST_IDENTIFIER) {
        target->table_name = copy_identifier_span(table_name);
        return target->table_name == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }
    if (table_name->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        child_at(table_name, 0U) != NULL && child_at(table_name, 1U) != NULL &&
        child_at(table_name, 0U)->kind == MYLITE_SQL_AST_IDENTIFIER &&
        child_at(table_name, 1U)->kind == MYLITE_SQL_AST_IDENTIFIER) {
        target->schema_name = copy_identifier_span(child_at(table_name, 0U));
        target->table_name = copy_identifier_span(child_at(table_name, 1U));
        if (target->schema_name == NULL || target->table_name == NULL) {
            return MYLITE_NOMEM;
        }
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int copy_update_assignments(const struct mylite_sql_ast_node *assignments,
                                   struct mylite_update_plan *plan)
{
    if (assignments == NULL || assignments->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST) {
        return MYLITE_UNSUPPORTED;
    }

    for (const struct mylite_sql_ast_node *assignment = assignments->first_child;
         assignment != NULL; assignment = assignment->next_sibling) {
        int status = copy_update_assignment(assignment, plan);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return plan->assignment_count == 0U ? MYLITE_UNSUPPORTED : MYLITE_OK;
}

static int copy_update_assignment(const struct mylite_sql_ast_node *assignment,
                                  struct mylite_update_plan *plan)
{
    struct mylite_update_assignment update_assignment = {0};
    int status = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = copy_update_column_reference(child_at(assignment, 0U), &update_assignment.target);
    if (status == MYLITE_OK) {
        status = add_update_assignment(plan, update_assignment);
    }
    if (status != MYLITE_OK) {
        update_assignment_deinit(&update_assignment);
    }
    return status;
}

static int add_update_assignment(struct mylite_update_plan *plan,
                                 struct mylite_update_assignment assignment)
{
    struct mylite_update_assignment *assignments =
        realloc(plan->assignments, (plan->assignment_count + 1U) * sizeof(*plan->assignments));

    if (assignments == NULL) {
        return MYLITE_NOMEM;
    }

    plan->assignments = assignments;
    plan->assignments[plan->assignment_count++] = assignment;
    return MYLITE_OK;
}

static int copy_update_column_reference(const struct mylite_sql_ast_node *identifier,
                                        struct mylite_update_column_reference *out_reference)
{
    char *parts[3] = {0};
    size_t part_count = 0U;
    int status = copy_update_column_reference_parts(identifier, parts, &part_count);

    if (status != MYLITE_OK) {
        for (size_t index = 0U; index < part_count; ++index) {
            free(parts[index]);
        }
        return status;
    }

    if (part_count == 1U) {
        out_reference->column_name = parts[0];
        return MYLITE_OK;
    }
    if (part_count == 2U) {
        out_reference->table_name = parts[0];
        out_reference->column_name = parts[1];
        return MYLITE_OK;
    }
    if (part_count == 3U) {
        out_reference->schema_name = parts[0];
        out_reference->table_name = parts[1];
        out_reference->column_name = parts[2];
        return MYLITE_OK;
    }

    return MYLITE_UNSUPPORTED;
}

static int copy_update_column_reference_parts(const struct mylite_sql_ast_node *identifier,
                                              char **parts, size_t *part_count)
{
    const struct mylite_sql_ast_node *segments[3] = {0};
    const struct mylite_sql_ast_node *current = identifier;
    size_t segment_count = 0U;

    *part_count = 0U;
    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (segment_count >= 3U) {
            return MYLITE_UNSUPPORTED;
        }
        segments[segment_count++] = child_at(current, 1U);
        current = child_at(current, 0U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER || segment_count >= 3U) {
        return MYLITE_UNSUPPORTED;
    }
    segments[segment_count++] = current;

    for (size_t index = 0U; index < segment_count; ++index) {
        const struct mylite_sql_ast_node *segment = segments[segment_count - index - 1U];

        if (segment == NULL || segment->kind != MYLITE_SQL_AST_IDENTIFIER) {
            return MYLITE_UNSUPPORTED;
        }
        parts[index] = copy_identifier_span(segment);
        if (parts[index] == NULL) {
            for (size_t previous = 0U; previous < index; ++previous) {
                free(parts[previous]);
                parts[previous] = NULL;
            }
            *part_count = 0U;
            return MYLITE_NOMEM;
        }
        *part_count += 1U;
    }
    return MYLITE_OK;
}

static int add_drop_table_target(struct mylite_drop_table_plan *plan,
                                 struct mylite_drop_table_target target)
{
    struct mylite_drop_table_target *targets =
        realloc(plan->targets, sizeof(*plan->targets) * (plan->target_count + 1U));

    if (targets == NULL) {
        return MYLITE_NOMEM;
    }

    plan->targets = targets;
    plan->targets[plan->target_count] = target;
    ++plan->target_count;
    return MYLITE_OK;
}

static int copy_create_table_elements(const struct mylite_sql_ast_node *elements,
                                      struct mylite_create_table_plan *plan)
{
    const struct mylite_sql_ast_node *element = NULL;
    int status = MYLITE_OK;

    if (elements == NULL || elements->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (element = elements->first_child; element != NULL; element = element->next_sibling) {
        if (element->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
            size_t column_index = plan->column_count;

            status = copy_create_table_column(element, plan);
            if (status == MYLITE_OK) {
                status = add_inline_create_table_column_indexes(plan, &plan->columns[column_index]);
            }
        } else if (element->kind == MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT ||
                   element->kind == MYLITE_SQL_AST_UNIQUE_INDEX ||
                   element->kind == MYLITE_SQL_AST_SECONDARY_INDEX) {
            status = copy_create_table_index(element, plan);
        } else {
            status = MYLITE_UNSUPPORTED;
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_column(const struct mylite_sql_ast_node *column_node,
                                    struct mylite_create_table_plan *plan)
{
    struct mylite_create_table_column *columns = NULL;
    struct mylite_create_table_column column = {
        .nullable = true,
        .visible = true,
    };
    int status = MYLITE_OK;

    column.name = copy_identifier_span(child_at(column_node, 0U));
    if (column.name == NULL) {
        return MYLITE_NOMEM;
    }
    status = copy_create_table_column_type(child_at(column_node, 1U), &column.type);
    if (status == MYLITE_OK) {
        status = copy_create_table_column_attributes(child_at(column_node, 2U), &column);
    }
    if (status != MYLITE_OK) {
        create_table_column_deinit(&column);
        return status;
    }

    columns = realloc(plan->columns, (plan->column_count + 1U) * sizeof(*plan->columns));
    if (columns == NULL) {
        create_table_column_deinit(&column);
        return MYLITE_NOMEM;
    }

    plan->columns = columns;
    plan->columns[plan->column_count++] = column;
    return MYLITE_OK;
}

static int copy_create_table_column_type(const struct mylite_sql_ast_node *type_node,
                                         struct mylite_create_table_column_type *type)
{
    if (type_node == NULL || type_node->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        return MYLITE_UNSUPPORTED;
    }

    *type = (struct mylite_create_table_column_type){
        .ast_type = type_node->column_type,
        .attributes =
            {
                .display_width = type_node->column_display_width,
                .length = type_node->column_length,
                .precision = type_node->column_precision,
                .scale = type_node->column_scale,
                .has_display_width = type_node->has_column_display_width,
                .has_signed = type_node->column_type_signed,
                .has_unsigned = type_node->column_type_unsigned,
                .has_length = type_node->has_column_length,
                .has_precision = type_node->has_column_precision,
                .has_scale = type_node->has_column_scale,
                .has_binary_attribute = type_node->column_binary_attribute,
                .has_byte_attribute = type_node->column_byte_attribute,
                .has_zerofill_attribute = type_node->column_zerofill_attribute,
                .is_national = type_node->column_national_attribute,
            },
    };
    if (type_node->has_column_character_set) {
        type->character_set = copy_span_text(type_node->column_character_set.text,
                                             type_node->column_character_set.length);
        if (type->character_set == NULL) {
            return MYLITE_NOMEM;
        }
        type->attributes.has_character_set = true;
        type->attributes.character_set = type->character_set;
        type->attributes.character_set_length = strlen(type->character_set);
    }
    if (type_node->has_column_collation) {
        type->collation =
            copy_span_text(type_node->column_collation.text, type_node->column_collation.length);
        if (type->collation == NULL) {
            return MYLITE_NOMEM;
        }
        type->attributes.has_collation = true;
        type->attributes.collation = type->collation;
        type->attributes.collation_length = strlen(type->collation);
    }
    return MYLITE_OK;
}

static int copy_create_table_column_attributes(const struct mylite_sql_ast_node *attributes,
                                               struct mylite_create_table_column *column)
{
    const struct mylite_sql_ast_node *attribute = NULL;

    for (attribute = attributes == NULL ? NULL : attributes->first_child; attribute != NULL;
         attribute = attribute->next_sibling) {
        char *copy = NULL;

        switch (attribute->column_attribute) {
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL:
            column->nullable = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL:
            column->nullable = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT:
            copy = copy_expression_text(child_at(attribute, 0U));
            if (copy == NULL && child_at(attribute, 0U) != NULL &&
                child_at(attribute, 0U)->literal_kind != MYLITE_SQL_AST_LITERAL_NULL) {
                return MYLITE_NOMEM;
            }
            free(column->default_text);
            column->default_text = copy;
            if (child_at(attribute, 0U) != NULL &&
                (child_at(attribute, 0U)->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP ||
                 child_at(attribute, 0U)->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION)) {
                column->has_generated_default = true;
            }
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_ON_UPDATE:
            column->has_on_update_current_timestamp = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT:
            copy = copy_string_literal_span(child_at(attribute, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(column->comment);
            column->comment = copy;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE:
            column->visible = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE:
            column->visible = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT:
            column->auto_increment = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY:
            column->primary_key = true;
            column->nullable = false;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY:
            column->unique_key = true;
            break;
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COLUMN_FORMAT:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_STORAGE:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NONE:
            break;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_index(const struct mylite_sql_ast_node *index_node,
                                   struct mylite_create_table_plan *plan)
{
    struct mylite_create_table_index index = {
        .algorithm = MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
        .is_visible = true,
    };
    const struct mylite_sql_ast_node *child = NULL;
    const struct mylite_sql_ast_node *key_parts =
        find_child_kind(index_node, MYLITE_SQL_AST_KEY_PART_LIST);
    const struct mylite_sql_ast_node *options =
        find_child_kind(index_node, MYLITE_SQL_AST_INDEX_OPTION_LIST);
    int status = MYLITE_OK;

    index.is_primary = index_node->kind == MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT;
    if (index.is_primary) {
        index.is_unique = true;
    } else {
        index.is_unique = index_node->kind == MYLITE_SQL_AST_UNIQUE_INDEX;
    }

    for (child = index_node->first_child; child != NULL && child != key_parts;
         child = child->next_sibling) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            free(index.name);
            index.name = copy_identifier_span(child);
            index.explicit_name = true;
            if (index.name == NULL) {
                create_table_index_deinit(&index);
                return MYLITE_NOMEM;
            }
        } else if (child->kind == MYLITE_SQL_AST_INDEX_TYPE) {
            index.algorithm = child->index_algorithm;
        }
    }
    if (index.is_primary) {
        free(index.name);
        index.name = copy_span_text("PRIMARY", strlen("PRIMARY"));
        index.explicit_name = true;
        if (index.name == NULL) {
            create_table_index_deinit(&index);
            return MYLITE_NOMEM;
        }
    }

    status = copy_create_table_key_parts(key_parts, &index);
    if (status == MYLITE_OK) {
        status = copy_create_table_index_options(options, &index);
    }
    if (status == MYLITE_OK) {
        status = add_create_table_index(plan, index);
    }
    if (status != MYLITE_OK) {
        create_table_index_deinit(&index);
    }
    return status;
}

static int copy_create_table_key_parts(const struct mylite_sql_ast_node *key_parts,
                                       struct mylite_create_table_index *index)
{
    const struct mylite_sql_ast_node *part_node = NULL;

    if (key_parts == NULL || key_parts->first_child == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (part_node = key_parts->first_child; part_node != NULL;
         part_node = part_node->next_sibling) {
        struct mylite_create_table_key_part *parts = NULL;
        struct mylite_create_table_key_part part = {
            .order = part_node->key_part_order,
        };
        const struct mylite_sql_ast_node *prefix = child_at(part_node, 1U);

        part.column_name = copy_identifier_span(child_at(part_node, 0U));
        if (part.column_name == NULL) {
            return MYLITE_NOMEM;
        }
        if (prefix != NULL) {
            part.has_prefix_length = true;
            part.prefix_length = prefix->column_length;
        }

        parts = realloc(index->parts, (index->part_count + 1U) * sizeof(*index->parts));
        if (parts == NULL) {
            create_table_key_part_deinit(&part);
            return MYLITE_NOMEM;
        }
        index->parts = parts;
        index->parts[index->part_count++] = part;
    }
    return MYLITE_OK;
}

static int copy_create_table_index_options(const struct mylite_sql_ast_node *options,
                                           struct mylite_create_table_index *index)
{
    const struct mylite_sql_ast_node *option = NULL;

    for (option = options == NULL ? NULL : options->first_child; option != NULL;
         option = option->next_sibling) {
        char *copy = NULL;

        switch (option->index_option) {
        case MYLITE_SQL_AST_INDEX_OPTION_USING:
            index->algorithm = option->index_algorithm;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_COMMENT:
            copy = copy_string_literal_span(child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(index->comment);
            index->comment = copy;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_VISIBLE:
            index->is_visible = true;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE:
            index->is_visible = false;
            break;
        case MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE:
        case MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE:
        case MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE:
        case MYLITE_SQL_AST_INDEX_OPTION_NONE:
            break;
        }
    }
    return MYLITE_OK;
}

static int copy_create_table_options(const struct mylite_sql_ast_node *statement,
                                     struct mylite_create_table_options *options)
{
    const struct mylite_sql_ast_node *option_list =
        find_child_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST);
    const struct mylite_sql_ast_node *option = NULL;

    for (option = option_list == NULL ? NULL : option_list->first_child; option != NULL;
         option = option->next_sibling) {
        char *copy = NULL;

        switch (option->table_option) {
        case MYLITE_SQL_AST_TABLE_OPTION_ENGINE:
            copy = copy_identifier_span(child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->engine);
            options->engine = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_CHARACTER_SET:
            if (child_at(option, 0U) != NULL &&
                child_at(option, 0U)->kind == MYLITE_SQL_AST_DEFAULT) {
                free(options->character_set);
                options->character_set = NULL;
                break;
            }
            copy = copy_schema_text_span(child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->character_set);
            options->character_set = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_COLLATE:
            if (child_at(option, 0U) != NULL &&
                child_at(option, 0U)->kind == MYLITE_SQL_AST_DEFAULT) {
                free(options->collation);
                options->collation = NULL;
                break;
            }
            copy = copy_schema_text_span(child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->collation);
            options->collation = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_COMMENT:
            copy = copy_string_literal_span(child_at(option, 0U));
            if (copy == NULL) {
                return MYLITE_NOMEM;
            }
            free(options->comment);
            options->comment = copy;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_AUTO_INCREMENT:
            options->has_auto_increment = true;
            options->auto_increment = child_at(option, 0U)->column_length;
            break;
        case MYLITE_SQL_AST_TABLE_OPTION_NONE:
            break;
        }
    }
    return MYLITE_OK;
}

static int add_create_table_index(struct mylite_create_table_plan *plan,
                                  struct mylite_create_table_index index)
{
    struct mylite_create_table_index *indexes =
        realloc(plan->indexes, (plan->index_count + 1U) * sizeof(*plan->indexes));

    if (indexes == NULL) {
        return MYLITE_NOMEM;
    }

    plan->indexes = indexes;
    plan->indexes[plan->index_count++] = index;
    return MYLITE_OK;
}

static int add_inline_create_table_column_indexes(struct mylite_create_table_plan *plan,
                                                  const struct mylite_create_table_column *column)
{
    int status = MYLITE_OK;

    if (column->primary_key) {
        status = add_single_column_index(plan, column->name, true, true);
    }
    if (status == MYLITE_OK && column->unique_key) {
        status = add_single_column_index(plan, column->name, false, true);
    }
    return status;
}

static int add_single_column_index(struct mylite_create_table_plan *plan, const char *column_name,
                                   bool is_primary, bool is_unique)
{
    struct mylite_create_table_index index = {
        .algorithm = MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE,
        .is_primary = is_primary,
        .is_unique = is_unique,
        .is_visible = true,
        .explicit_name = is_primary,
        .part_count = 1U,
    };

    if (is_primary) {
        index.name = copy_span_text("PRIMARY", strlen("PRIMARY"));
    }
    index.parts = calloc(1U, sizeof(*index.parts));
    if ((is_primary && index.name == NULL) || index.parts == NULL) {
        create_table_index_deinit(&index);
        return MYLITE_NOMEM;
    }
    index.parts[0].column_name = copy_span_text(column_name, strlen(column_name));
    if (index.parts[0].column_name == NULL) {
        create_table_index_deinit(&index);
        return MYLITE_NOMEM;
    }

    int status = add_create_table_index(plan, index);
    if (status != MYLITE_OK) {
        create_table_index_deinit(&index);
    }
    return status;
}

static int assign_generated_index_names(mylite_db *database, struct mylite_create_table_plan *plan)
{
    for (size_t index = 0U; index < plan->index_count; ++index) {
        int status = assign_generated_index_name(database, plan, index);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int assign_generated_index_name(mylite_db *database, struct mylite_create_table_plan *plan,
                                       size_t index)
{
    struct mylite_create_table_index *table_index = &plan->indexes[index];
    const char *base = NULL;
    unsigned int suffix = 1U;

    if (table_index->name != NULL) {
        return MYLITE_OK;
    }
    if (table_index->part_count == 0U || table_index->parts[0].column_name == NULL) {
        (void)set_error_message(database, "Index has no key parts");
        return MYLITE_EXEC_ERROR;
    }

    base = table_index->parts[0].column_name;
    for (;;) {
        char *candidate = generated_index_name_candidate(base, suffix);

        if (candidate == NULL) {
            (void)set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        if (!create_table_index_name_exists(plan, candidate, index)) {
            table_index->name = candidate;
            return MYLITE_OK;
        }
        free(candidate);
        ++suffix;
    }
}

static char *generated_index_name_candidate(const char *base, unsigned int suffix)
{
    enum { suffix_buffer_size = 32 };
    char suffix_buffer[suffix_buffer_size];
    size_t candidate_length = strlen(base);
    char *candidate = NULL;

    suffix_buffer[0] = '\0';
    if (suffix > 1U) {
        int written = snprintf(suffix_buffer, sizeof(suffix_buffer), "_%u", suffix);

        if (written < 0) {
            return NULL;
        }
        candidate_length += (size_t)written;
    }

    candidate = malloc(candidate_length + 1U);
    if (candidate == NULL) {
        return NULL;
    }
    (void)snprintf(candidate, candidate_length + 1U, "%s%s", base, suffix_buffer);
    return candidate;
}

static bool create_table_index_name_exists(const struct mylite_create_table_plan *plan,
                                           const char *name, size_t before_index)
{
    for (size_t index = 0U; index < before_index; ++index) {
        if (plan->indexes[index].name != NULL &&
            ascii_case_equal(plan->indexes[index].name, name)) {
            return true;
        }
    }
    return false;
}

static int describe_create_table_column(const struct mylite_create_table_column *column,
                                        const struct mylite_schema_default *schema_default,
                                        const struct mylite_create_table_options *table_options,
                                        struct mylite_column_type_descriptor *out_descriptor)
{
    const char *type_name = create_table_column_type_name(column->type.ast_type);
    struct mylite_column_type_attributes attributes = column->type.attributes;
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;

    if (type_name == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    if (create_table_column_uses_string_binary_descriptor(column->type.ast_type) &&
        !attributes.has_character_set && !attributes.has_collation) {
        const char *character_set = table_options->character_set == NULL
                                        ? schema_default->character_set
                                        : table_options->character_set;
        const char *collation =
            table_options->collation == NULL ? schema_default->collation : table_options->collation;

        attributes.has_character_set = true;
        attributes.character_set = character_set;
        attributes.character_set_length = strlen(character_set);
        attributes.has_collation = true;
        attributes.collation = collation;
        attributes.collation_length = strlen(collation);
    }

    if (create_table_column_uses_integer_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_integer(type_name, strlen(type_name), attributes,
                                                     out_descriptor);
    } else if (create_table_column_uses_string_binary_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_string_binary(type_name, strlen(type_name), attributes,
                                                           out_descriptor);
    } else if (create_table_column_uses_numeric_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_numeric(type_name, strlen(type_name), attributes,
                                                     out_descriptor);
    } else if (create_table_column_uses_temporal_descriptor(column->type.ast_type)) {
        status = mylite_column_type_describe_temporal(type_name, strlen(type_name), attributes,
                                                      out_descriptor);
    } else {
        return MYLITE_UNSUPPORTED;
    }

    return status == MYLITE_COLUMN_TYPE_OK ? MYLITE_OK : MYLITE_EXEC_ERROR;
}

static const char *create_table_column_type_name(enum mylite_sql_ast_column_type column_type)
{
    switch (column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
        return "TINYINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
        return "SMALLINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
        return "MEDIUMINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
        return "INT";
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        return "BIGINT";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
        return "BOOL";
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
        return "BOOLEAN";
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
        return "CHAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
        return "VARCHAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
        return "TINYTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
        return "TEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
        return "MEDIUMTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
        return "LONGTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        return "BINARY";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
        return "VARBINARY";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
        return "TINYBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
        return "BLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
        return "MEDIUMBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
        return "LONGBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        return "DECIMAL";
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
        return "FLOAT";
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
        return "DOUBLE";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
        return "DATE";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
        return "TIME";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
        return "DATETIME";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
        return "TIMESTAMP";
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        return "YEAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
        return NULL;
    }

    return NULL;
}

static bool create_table_column_uses_integer_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_TINYINT) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN) {
        return false;
    }
    return true;
}

static bool
create_table_column_uses_string_binary_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB) {
        return false;
    }
    return true;
}

static bool create_table_column_uses_numeric_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE) {
        return false;
    }
    return true;
}

static bool
create_table_column_uses_temporal_descriptor(enum mylite_sql_ast_column_type column_type)
{
    if (column_type < MYLITE_SQL_AST_COLUMN_TYPE_DATE) {
        return false;
    }
    if (column_type > MYLITE_SQL_AST_COLUMN_TYPE_YEAR) {
        return false;
    }
    return true;
}

static const char *
sqlite_affinity_for_descriptor(const struct mylite_column_type_descriptor *descriptor)
{
    if (descriptor->integer_type != MYLITE_COLUMN_INTEGER_NONE || descriptor->is_boolean_alias) {
        return "INTEGER";
    }
    if (descriptor->is_approximate_numeric) {
        return "REAL";
    }
    if (descriptor->is_exact_numeric) {
        return "NUMERIC";
    }
    if (descriptor->is_binary_string) {
        return "BLOB";
    }
    return "TEXT";
}

static char *physical_table_name(const char *schema_name, const char *table_name)
{
    static const char prefix[] = "__mylite_user_";
    static const char separator[] = "__";
    size_t schema_length = strlen(schema_name);
    size_t table_length = strlen(table_name);
    size_t output_length =
        strlen(prefix) + (schema_length * 2U) + strlen(separator) + (table_length * 2U);
    char *output = malloc(output_length + 1U);
    size_t offset = 0U;

    if (output == NULL) {
        return NULL;
    }

    memcpy(output + offset, prefix, strlen(prefix));
    offset += strlen(prefix);
    for (size_t index = 0U; index < schema_length; ++index) {
        (void)snprintf(output + offset, 3U, "%02X", (unsigned char)schema_name[index]);
        offset += 2U;
    }
    memcpy(output + offset, separator, strlen(separator));
    offset += strlen(separator);
    for (size_t index = 0U; index < table_length; ++index) {
        (void)snprintf(output + offset, 3U, "%02X", (unsigned char)table_name[index]);
        offset += 2U;
    }
    output[offset] = '\0';
    return output;
}

static char *build_create_physical_table_sql(mylite_stmt *stmt, const char *physical_name,
                                             const struct mylite_schema_default *schema_default)
{
    sqlite3_str *sql = sqlite3_str_new(stmt->database->sqlite);

    if (sql == NULL) {
        return NULL;
    }

    sqlite3_str_appendf(sql, "CREATE TABLE \"%w\"(", physical_name);
    for (size_t index = 0U; index < stmt->create_table.column_count; ++index) {
        struct mylite_column_type_descriptor descriptor;
        int status =
            describe_create_table_column(&stmt->create_table.columns[index], schema_default,
                                         &stmt->create_table.options, &descriptor);

        if (status != MYLITE_OK) {
            sqlite3_free(sqlite3_str_finish(sql));
            return NULL;
        }
        if (index != 0U) {
            sqlite3_str_append(sql, ",", 1);
        }
        sqlite3_str_appendf(sql, "\"%w\" %s", stmt->create_table.columns[index].name,
                            sqlite_affinity_for_descriptor(&descriptor));
    }
    sqlite3_str_append(sql, ")", 1);
    return sqlite3_str_finish(sql);
}

static char *copy_expression_text(const struct mylite_sql_ast_node *node)
{
    if (node == NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL &&
        node->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return copy_string_literal_span(node);
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL && node->literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        return copy_span_text("CURRENT_TIMESTAMP", strlen("CURRENT_TIMESTAMP"));
    }
    return copy_span_text(node->span.text, node->span.length);
}

static int normalize_create_table_options(mylite_db *database, const char *schema_name,
                                          const struct mylite_schema_default *schema_default,
                                          struct mylite_create_table_options *options)
{
    const struct mylite_charset *character_set = NULL;
    const struct mylite_collation *collation = NULL;
    const char *collation_name = NULL;
    int status = MYLITE_OK;

    (void)schema_name;
    if (options->engine != NULL && !is_supported_engine_name(options->engine)) {
        status = set_error_message_parts(database, "Unsupported storage engine: '", options->engine,
                                         "'");
        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    if (options->character_set != NULL) {
        character_set = mylite_charset_lookup(options->character_set);
        if (character_set == NULL) {
            return set_unknown_charset_error(database, options->character_set);
        }
    }
    if (options->collation != NULL) {
        collation = mylite_collation_lookup(options->collation);
        if (collation == NULL) {
            return set_unknown_collation_error(database, options->collation);
        }
    }
    if (character_set == NULL && collation != NULL) {
        character_set = mylite_charset_lookup(collation->character_set);
    }
    if (character_set == NULL) {
        character_set = mylite_charset_lookup(schema_default->character_set);
    }
    if (collation == NULL) {
        collation_name = options->character_set == NULL ? schema_default->collation
                                                        : character_set->default_collation;
        collation = mylite_collation_lookup(collation_name);
    }
    if (character_set == NULL || collation == NULL) {
        (void)set_error_message(database, "Unsupported charset/collation registry entry");
        return MYLITE_EXEC_ERROR;
    }
    if (!mylite_charset_collation_match(character_set, collation)) {
        return set_collation_charset_error(database, collation->name, character_set->name);
    }

    status =
        normalize_create_table_option_text(database, &options->character_set, character_set->name);
    if (status != MYLITE_OK) {
        return status;
    }
    return normalize_create_table_option_text(database, &options->collation, collation->name);
}

static int normalize_create_table_option_text(mylite_db *database, char **target, const char *value)
{
    char *copy = copy_span_text(value, strlen(value));

    if (copy == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(*target);
    *target = copy;
    return MYLITE_OK;
}

static bool is_supported_engine_name(const char *name)
{
    if (name == NULL) {
        return true;
    }
    return ascii_case_equal(name, "InnoDB");
}

static bool validate_create_table_column_names(mylite_db *database,
                                               const struct mylite_create_table_plan *plan)
{
    if (plan->column_count == 0U) {
        (void)set_error_message(database, "CREATE TABLE requires at least one column");
        return false;
    }

    for (size_t left = 0U; left < plan->column_count; ++left) {
        for (size_t right = left + 1U; right < plan->column_count; ++right) {
            if (ascii_case_equal(plan->columns[left].name, plan->columns[right].name)) {
                (void)set_error_message_parts(database, "Duplicate column name '",
                                              plan->columns[right].name, "'");
                return false;
            }
        }
    }
    return true;
}

static bool validate_create_table_indexes(mylite_db *database,
                                          const struct mylite_create_table_plan *plan)
{
    bool has_primary = false;

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (table_index->is_primary) {
            if (has_primary) {
                (void)set_error_message(database, "Multiple primary key defined");
                return false;
            }
            has_primary = true;
        }
        if (table_index->explicit_name &&
            create_table_index_name_exists(plan, table_index->name, index)) {
            (void)set_error_message_parts(database, "Duplicate key name '", table_index->name, "'");
            return false;
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            if (find_create_table_column(plan, table_index->parts[part].column_name) == NULL) {
                (void)set_error_message_parts(database, "Key column '",
                                              table_index->parts[part].column_name,
                                              "' doesn't exist in table");
                return false;
            }
        }
    }
    return true;
}

static void apply_create_table_primary_key_nullability(struct mylite_create_table_plan *plan)
{
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        if (!table_index->is_primary) {
            continue;
        }
        for (size_t part = 0U; part < table_index->part_count; ++part) {
            for (size_t column = 0U; column < plan->column_count; ++column) {
                if (ascii_case_equal(plan->columns[column].name,
                                     table_index->parts[part].column_name)) {
                    plan->columns[column].nullable = false;
                }
            }
        }
    }
}

static const struct mylite_create_table_column *
find_create_table_column(const struct mylite_create_table_plan *plan, const char *name)
{
    for (size_t index = 0U; index < plan->column_count; ++index) {
        if (ascii_case_equal(plan->columns[index].name, name)) {
            return &plan->columns[index];
        }
    }
    return NULL;
}

static struct mylite_create_table_column_index_status
create_table_column_index_status(const struct mylite_create_table_plan *plan,
                                 const char *column_name)
{
    struct mylite_create_table_column_index_status status = {
        .indexed = false,
        .unique = false,
        .primary = false,
    };

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct mylite_create_table_index *table_index = &plan->indexes[index];

        for (size_t part = 0U; part < table_index->part_count; ++part) {
            if (!ascii_case_equal(table_index->parts[part].column_name, column_name)) {
                continue;
            }
            status.indexed = true;
            if (table_index->is_primary) {
                status.primary = true;
            }
            if (table_index->is_unique && part == 0U) {
                status.unique = true;
            }
        }
    }
    return status;
}

static const char *create_table_column_key(const struct mylite_create_table_plan *plan,
                                           const char *column_name)
{
    struct mylite_create_table_column_index_status status =
        create_table_column_index_status(plan, column_name);

    if (status.primary) {
        return "PRI";
    }
    if (status.unique) {
        return "UNI";
    }
    if (status.indexed) {
        return "MUL";
    }
    return "";
}

static const char *create_table_column_extra(const struct mylite_create_table_column *column)
{
    if (column->auto_increment) {
        return "auto_increment";
    }
    if (column->has_generated_default && column->has_on_update_current_timestamp &&
        !column->visible) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (column->has_generated_default && column->has_on_update_current_timestamp) {
        return "DEFAULT_GENERATED on update CURRENT_TIMESTAMP";
    }
    if (column->has_generated_default && !column->visible) {
        return "DEFAULT_GENERATED INVISIBLE";
    }
    if (column->has_generated_default) {
        return "DEFAULT_GENERATED";
    }
    if (column->has_on_update_current_timestamp && !column->visible) {
        return "on update CURRENT_TIMESTAMP INVISIBLE";
    }
    if (column->has_on_update_current_timestamp) {
        return "on update CURRENT_TIMESTAMP";
    }
    if (!column->visible) {
        return "INVISIBLE";
    }
    return "";
}

static const char *index_collation_for_order(enum mylite_sql_ast_key_part_order order)
{
    return order == MYLITE_SQL_AST_KEY_PART_ORDER_DESC ? "D" : "A";
}

static int begin_sqlite_transaction(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    return rc == SQLITE_OK ? MYLITE_OK : set_sqlite_error(database);
}

static int commit_sqlite_transaction(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, "COMMIT", NULL, NULL, NULL);

    return rc == SQLITE_OK ? MYLITE_OK : set_sqlite_error(database);
}

static void rollback_sqlite_transaction(mylite_db *database)
{
    (void)sqlite3_exec(database->sqlite, "ROLLBACK", NULL, NULL, NULL);
}

static int apply_schema_option(const struct mylite_sql_ast_node *option,
                               struct mylite_schema_options *options)
{
    const struct mylite_sql_ast_node *value = child_at(option, 0U);
    char **target = NULL;
    char *copy = NULL;

    switch (option->schema_option) {
    case MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET:
        target = &options->character_set;
        copy = copy_schema_text_span(value);
        break;
    case MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE:
        target = &options->collation;
        copy = copy_schema_text_span(value);
        break;
    case MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION:
        target = &options->encryption;
        copy = copy_string_literal_span(value);
        break;
    case MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY:
        options->has_read_only = true;
        options->read_only = 0;
        if (value != NULL && value->kind != MYLITE_SQL_AST_IDENTIFIER) {
            if (value->span.length == 1U && value->span.text != NULL &&
                value->span.text[0] == '1') {
                options->read_only = 1;
            } else if (value->span.length != 1U || value->span.text == NULL ||
                       value->span.text[0] != '0') {
                options->invalid_read_only = true;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_SCHEMA_OPTION_NONE:
        return MYLITE_OK;
    }

    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    if (option->schema_option == MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION &&
        !is_valid_encryption_value(copy)) {
        options->invalid_encryption = true;
    }

    free(*target);
    *target = copy;
    return MYLITE_OK;
}

static int normalize_schema_options(mylite_db *database, struct mylite_schema_options *options)
{
    int status = MYLITE_OK;

    if (options->invalid_encryption) {
        (void)set_error_message(database, "Incorrect argument (should be Y or N) value");
        return MYLITE_EXEC_ERROR;
    }
    if (options->invalid_read_only) {
        (void)set_error_message(database, "Incorrect READ ONLY value");
        return MYLITE_EXEC_ERROR;
    }

    status = normalize_schema_charset_and_collation(database, options);
    return status;
}

static int normalize_schema_charset_and_collation(mylite_db *database,
                                                  struct mylite_schema_options *options)
{
    const struct mylite_charset *character_set = mylite_charset_lookup(options->character_set);
    const struct mylite_collation *collation = mylite_collation_lookup(options->collation);
    int status = MYLITE_OK;

    if (options->character_set != NULL && character_set == NULL) {
        return set_unknown_charset_error(database, options->character_set);
    }
    if (options->collation != NULL && collation == NULL) {
        return set_unknown_collation_error(database, options->collation);
    }
    if (character_set != NULL && collation != NULL &&
        !mylite_charset_collation_match(character_set, collation)) {
        return set_collation_charset_error(database, collation->name, character_set->name);
    }
    if (character_set == NULL && collation == NULL) {
        return MYLITE_OK;
    }

    if (character_set == NULL) {
        character_set = mylite_charset_lookup(collation->character_set);
    }
    if (collation == NULL) {
        collation = mylite_collation_lookup(character_set->default_collation);
    }
    if (character_set == NULL || collation == NULL) {
        (void)set_error_message(database, "Unsupported charset/collation registry entry");
        return MYLITE_EXEC_ERROR;
    }

    status = normalize_schema_option_text(database, &options->character_set, character_set->name);
    if (status != MYLITE_OK) {
        return status;
    }
    return normalize_schema_option_text(database, &options->collation, collation->name);
}

static int normalize_schema_option_text(mylite_db *database, char **target, const char *value)
{
    char *copy = copy_span_text(value, strlen(value));

    if (copy == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(*target);
    *target = copy;
    return MYLITE_OK;
}

static int set_unknown_charset_error(mylite_db *database, const char *name)
{
    int status = set_error_message_parts(database, "Unknown character set: '", name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_unknown_collation_error(mylite_db *database, const char *name)
{
    int status = set_error_message_parts(database, "Unknown collation: '", name, "'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_collation_charset_error(mylite_db *database, const char *collation,
                                       const char *character_set)
{
    char *prefix = NULL;
    int status = MYLITE_EXEC_ERROR;

    if (set_error_message_parts(database, "COLLATION '", collation,
                                "' is not valid for CHARACTER SET '") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }

    prefix = database->error_message;
    database->error_message = NULL;
    status = set_error_message_parts(database, prefix, character_set, "'");
    free(prefix);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_unknown_table_error(mylite_db *database, const char *schema_name,
                                   const char *table_name)
{
    char *message = sqlite3_mprintf("Unknown table '%q.%q'", schema_name, table_name);
    int status = MYLITE_OK;

    if (message == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = set_error_message(database, message);
    sqlite3_free(message);
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static bool is_valid_encryption_value(const char *value)
{
    if (value == NULL || value[0] == '\0' || value[1] != '\0') {
        return false;
    }
    if (value[0] == 'Y' || value[0] == 'y' || value[0] == 'N' || value[0] == 'n') {
        return true;
    }
    return false;
}

static bool ascii_case_equal(const char *left, const char *right)
{
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        unsigned char left_byte = (unsigned char)left[index];
        unsigned char right_byte = (unsigned char)right[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return false;
        }
        ++index;
    }
    if (left[index] == '\0' && right[index] == '\0') {
        return true;
    }
    return false;
}

static char *copy_identifier_span(const struct mylite_sql_ast_node *node)
{
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    char *copy = NULL;
    size_t output = 0U;

    if (text == NULL) {
        return NULL;
    }
    if (length < 2U || text[0] != '`' || text[length - 1U] != '`') {
        return copy_span_text(text, length);
    }

    copy = malloc(length - 1U);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = 1U; index + 1U < length; ++index) {
        if (text[index] == '`' && index + 2U < length && text[index + 1U] == '`') {
            copy[output++] = '`';
            ++index;
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

static char *copy_string_literal_span(const struct mylite_sql_ast_node *node)
{
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    char quote = '\0';
    char *copy = NULL;
    size_t output = 0U;

    if (text == NULL) {
        return NULL;
    }
    if (length < 2U || (text[0] != '\'' && text[0] != '"')) {
        return copy_span_text(text, length);
    }

    quote = text[0];
    copy = malloc(length - 1U);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = 1U; index + 1U < length; ++index) {
        if (text[index] == quote && index + 2U < length && text[index + 1U] == quote) {
            copy[output++] = quote;
            ++index;
        } else if (text[index] == '\\' && index + 2U < length) {
            copy[output++] = text[++index];
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

static char *copy_schema_text_span(const struct mylite_sql_ast_node *node)
{
    if (node != NULL && node->kind == MYLITE_SQL_AST_LITERAL) {
        return copy_string_literal_span(node);
    }
    return copy_identifier_span(node);
}

static char *copy_span_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    if (length > 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

static bool span_contains_newline(const char *text, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] == '\n' || text[index] == '\r') {
            return true;
        }
    }
    return false;
}

static bool text_contains_word(const char *text, const char *word)
{
    if (text == NULL || word == NULL || word[0] == '\0') {
        return false;
    }
    return strstr(text, word) != NULL;
}

static const struct mylite_result_column_metadata *result_metadata_column(const mylite_stmt *stmt,
                                                                          int column)
{
    if (stmt == NULL || column < 0 || stmt->result_metadata.columns == NULL ||
        (size_t)column >= stmt->result_metadata.column_count) {
        return NULL;
    }
    return &stmt->result_metadata.columns[column];
}

static bool
insert_column_uses_numeric_implicit_default(const struct mylite_insert_table_column *column)
{
    static const char *const numeric_types[] = {
        "tinyint", "smallint", "mediumint", "int",     "bigint", "decimal",
        "float",   "double",   "bool",      "boolean", "year",
    };

    if (column == NULL || column->data_type == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(numeric_types) / sizeof(numeric_types[0]); ++index) {
        if (ascii_case_equal(column->data_type, numeric_types[index])) {
            return true;
        }
    }
    return false;
}

static bool column_default_is_current_timestamp(const char *default_text)
{
    const char *start = default_text;
    const char *end = default_text == NULL ? NULL : default_text + strlen(default_text);
    static const char *const supported_current_timestamp_defaults[] = {
        "CURRENT_TIMESTAMP",
        "CURRENT_TIMESTAMP()",
        "now()",
    };
    char *copy = NULL;
    bool matches = false;

    if (default_text == NULL) {
        return false;
    }
    while (start < end && isspace((unsigned char)*start)) {
        ++start;
    }
    while (end > start && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    if (end > start + 1 && *start == '(' && *(end - 1) == ')') {
        ++start;
        --end;
        while (start < end && isspace((unsigned char)*start)) {
            ++start;
        }
        while (end > start && isspace((unsigned char)*(end - 1))) {
            --end;
        }
    }

    copy = copy_span_text(start, (size_t)(end - start));
    if (copy == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(supported_current_timestamp_defaults) /
                                        sizeof(supported_current_timestamp_defaults[0]);
         ++index) {
        if (ascii_case_equal(copy, supported_current_timestamp_defaults[index])) {
            matches = true;
            break;
        }
    }
    free(copy);
    return matches;
}

static bool parse_insert_integer_text(const char *text, int64_t *out_value)
{
    enum { decimal_base = 10 };
    char *end = NULL;
    long long value = 0;

    if (text == NULL || text[0] == '\0') {
        return false;
    }
    errno = 0;
    value = strtoll(text, &end, decimal_base);
    if (errno != 0 || end == text) {
        return false;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *out_value = (int64_t)value;
    return true;
}

static bool parse_insert_real_text(const char *text, double *out_value)
{
    char *end = NULL;
    double value = 0.0;

    if (text == NULL || text[0] == '\0') {
        return false;
    }
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text) {
        return false;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *out_value = value;
    return true;
}

static char *insert_current_timestamp_text(void)
{
    enum { timestamp_length = 19U };
    time_t now = time(NULL);
    struct tm tm_value;
    char *timestamp = malloc(timestamp_length + 1U);

    if (timestamp == NULL) {
        return NULL;
    }
#ifdef _WIN32
    if (gmtime_s(&tm_value, &now) != 0) {
        free(timestamp);
        return NULL;
    }
#else
    if (gmtime_r(&now, &tm_value) == NULL) {
        free(timestamp);
        return NULL;
    }
#endif
    if (strftime(timestamp, timestamp_length + 1U, "%Y-%m-%d %H:%M:%S", &tm_value) == 0U) {
        free(timestamp);
        return NULL;
    }
    return timestamp;
}

static void schema_options_deinit(struct mylite_schema_options *options)
{
    if (options == NULL) {
        return;
    }

    free(options->character_set);
    free(options->collation);
    free(options->encryption);
    *options = (struct mylite_schema_options){0};
}

static void create_table_options_deinit(struct mylite_create_table_options *options)
{
    if (options == NULL) {
        return;
    }

    free(options->engine);
    free(options->character_set);
    free(options->collation);
    free(options->comment);
    *options = (struct mylite_create_table_options){0};
}

static void create_table_plan_deinit(struct mylite_create_table_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    create_table_options_deinit(&plan->options);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        create_table_column_deinit(&plan->columns[index]);
    }
    free(plan->columns);
    for (size_t index = 0U; index < plan->index_count; ++index) {
        create_table_index_deinit(&plan->indexes[index]);
    }
    free(plan->indexes);
    *plan = (struct mylite_create_table_plan){0};
}

static void drop_table_plan_deinit(struct mylite_drop_table_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->target_count; ++index) {
        drop_table_target_deinit(&plan->targets[index]);
    }
    free(plan->targets);
    *plan = (struct mylite_drop_table_plan){0};
}

static void drop_table_target_deinit(struct mylite_drop_table_target *target)
{
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    *target = (struct mylite_drop_table_target){0};
}

static void insert_values_plan_deinit(struct mylite_insert_values_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->schema_name);
    free(plan->table_name);
    for (size_t index = 0U; index < plan->column_count; ++index) {
        free(plan->columns[index]);
    }
    free((void *)plan->columns);
    for (size_t index = 0U; index < plan->row_count; ++index) {
        insert_row_deinit(&plan->rows[index]);
    }
    free(plan->rows);
    *plan = (struct mylite_insert_values_plan){0};
}

static void insert_set_plan_deinit(struct mylite_insert_set_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        insert_set_assignment_deinit(&plan->assignments[index]);
    }
    free(plan->assignments);
    *plan = (struct mylite_insert_set_plan){0};
}

static void insert_set_assignment_deinit(struct mylite_insert_set_assignment *assignment)
{
    if (assignment == NULL) {
        return;
    }

    insert_column_reference_deinit(&assignment->target);
    insert_value_deinit(&assignment->value);
    *assignment = (struct mylite_insert_set_assignment){0};
}

static void insert_column_reference_deinit(struct mylite_insert_column_reference *reference)
{
    if (reference == NULL) {
        return;
    }

    free(reference->schema_name);
    free(reference->table_name);
    free(reference->column_name);
    *reference = (struct mylite_insert_column_reference){0};
}

static void update_plan_deinit(struct mylite_update_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    update_target_deinit(&plan->target);
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        update_assignment_deinit(&plan->assignments[index]);
    }
    free(plan->assignments);
    *plan = (struct mylite_update_plan){0};
}

static void update_target_deinit(struct mylite_update_target *target)
{
    if (target == NULL) {
        return;
    }

    free(target->schema_name);
    free(target->table_name);
    free(target->alias);
    *target = (struct mylite_update_target){0};
}

static void update_assignment_deinit(struct mylite_update_assignment *assignment)
{
    if (assignment == NULL) {
        return;
    }

    update_column_reference_deinit(&assignment->target);
    *assignment = (struct mylite_update_assignment){0};
}

static void update_column_reference_deinit(struct mylite_update_column_reference *reference)
{
    if (reference == NULL) {
        return;
    }

    free(reference->schema_name);
    free(reference->table_name);
    free(reference->column_name);
    *reference = (struct mylite_update_column_reference){0};
}

static void update_order_plan_deinit(struct mylite_update_order_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    free(plan->order_keys);
    *plan = (struct mylite_update_order_plan){0};
}

static void update_rowset_deinit(struct mylite_update_rowset *rowset)
{
    if (rowset == NULL) {
        return;
    }

    for (size_t index = 0U; index < rowset->row_count; ++index) {
        update_row_deinit(&rowset->rows[index]);
    }
    free(rowset->rows);
    *rowset = (struct mylite_update_rowset){0};
}

static void update_row_deinit(struct mylite_update_row *row)
{
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->value_count; ++index) {
        mylite_expression_value_deinit(&row->values[index]);
    }
    for (size_t index = 0U; index < row->order_value_count; ++index) {
        mylite_expression_value_deinit(&row->order_values[index]);
    }
    free(row->values);
    free(row->order_values);
    *row = (struct mylite_update_row){0};
}

static void insert_row_deinit(struct mylite_insert_row *row)
{
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->value_count; ++index) {
        insert_value_deinit(&row->values[index]);
    }
    free(row->values);
    *row = (struct mylite_insert_row){0};
}

static void insert_value_deinit(struct mylite_insert_value *value)
{
    if (value == NULL) {
        return;
    }

    free(value->text);
    insert_column_reference_deinit(&value->column_reference);
    insert_value_child_deinit(value->left);
    insert_value_child_deinit(value->right);
    free(value->left);
    free(value->right);
    *value = (struct mylite_insert_value){0};
}

static void insert_value_child_deinit(struct mylite_insert_value *value)
{
    if (value == NULL) {
        return;
    }

    free(value->text);
    insert_column_reference_deinit(&value->column_reference);
    free(value->left);
    free(value->right);
    *value = (struct mylite_insert_value){0};
}

static void insert_table_deinit(struct mylite_insert_table *table)
{
    if (table == NULL) {
        return;
    }

    free(table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        insert_table_column_deinit(&table->columns[index]);
    }
    free(table->columns);
    for (size_t index = 0U; index < table->unique_index_count; ++index) {
        insert_unique_index_deinit(&table->unique_indexes[index]);
    }
    free(table->unique_indexes);
    *table = (struct mylite_insert_table){0};
}

static void insert_table_column_deinit(struct mylite_insert_table_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->name);
    free(column->default_text);
    free(column->data_type);
    free(column->extra);
    *column = (struct mylite_insert_table_column){0};
}

static void insert_unique_index_deinit(struct mylite_insert_unique_index *index)
{
    if (index == NULL) {
        return;
    }

    free(index->name);
    free(index->column_indexes);
    *index = (struct mylite_insert_unique_index){0};
}

static void result_metadata_deinit(struct mylite_result_metadata *metadata)
{
    if (metadata == NULL) {
        return;
    }

    for (size_t index = 0U; index < metadata->column_count; ++index) {
        result_column_metadata_deinit(&metadata->columns[index]);
    }
    free(metadata->columns);
    *metadata = (struct mylite_result_metadata){0};
}

static void scalar_result_deinit(struct mylite_scalar_result *result)
{
    if (result == NULL) {
        return;
    }

    for (size_t index = 0U; index < result->value_count; ++index) {
        mylite_expression_value_deinit(&result->values[index]);
        free(result->texts[index]);
    }
    mylite_expression_warnings_deinit(&result->warnings);
    free(result->values);
    free((void *)result->texts);
    *result = (struct mylite_scalar_result){0};
}

static void table_select_result_deinit(struct mylite_table_select_result *result)
{
    if (result == NULL) {
        return;
    }

    table_select_current_values_deinit(result);
    for (size_t index = 0U; index < result->row_count; ++index) {
        table_select_row_deinit(&result->rows[index]);
    }
    free(result->rows);
    *result = (struct mylite_table_select_result){0};
}

static void table_select_row_deinit(struct mylite_table_select_row *row)
{
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->value_count; ++index) {
        mylite_expression_value_deinit(&row->values[index]);
    }
    for (size_t index = 0U; index < row->order_value_count; ++index) {
        mylite_expression_value_deinit(&row->order_values[index]);
    }
    free(row->values);
    free(row->order_values);
    *row = (struct mylite_table_select_row){0};
}

static void result_column_metadata_deinit(struct mylite_result_column_metadata *metadata)
{
    if (metadata == NULL) {
        return;
    }

    free(metadata->name);
    free(metadata->schema_name);
    free(metadata->table_name);
    free(metadata->origin_schema_name);
    free(metadata->origin_table_name);
    free(metadata->origin_column_name);
    *metadata = (struct mylite_result_column_metadata){0};
}

static void select_plan_deinit(struct mylite_select_plan *plan)
{
    if (plan == NULL) {
        return;
    }

    select_table_deinit(&plan->table);
    for (size_t index = 0U; index < plan->output_count; ++index) {
        select_output_column_deinit(&plan->outputs[index]);
    }
    free(plan->outputs);
    free(plan->order_keys);
    *plan = (struct mylite_select_plan){0};
}

static void select_constant_values_deinit(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return;
    }

    for (size_t index = 0U; index < stmt->select_constant_value_count; ++index) {
        mylite_expression_value_deinit(&stmt->select_constant_values[index].value);
    }
    free(stmt->select_constant_values);
    stmt->select_constant_values = NULL;
    stmt->select_constant_value_count = 0U;
}

static void select_table_deinit(struct mylite_select_table *table)
{
    if (table == NULL) {
        return;
    }

    free(table->schema_name);
    free(table->table_name);
    free(table->alias);
    free(table->physical_name);
    for (size_t index = 0U; index < table->column_count; ++index) {
        select_column_deinit(&table->columns[index]);
    }
    free(table->columns);
    *table = (struct mylite_select_table){0};
}

static void select_column_deinit(struct mylite_select_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->name);
    *column = (struct mylite_select_column){0};
}

static void select_output_column_deinit(struct mylite_select_output_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->label);
    *column = (struct mylite_select_output_column){0};
}

static void insert_bound_values_deinit(struct mylite_insert_bound_value *values, size_t value_count)
{
    if (values == NULL) {
        return;
    }

    for (size_t index = 0U; index < value_count; ++index) {
        insert_bound_value_deinit(&values[index]);
    }
    free(values);
}

static void insert_bound_value_deinit(struct mylite_insert_bound_value *value)
{
    if (value == NULL) {
        return;
    }

    free(value->text_value);
    *value = (struct mylite_insert_bound_value){0};
}

static void create_table_column_deinit(struct mylite_create_table_column *column)
{
    if (column == NULL) {
        return;
    }

    free(column->name);
    free(column->type.character_set);
    free(column->type.collation);
    free(column->default_text);
    free(column->comment);
    *column = (struct mylite_create_table_column){0};
}

static void create_table_index_deinit(struct mylite_create_table_index *index)
{
    if (index == NULL) {
        return;
    }

    free(index->name);
    free(index->comment);
    for (size_t part = 0U; part < index->part_count; ++part) {
        create_table_key_part_deinit(&index->parts[part]);
    }
    free(index->parts);
    *index = (struct mylite_create_table_index){0};
}

static void create_table_key_part_deinit(struct mylite_create_table_key_part *part)
{
    if (part == NULL) {
        return;
    }

    free(part->column_name);
    *part = (struct mylite_create_table_key_part){0};
}

static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index)
{
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static const struct mylite_sql_ast_node *find_child_kind(const struct mylite_sql_ast_node *node,
                                                         enum mylite_sql_ast_node_kind kind)
{
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    for (child = node->first_child; child != NULL; child = child->next_sibling) {
        if (child->kind == kind) {
            return child;
        }
    }
    return NULL;
}

static const struct mylite_sql_ast_node *single_statement(const struct mylite_sql_ast_node *root)
{
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || root->first_child == NULL ||
        root->first_child->next_sibling != NULL) {
        return NULL;
    }

    return root->first_child;
}

static int map_parse_status(mylite_db *database, enum mylite_sql_parse_status status)
{
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return MYLITE_OK;
    case MYLITE_SQL_PARSE_MISUSE:
        return MYLITE_MISUSE;
    case MYLITE_SQL_PARSE_NOMEM:
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    case MYLITE_SQL_PARSE_LEXER_ERROR:
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        if (set_error_message(database, mylite_sql_parse_status_name(status)) == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        return MYLITE_PARSE_ERROR;
    }

    return MYLITE_PARSE_ERROR;
}

static int map_translate_status(mylite_db *database, enum mylite_sqlite_translate_status status)
{
    switch (status) {
    case MYLITE_SQLITE_TRANSLATE_OK:
        return MYLITE_OK;
    case MYLITE_SQLITE_TRANSLATE_NOMEM:
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    case MYLITE_SQLITE_TRANSLATE_UNSUPPORTED:
        if (set_error_message(database, "unsupported SQL statement") == MYLITE_NOMEM) {
            return MYLITE_NOMEM;
        }
        return MYLITE_UNSUPPORTED;
    }

    return MYLITE_UNSUPPORTED;
}

static void clear_warnings(mylite_db *database)
{
    if (database == NULL) {
        return;
    }

    mylite_expression_warnings_deinit(&database->warnings);
}

static int set_sqlite_error(mylite_db *database)
{
    if (set_error_message(database, sqlite3_errmsg(database->sqlite)) == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }

    return MYLITE_SQLITE_ERROR;
}

static int set_error_message(mylite_db *database, const char *message)
{
    size_t length = message == NULL ? 0U : strlen(message);
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        clear_error_message(database);
        return MYLITE_NOMEM;
    }

    if (length > 0U) {
        memcpy(copy, message, length);
    }
    copy[length] = '\0';

    free(database->error_message);
    database->error_message = copy;
    return MYLITE_OK;
}

static int set_error_message_parts(mylite_db *database, const char *prefix, const char *value,
                                   const char *suffix)
{
    size_t prefix_length = prefix == NULL ? 0U : strlen(prefix);
    size_t value_length = value == NULL ? 0U : strlen(value);
    size_t suffix_length = suffix == NULL ? 0U : strlen(suffix);
    size_t length = prefix_length + value_length + suffix_length;
    char *message = malloc(length + 1U);
    size_t offset = 0U;
    int status = MYLITE_OK;

    if (message == NULL) {
        clear_error_message(database);
        return MYLITE_NOMEM;
    }

    if (prefix_length > 0U) {
        memcpy(message + offset, prefix, prefix_length);
        offset += prefix_length;
    }
    if (value_length > 0U) {
        memcpy(message + offset, value, value_length);
        offset += value_length;
    }
    if (suffix_length > 0U) {
        memcpy(message + offset, suffix, suffix_length);
        offset += suffix_length;
    }
    message[offset] = '\0';

    status = set_error_message(database, message);
    free(message);
    return status;
}

static int append_database_warning(mylite_db *database, unsigned int code, const char *message)
{
    struct mylite_expression_warning *items = NULL;
    char *copy = NULL;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }

    copy = copy_span_text(message == NULL ? "" : message, message == NULL ? 0U : strlen(message));
    if (copy == NULL) {
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    items = realloc(database->warnings.items,
                    (database->warnings.count + 1U) * sizeof(*database->warnings.items));
    if (items == NULL) {
        free(copy);
        (void)set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    database->warnings.items = items;
    database->warnings.items[database->warnings.count++] =
        (struct mylite_expression_warning){.code = code, .message = copy};
    return MYLITE_OK;
}

static void clear_error_message(mylite_db *database)
{
    if (database == NULL) {
        return;
    }

    free(database->error_message);
    database->error_message = NULL;
}

static sqlite3_destructor_type sqlite_transient_destructor(void)
{
    // SQLite's public macro intentionally uses this sentinel pointer value.
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
