#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    show_warnings_column_count = 3,
    show_count_warnings_column_count = 1,
    row_count_column = 0,
    row_count_text_capacity = 32,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_table = 1051,
    mysql_error_not_unique_table_alias = 1066,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_table_does_not_exist = 1146,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_warnings {
    const char *sql;
    size_t row_count;
    const char *level;
    const char *code;
    const char *message_part;
    const char *context;
};

struct expected_single_value {
    const char *sql;
    const char *expected;
    const char *context;
};

static const char *const show_warnings_names[show_warnings_column_count] = {
    "Level",
    "Code",
    "Message",
};

static int test_create_drop_if_exists_noops_and_diagnostics(void);
static int test_multi_table_drop_lifecycle(void);
static int test_errors_and_unsupported_forms(void);
static int test_catalog_lookup_failures_are_not_noops(void);
static int test_reopen_persistence(void);
static int test_independent_file_backed_handles(void);
static int execute_empty_ok(mylite_db *database, const char *sql, size_t warning_count);
static int expect_show_warnings_result(
    mylite_db *database,
    struct expected_show_warnings expectation
);
static int expect_show_warnings_two_notes(
    mylite_db *database,
    const char *first_message_part,
    const char *second_message_part,
    const char *context
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int expect_show_tables(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *first_table,
    const char *second_table,
    const char *context
);
static int expect_show_columns_numbers(mylite_db *database, const char *context);
static int expect_select_single_value(
    mylite_db *database,
    struct expected_single_value expectation
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sqlite_sql(sqlite3 *connection, const char *sql);
static int expect_int(int actual, int expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_create_drop_if_exists_noops_and_diagnostics();
    failures += test_multi_table_drop_lifecycle();
    failures += test_errors_and_unsupported_forms();
    failures += test_catalog_lookup_failures_are_not_noops();
    failures += test_reopen_persistence();
    failures += test_independent_file_backed_handles();

    return failures == 0 ? 0 : 1;
}

static int test_create_drop_if_exists_noops_and_diagnostics(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "noop") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open noop file");
    failures += execute_empty_ok(database, "CREATE DATABASE app", 0U);
    failures += execute_empty_ok(database, "USE app", 0U);

    failures += execute_empty_ok(
        database,
        "CREATE TABLE IF NOT EXISTS numbers (id INT, amount BIGINT NOT NULL)",
        0U
    );
    failures += expect_row_count(database, 0, "create if not exists row count");
    failures += expect_show_columns_numbers(database, "created numbers definition");

    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_empty_ok(
        database,
        "CREATE TABLE IF NOT EXISTS numbers (id BIGINT NOT NULL, replacement INT)",
        1U
    );
    failures += expect_row_count(database, 0, "existing create row count");
    session = mylite_connection_session_state(database);
    failures +=
        expect_uint64(session->catalog_generation, catalog_generation, "existing create catalog");
    failures += expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "existing create sqlite schema"
    );
    failures += expect_show_columns_numbers(database, "existing create leaves definition");

    failures += execute_empty_ok(
        database,
        "CREATE TABLE IF NOT EXISTS numbers (id BIGINT NOT NULL, replacement INT)",
        1U
    );
    failures += expect_show_count_warnings(database, "1", "existing create warning count");
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Note",
            .code = "1050",
            .message_part = "Table 'numbers' already exists",
            .context = "existing create warning row",
        }
    );
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT @@warning_count",
            .expected = "1",
            .context = "existing create warning count variable",
        }
    );

    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS numbers", 0U);
    failures += expect_row_count(database, 0, "drop existing row count");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS numbers", 1U);
    failures += expect_row_count(database, 0, "drop missing row count");
    session = mylite_connection_session_state(database);
    failures +=
        expect_uint64(session->catalog_generation, catalog_generation, "missing drop catalog");
    failures += expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "missing drop sqlite schema"
    );

    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS numbers", 1U);
    failures += expect_show_count_warnings(database, "1", "missing drop warning count");
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Note",
            .code = "1051",
            .message_part = "Unknown table 'app.numbers'",
            .context = "missing drop warning row",
        }
    );
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT @@warning_count",
            .expected = "1",
            .context = "missing drop warning count variable",
        }
    );

    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS missing_schema.numbers", 1U);
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Note",
            .code = "1051",
            .message_part = "Unknown table 'missing_schema.numbers'",
            .context = "missing schema drop warning row",
        }
    );
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT @@warning_count",
            .expected = "1",
            .context = "missing schema drop warning count variable",
        }
    );

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read noop preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "noop preamble unchanged"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_multi_table_drop_lifecycle(void) {
    char path[test_path_capacity];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "multi_drop") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open multi drop file");
    failures += execute_empty_ok(database, "CREATE DATABASE app", 0U);
    failures += execute_empty_ok(database, "CREATE DATABASE other", 0U);
    failures += execute_empty_ok(database, "USE app", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE a (id INT)", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE b (id INT)", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE c (id INT)", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE other.q (id INT)", 0U);

    failures += execute_empty_ok(database, "DROP TABLE a, b", 0U);
    failures += expect_row_count(database, 0, "multi drop row count");
    failures += expect_show_tables(database, "SHOW TABLES", 1U, "c", NULL, "multi drop leaves c");

    failures += execute_empty_ok(database, "CREATE TABLE a (id INT)", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE b (id INT)", 0U);
    failures += execute_error(
        database,
        "DROP TABLE a, missing_table, b",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'app.missing_table'",
        }
    );
    failures += expect_show_tables(
        database,
        "SHOW TABLES",
        3U,
        "a",
        "b",
        "missing multi drop leaves existing tables"
    );
    failures += execute_error(
        database,
        "DROP TABLE missing_one, missing_two",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'app.missing_one,app.missing_two'",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE app.a, missing_schema.nope",
        (struct expected_sql_error){
            .code = mysql_error_unknown_table,
            .sqlstate = "42S02",
            .message_part = "Unknown table 'missing_schema.nope'",
        }
    );

    failures += execute_error(
        database,
        "DROP TABLE a, a",
        (struct expected_sql_error){
            .code = mysql_error_not_unique_table_alias,
            .sqlstate = "42000",
            .message_part = "Not unique table/alias: 'a'",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE missing, missing",
        (struct expected_sql_error){
            .code = mysql_error_not_unique_table_alias,
            .sqlstate = "42000",
            .message_part = "Not unique table/alias: 'missing'",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE IF EXISTS a, app.a",
        (struct expected_sql_error){
            .code = mysql_error_not_unique_table_alias,
            .sqlstate = "42000",
            .message_part = "Not unique table/alias: 'a'",
        }
    );

    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS a, missing_one, missing_two", 2U);
    failures += expect_row_count(database, 0, "mixed if exists row count");
    failures += expect_show_tables(
        database,
        "SHOW TABLES",
        2U,
        "b",
        "c",
        "mixed if exists drops existing target"
    );

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation = session->catalog_generation;
        sqlite_schema_generation = session->sqlite_schema_generation;
    }
    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS missing_one, missing_two", 2U);
    failures += expect_row_count(database, 0, "all missing if exists row count");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation,
            "all missing if exists keeps catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "all missing if exists keeps sqlite generation"
        );
    }
    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS missing_one, missing_two", 2U);
    failures += expect_show_warnings_two_notes(
        database,
        "Unknown table 'app.missing_one'",
        "Unknown table 'app.missing_two'",
        "all missing if exists warnings"
    );

    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS b, missing_schema.nope", 1U);
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Note",
            .code = "1051",
            .message_part = "Unknown table 'missing_schema.nope'",
            .context = "mixed explicit missing schema warning",
        }
    );
    failures += expect_show_tables(
        database,
        "SHOW TABLES",
        1U,
        "c",
        NULL,
        "explicit missing schema drops existing target"
    );

    failures += execute_empty_ok(database, "CREATE DATABASE case_db", 0U);
    failures += execute_empty_ok(database, "USE case_db", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE A (id INT)", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE a (id INT)", 0U);
    failures += execute_empty_ok(database, "DROP TABLE A, a", 0U);
    failures += expect_show_tables(
        database,
        "SHOW TABLES",
        0U,
        NULL,
        NULL,
        "case-distinct multi drop clears both tables"
    );
    failures += execute_empty_ok(database, "CREATE TABLE a (id INT)", 0U);
    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS A, a", 1U);
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Note",
            .code = "1051",
            .message_part = "Unknown table 'case_db.A'",
            .context = "case-distinct missing if exists warning",
        }
    );
    failures += expect_show_tables(
        database,
        "SHOW TABLES",
        0U,
        NULL,
        NULL,
        "case-distinct if exists drops existing table"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen multi drop file");
    failures += execute_error(
        database,
        "DROP TABLE app.c, no_default",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_show_tables(
        database,
        "SHOW TABLES FROM app",
        1U,
        "c",
        NULL,
        "no default schema leaves qualified table"
    );

    failures += execute_error(
        database,
        "DROP TABLE IF EXISTS app.c, no_default",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_empty_ok(database, "DROP TABLE app.c, other.q", 0U);
    failures += expect_show_tables(
        database,
        "SHOW TABLES FROM app",
        0U,
        NULL,
        NULL,
        "qualified multi drop clears app"
    );
    failures += expect_show_tables(
        database,
        "SHOW TABLES FROM other",
        0U,
        NULL,
        NULL,
        "qualified multi drop clears other"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_errors_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open errors file");

    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS no_schema (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE IF EXISTS no_schema",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS missing_schema.t (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS _mylite_schema.t (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_empty_ok(database, "CREATE DATABASE app", 0U);
    failures += execute_empty_ok(database, "USE app", 0U);
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS _mylite_table (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE IF EXISTS _mylite_table",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "CREATE TEMPORARY TABLE temp_table LIKE other_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.other_table' doesn't exist",
        }
    );
    failures +=
        execute_empty_ok(database, "DROP TEMPORARY TABLE IF EXISTS temp_table RESTRICT", 1U);
    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS a RESTRICT", 1U);
    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS a, b CASCADE", 2U);
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS select_target AS SELECT 1 AS id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_catalog_lookup_failures_are_not_noops(void) {
    char table_catalog_path[test_path_capacity];
    char schema_catalog_path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(table_catalog_path, sizeof(table_catalog_path), "catalog_table") != 0 ||
        make_test_path(schema_catalog_path, sizeof(schema_catalog_path), "catalog_schema") != 0) {
        return 1;
    }
    remove_related_files(table_catalog_path);
    remove_related_files(schema_catalog_path);

    failures +=
        expect_int(mylite_open(table_catalog_path, &database), MYLITE_OK, "open table catalog");
    failures += execute_empty_ok(database, "CREATE DATABASE app", 0U);
    failures += execute_empty_ok(database, "USE app", 0U);
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        execute_sqlite_sql(sqlite, "DROP TABLE _mylite_catalog_tables"),
        0,
        "drop catalog tables table"
    );
    failures += execute_error(
        database,
        "DROP TABLE IF EXISTS app.missing_table",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "failed to read table descriptor",
        }
    );
    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(mylite_open(schema_catalog_path, &database), MYLITE_OK, "open schema catalog");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        execute_sqlite_sql(sqlite, "DROP TABLE _mylite_catalog_schemas"),
        0,
        "drop catalog schemas table"
    );
    failures += execute_error(
        database,
        "DROP TABLE IF EXISTS missing_schema.missing_table",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "failed to read schema descriptor",
        }
    );
    mylite_close(database);

    remove_related_files(table_catalog_path);
    remove_related_files(schema_catalog_path);
    return failures;
}

