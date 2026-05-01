#include <mylite/mylite.h>

#include "mylite_charset.h"
#include "mylite_internal.h"
#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"
#include "mylite_vfs.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
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
static bool is_valid_encryption_value(const char *value);
static bool ascii_case_equal(const char *left, const char *right);
static char *copy_identifier_span(const struct mylite_sql_ast_node *node);
static char *copy_string_literal_span(const struct mylite_sql_ast_node *node);
static char *copy_schema_text_span(const struct mylite_sql_ast_node *node);
static char *copy_span_text(const char *text, size_t length);
static bool span_contains_newline(const char *text, size_t length);
static void schema_options_deinit(struct mylite_schema_options *options);
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
            return MYLITE_UNSUPPORTED;
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
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
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
    if (kind == MYLITE_STMT_DROP_SCHEMA &&
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
