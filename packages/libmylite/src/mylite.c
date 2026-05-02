#include <mylite/mylite.h>

#include "mylite_charset.h"
#include "mylite_internal.h"
#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"
#include "mylite_vfs.h"
#include "sqlite3.h"
#include "types/mylite_column_type.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
};

enum mylite_information_schema_table {
    MYLITE_INFORMATION_SCHEMA_NONE = 0,
    MYLITE_INFORMATION_SCHEMA_SCHEMATA = 1,
    MYLITE_INFORMATION_SCHEMA_TABLES = 2,
    MYLITE_INFORMATION_SCHEMA_COLUMNS = 3,
    MYLITE_INFORMATION_SCHEMA_STATISTICS = 4,
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

struct mylite_connection_charset_request {
    const char *character_set_name;
    const char *collation_name;
};

struct mylite_db {
    sqlite3 *sqlite;
    char *error_message;
    char *selected_schema;
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
    char *character_set_name;
    char *collation_name;
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
                                    mylite_stmt **out_stmt);
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
static int prepare_show_schemas_statement(mylite_db *database, mylite_stmt **out_stmt);
static int prepare_information_schema_select_statement(mylite_db *database,
                                                       const struct mylite_sql_ast_node *statement,
                                                       mylite_stmt **out_stmt);
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
static int copy_create_table_name(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_create_table_plan *plan);
static int copy_drop_table_target(const struct mylite_sql_ast_node *table_name,
                                  struct mylite_drop_table_target *target);
static int add_drop_table_target(struct mylite_drop_table_plan *plan,
                                 struct mylite_drop_table_target target);
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
static void schema_options_deinit(struct mylite_schema_options *options);
static void create_table_options_deinit(struct mylite_create_table_options *options);
static void create_table_plan_deinit(struct mylite_create_table_plan *plan);
static void drop_table_plan_deinit(struct mylite_drop_table_plan *plan);
static void drop_table_target_deinit(struct mylite_drop_table_target *target);
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
static int set_error_message(mylite_db *database, const char *message);
static int set_error_message_parts(mylite_db *database, const char *prefix, const char *value,
                                   const char *suffix);
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

    status = prepare_parsed_statement(database, parse_result.root, out_stmt);
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
    free(stmt);
}

int mylite_step(mylite_stmt *stmt)
{
    int rc = SQLITE_OK;

    if (stmt == NULL) {
        return MYLITE_MISUSE;
    }

    clear_error_message(stmt->database);
    if (stmt->kind != MYLITE_STMT_SQLITE) {
        return execute_custom_statement(stmt);
    }

    rc = sqlite3_step(stmt->sqlite_stmt);
    if (rc == SQLITE_ROW) {
        return MYLITE_ROW;
    }
    if (rc == SQLITE_DONE) {
        return MYLITE_DONE;
    }

    return set_sqlite_error(stmt->database);
}

int mylite_column_count(const mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return 0;
    }

    if (stmt->sqlite_stmt == NULL) {
        return 0;
    }

    return sqlite3_column_count(stmt->sqlite_stmt);
}

const char *mylite_column_name(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        return NULL;
    }

    return sqlite3_column_name(stmt->sqlite_stmt, column);
}

int64_t mylite_column_int64(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        return 0;
    }

    return (int64_t)sqlite3_column_int64(stmt->sqlite_stmt, column);
}

const char *mylite_column_text(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL || stmt->sqlite_stmt == NULL || column < 0 ||
        column >= sqlite3_column_count(stmt->sqlite_stmt)) {
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
                                    mylite_stmt **out_stmt)
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
        case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
            return prepare_show_schemas_statement(database, out_stmt);
        case MYLITE_SQL_AST_SELECT_STATEMENT:
            status = prepare_information_schema_select_statement(database, statement, out_stmt);
            if (status != MYLITE_UNSUPPORTED) {
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
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
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