static int test_reopen_persistence(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "reopen") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open reopen file");
    failures += execute_empty_ok(database, "CREATE DATABASE app", 0U);
    failures += execute_empty_ok(database, "USE app", 0U);
    failures += execute_empty_ok(database, "CREATE TABLE IF NOT EXISTS persistent (id INT)", 0U);
    failures += execute_empty_ok(database, "INSERT INTO persistent VALUES (7)", 0U);
    mylite_close(database);

    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen file");
    failures += execute_empty_ok(database, "USE app", 0U);
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT id FROM persistent",
            .expected = "7",
            .context = "persistent row after reopen",
        }
    );
    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS persistent", 0U);
    failures += execute_empty_ok(database, "DROP TABLE IF EXISTS persistent", 1U);
    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_independent_file_backed_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second");
    failures += execute_empty_ok(first, "CREATE DATABASE app", 0U);
    failures += execute_empty_ok(second, "CREATE DATABASE app", 0U);
    failures += execute_empty_ok(first, "USE app", 0U);
    failures += execute_empty_ok(second, "USE app", 0U);
    failures += execute_empty_ok(first, "CREATE TABLE IF NOT EXISTS shared_name (id INT)", 0U);
    failures += execute_empty_ok(second, "DROP TABLE IF EXISTS shared_name", 1U);
    failures += expect_show_warnings_result(
        second,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Note",
            .code = "1051",
            .message_part = "Unknown table 'app.shared_name'",
            .context = "second independent warning",
        }
    );
    failures += execute_empty_ok(first, "DROP TABLE IF EXISTS shared_name", 0U);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int execute_empty_ok(mylite_db *database, const char *sql, size_t warning_count) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_show_warnings_result(
    mylite_db *database,
    struct expected_show_warnings expectation
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expectation.sql, &result);

    failures += expect_size(
        mylite_result_column_count(result),
        show_warnings_column_count,
        expectation.context
    );
    for (size_t column_index = 0U; column_index < show_warnings_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            show_warnings_names[column_index],
            expectation.context
        );
    }
    failures +=
        expect_size(mylite_result_row_count(result), expectation.row_count, expectation.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expectation.context);
    if (expectation.row_count > 0U) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            expectation.level,
            expectation.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 1U),
            expectation.code,
            expectation.context
        );
        failures += expect_text_contains(
            mylite_result_value_text(result, 0U, 2U),
            expectation.message_part,
            expectation.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_show_warnings_two_notes(
    mylite_db *database,
    const char *first_message_part,
    const char *second_message_part,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    failures +=
        expect_size(mylite_result_column_count(result), show_warnings_column_count, context);
    failures += expect_size(mylite_result_row_count(result), 2U, context);
    for (size_t row_index = 0U; row_index < 2U; ++row_index) {
        failures +=
            expect_text_or_null(mylite_result_value_text(result, row_index, 0U), "Note", context);
        failures +=
            expect_text_or_null(mylite_result_value_text(result, row_index, 1U), "1051", context);
    }
    failures +=
        expect_text_contains(mylite_result_value_text(result, 0U, 2U), first_message_part, context);
    failures += expect_text_contains(
        mylite_result_value_text(result, 1U, 2U),
        second_message_part,
        context
    );
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    failures +=
        expect_size(mylite_result_column_count(result), show_count_warnings_column_count, context);
    failures += expect_text_or_null(
        mylite_result_column_name(result, 0U),
        "@@session.warning_count",
        context
    );
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    const char *value = NULL;
    char expected_text[row_count_text_capacity];
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);
    int written = snprintf(expected_text, sizeof(expected_text), "%lld", (long long)expected);

    if (written < 0 || (size_t)written >= sizeof(expected_text)) {
        fprintf(stderr, "%s: failed to format expected row count\n", context);
        failures += 1;
    }
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_size(mylite_result_column_count(result), 1U, context);
    value = mylite_result_value_text(result, 0U, row_count_column);
    failures += expect_text_or_null(value, expected_text, context);
    mylite_result_free(result);

    return failures;
}

