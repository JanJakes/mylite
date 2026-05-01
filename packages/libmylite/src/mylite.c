#include <mylite/mylite.h>

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

struct mylite_db {
    sqlite3 *sqlite;
    char *error_message;
    char *selected_schema;
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
};

static const char schema_catalog_sql[] = "CREATE TABLE IF NOT EXISTS __mylite_schema_catalog("
                                         "name TEXT PRIMARY KEY COLLATE BINARY,"
                                         "default_character_set TEXT NOT NULL,"
                                         "default_collation TEXT NOT NULL,"
                                         "default_encryption TEXT NOT NULL,"
                                         "read_only INTEGER NOT NULL,"
                                         "is_system INTEGER NOT NULL)";
static const char show_schemas_sql[] =
    "SELECT name AS \"Database\" FROM __mylite_schema_catalog ORDER BY name COLLATE BINARY";

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db);
static int initialize_schema_catalog(mylite_db *database);
static int seed_system_schema(mylite_db *database, const char *name);
static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    mylite_stmt **out_stmt);
static int prepare_schema_lifecycle_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt);
static int prepare_show_schemas_statement(mylite_db *database, mylite_stmt **out_stmt);
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
static int schema_exists(mylite_db *database, const char *schema_name,
                         struct mylite_schema_presence *out_presence);
static int insert_schema(mylite_db *database, const char *schema_name,
                         const struct mylite_schema_options *options);
static int update_schema(mylite_db *database, const char *schema_name,
                         const struct mylite_schema_options *options);
static int delete_schema(mylite_db *database, const char *schema_name);
static int set_selected_schema(mylite_db *database, const char *schema_name);
static void clear_selected_schema_if_matches(mylite_db *database, const char *schema_name);
static int copy_statement_schema_name(const struct mylite_sql_ast_node *statement,
                                      enum mylite_stmt_kind kind, char **out_schema_name);
static int copy_schema_options(const struct mylite_sql_ast_node *statement,
                               enum mylite_stmt_kind kind, struct mylite_schema_options *options);
static int apply_schema_option(const struct mylite_sql_ast_node *option,
                               struct mylite_schema_options *options);
static int validate_schema_options(mylite_db *database,
                                   const struct mylite_schema_options *options);
static bool is_valid_encryption_value(const char *value);
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

    *out_db = database;
    return MYLITE_OK;
}

static int initialize_schema_catalog(mylite_db *database)
{
    int rc = sqlite3_exec(database->sqlite, schema_catalog_sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    rc = seed_system_schema(database, "information_schema");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = seed_system_schema(database, "mysql");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = seed_system_schema(database, "performance_schema");
    if (rc != MYLITE_OK) {
        return rc;
    }
    return seed_system_schema(database, "sys");
}

static int seed_system_schema(mylite_db *database, const char *name)
{
    sqlite3_stmt *stmt = NULL;
    static const char sql[] =
        "INSERT OR IGNORE INTO __mylite_schema_catalog("
        "name, default_character_set, default_collation, default_encryption, read_only, is_system)"
        " VALUES(?, 'utf8mb4', 'utf8mb4_0900_ai_ci', 'N', 0, 1)";
    int rc = sqlite3_prepare_v3(database->sqlite, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return set_sqlite_error(database);
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
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
        case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
            return prepare_show_schemas_statement(database, out_stmt);
        case MYLITE_SQL_AST_SCRIPT:
        case MYLITE_SQL_AST_SELECT_STATEMENT:
        case MYLITE_SQL_AST_SELECT_LIST:
        case MYLITE_SQL_AST_SELECT_ITEM:
        case MYLITE_SQL_AST_FROM_DUAL:
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
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
}

static int prepare_show_schemas_statement(mylite_db *database, mylite_stmt **out_stmt)
{
    return prepare_sqlite_statement(database, show_schemas_sql, out_stmt);
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

    status = copy_statement_schema_name(statement, kind, &stmt->schema_name);
    if (status == MYLITE_OK) {
        status = copy_schema_options(statement, kind, &stmt->options);
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
    case MYLITE_STMT_SQLITE:
        status = MYLITE_MISUSE;
        break;
    }

    return status == MYLITE_OK ? MYLITE_DONE : status;
}

static int execute_create_schema_statement(mylite_stmt *stmt)
{
    struct mylite_schema_presence presence;
    int status = validate_schema_options(stmt->database, &stmt->options);

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
    int status = validate_schema_options(stmt->database, &stmt->options);

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
    const char *character_set = options->character_set == NULL ? "utf8mb4" : options->character_set;
    const char *collation = options->collation == NULL ? "utf8mb4_0900_ai_ci" : options->collation;
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

static int validate_schema_options(mylite_db *database, const struct mylite_schema_options *options)
{
    if (options->invalid_encryption) {
        (void)set_error_message(database, "Incorrect argument (should be Y or N) value");
        return MYLITE_EXEC_ERROR;
    }
    if (options->invalid_read_only) {
        (void)set_error_message(database, "Incorrect READ ONLY value");
        return MYLITE_EXEC_ERROR;
    }
    return MYLITE_OK;
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
