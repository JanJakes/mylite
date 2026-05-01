#include <mylite/mylite.h>

#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"
#include "mylite_vfs.h"
#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

struct mylite_db {
    sqlite3 *sqlite;
    char *error_message;
};

struct mylite_stmt {
    mylite_db *database;
    sqlite3_stmt *sqlite_stmt;
};

static int open_sqlite_database(const char *filename, int flags, const char *vfs_name,
                                mylite_db **out_db);
static int prepare_sqlite_statement(mylite_db *database, const char *sqlite_sql,
                                    mylite_stmt **out_stmt);
static int map_parse_status(mylite_db *database, enum mylite_sql_parse_status status);
static int map_translate_status(mylite_db *database, enum mylite_sqlite_translate_status status);
static int set_sqlite_error(mylite_db *database);
static int set_error_message(mylite_db *database, const char *message);
static void clear_error_message(mylite_db *database);

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
    struct mylite_sqlite_translate_result translate_result;
    enum mylite_sql_parse_status parse_status = MYLITE_SQL_PARSE_OK;
    enum mylite_sqlite_translate_status translate_status = MYLITE_SQLITE_TRANSLATE_OK;
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

    translate_status = mylite_sqlite_translate(parse_result.root, &translate_result);
    if (translate_status != MYLITE_SQLITE_TRANSLATE_OK) {
        status = map_translate_status(database, translate_status);
        mylite_sql_parse_result_deinit(&parse_result);
        return status;
    }

    status = prepare_sqlite_statement(database, translate_result.sql, out_stmt);
    mylite_sqlite_translate_result_deinit(&translate_result);
    mylite_sql_parse_result_deinit(&parse_result);
    return status;
}

void mylite_finalize(mylite_stmt *stmt)
{
    if (stmt == NULL) {
        return;
    }

    sqlite3_finalize(stmt->sqlite_stmt);
    free(stmt);
}

int mylite_step(mylite_stmt *stmt)
{
    int rc = SQLITE_OK;

    if (stmt == NULL) {
        return MYLITE_MISUSE;
    }

    clear_error_message(stmt->database);
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

    return sqlite3_column_count(stmt->sqlite_stmt);
}

const char *mylite_column_name(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL || column < 0 || column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        return NULL;
    }

    return sqlite3_column_name(stmt->sqlite_stmt, column);
}

int64_t mylite_column_int64(const mylite_stmt *stmt, int column)
{
    if (stmt == NULL || column < 0 || column >= sqlite3_column_count(stmt->sqlite_stmt)) {
        return 0;
    }

    return (int64_t)sqlite3_column_int64(stmt->sqlite_stmt, column);
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

    *out_db = database;
    return MYLITE_OK;
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
        .sqlite_stmt = sqlite_stmt,
    };
    *out_stmt = stmt;
    return MYLITE_OK;
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

static void clear_error_message(mylite_db *database)
{
    if (database == NULL) {
        return;
    }

    free(database->error_message);
    database->error_message = NULL;
}