static int expect_show_tables(
    mylite_db *database,
    const char *sql,
    size_t expected_row_count,
    const char *first_table,
    const char *second_table,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    if (first_table != NULL) {
        failures +=
            expect_text_or_null(mylite_result_value_text(result, 0U, 0U), first_table, context);
    }
    if (second_table != NULL) {
        failures +=
            expect_text_or_null(mylite_result_value_text(result, 1U, 0U), second_table, context);
    }
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_columns_numbers(mylite_db *database, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COLUMNS FROM numbers", &result);

    failures += expect_size(mylite_result_row_count(result), 2U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), "id", context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 1U), "int", context);
    failures += expect_text_or_null(mylite_result_value_text(result, 1U, 0U), "amount", context);
    failures += expect_text_or_null(mylite_result_value_text(result, 1U, 1U), "bigint", context);
    failures += expect_text_or_null(mylite_result_value_text(result, 1U, 2U), "NO", context);
    mylite_result_free(result);

    return failures;
}

static int expect_select_single_value(
    mylite_db *database,
    struct expected_single_value expectation
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expectation.sql, &result);

    failures += expect_size(mylite_result_row_count(result), 1U, expectation.context);
    failures += expect_size(mylite_result_column_count(result), 1U, expectation.context);
    failures += expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expectation.expected,
        expectation.context
    );
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        return 1;
    }
    *out_result = NULL;

    rc = mylite_execute(database, sql, strlen(sql), out_result);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_table_if_exists_%s_%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        (void)remove(related_path);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int status = 0;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        status = 1;
    }
    if (fclose(file) != 0) {
        status = 1;
    }

    return status;
}

static int execute_sqlite_sql(sqlite3 *connection, const char *sql) {
    char *error = NULL;
    int sqlite_rc = SQLITE_OK;

    if (connection == NULL || sql == NULL) {
        return 1;
    }

    sqlite_rc = sqlite3_exec(connection, sql, NULL, NULL, &error);
    if (sqlite_rc != SQLITE_OK) {
        fprintf(stderr, "%s: SQLite error: %s\n", sql, error == NULL ? "(none)" : error);
        sqlite3_free(error);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected \"%s\", got \"%s\"\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected \"%s\" to contain \"%s\"\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }

    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
