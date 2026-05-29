#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

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
    path_suffix_capacity = 16,
    show_warnings_column_count = 3,
    show_count_warnings_column_count = 1,
    mysql_error_parse = 1064,
    mysql_error_database_access_denied = 1044,
    mysql_error_no_database_selected = 1046,
    mysql_error_database_exists = 1007,
    mysql_error_cant_drop_database = 1008,
    mysql_error_system_schema_access = 3552,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_unknown = 1105,
    show_database_like_column_name_capacity = 128,
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

struct expected_empty_statement_result {
    int64_t affected_rows;
    size_t warning_count;
    const char *context;
};

static const char *const show_warnings_names[show_warnings_column_count] = {
    "Level",
    "Code",
    "Message",
};

static int test_schema_success_persistence_and_cleanup(void);
static int test_schema_if_exists_noops_and_diagnostics(void);
static int test_schema_diagnostics_and_unsupported_syntax(void);
static int test_schema_if_exists_catalog_lookup_failures_are_not_noops(void);
static int test_drop_schema_physical_failure_preserves_catalog(void);
static int test_same_file_schema_handles(void);
static int test_independent_schema_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_empty_result(
    const mylite_result *result,
    int64_t expected_affected_rows,
    const char *context
);
static int expect_empty_result_with_warnings(
    const mylite_result *result,
    struct expected_empty_statement_result expectation
);
static int expect_show_warnings_result(
    mylite_db *database,
    struct expected_show_warnings expectation
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_select_single_value(
    mylite_db *database,
    struct expected_single_value expectation
);
static int expect_show_databases(
    mylite_db *database,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
);
static int expect_show_schemas(
    mylite_db *database,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
);
static int expect_show_schema_statement(
    mylite_db *database,
    const char *sql,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
);
static int expect_show_schema_statement_with_builtins(
    mylite_db *database,
    const char *sql,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int execute_sql(sqlite3 *connection, const char *sql);
static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_schema_success_persistence_and_cleanup();
    failures += test_schema_if_exists_noops_and_diagnostics();
    failures += test_schema_diagnostics_and_unsupported_syntax();
    failures += test_schema_if_exists_catalog_lookup_failures_are_not_noops();
    failures += test_drop_schema_physical_failure_preserves_catalog();
    failures += test_same_file_schema_handles();
    failures += test_independent_schema_handles();

    return failures == 0 ? 0 : 1;
}

static int test_schema_success_persistence_and_cleanup(void) {
    static const char *const initial_schemas[] = {"app", "archive"};
    static const char *const archive_only[] = {"archive"};
    static const char *const show_mysql[] = {"mysql"};
    static const char *const show_performance_schema[] = {"performance_schema"};
    static const char *const show_sys[] = {"sys"};
    static const char *const show_app[] = {"app"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    struct mylite_catalog_schema_descriptor app_schema = {0};
    struct mylite_catalog_schema_descriptor archive_schema = {0};
    struct mylite_catalog_table_descriptor numbers_table = {0};
    struct mylite_catalog_table_descriptor other_table = {0};
    char numbers_physical[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char other_physical[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    int64_t app_schema_id = 0;
    uint64_t generation_before_drop = 0U;
    uint64_t sqlite_generation_before_drop = 0U;
    int has_numbers_physical = 1;
    int has_other_physical = 1;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open schema file");
    failures += expect_show_databases(database, NULL, 0U, "initial show databases");
    failures += expect_show_schema_statement(
        database,
        "SHOW DATABASES LIKE 'mysql'",
        show_mysql,
        1U,
        "show mysql built-in schema"
    );
    failures += expect_show_schema_statement(
        database,
        "SHOW DATABASES LIKE 'performance_schema'",
        show_performance_schema,
        1U,
        "show performance_schema built-in schema"
    );
    failures += expect_show_schema_statement(
        database,
        "SHOW SCHEMAS LIKE 'sys'",
        show_sys,
        1U,
        "show sys built-in schema"
    );
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    failures += expect_empty_result(result, 1, "create database result");
    mylite_result_free(result);
    result = NULL;

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_bool(session->has_selected_schema, false, "create does not select");
    }

    failures += execute_ok(database, "USE mysql", &result);
    failures += expect_empty_result(result, 0, "use mysql built-in schema");
    mylite_result_free(result);
    result = NULL;
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT DATABASE()",
            .expected = "mysql",
            .context = "selected mysql built-in schema",
        }
    );
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT @@character_set_database",
            .expected = "utf8mb4",
            .context = "mysql built-in schema charset",
        }
    );
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT @@collation_database",
            .expected = "utf8mb4_0900_ai_ci",
            .context = "mysql built-in schema collation",
        }
    );
    failures += execute_ok(database, "USE performance_schema", &result);
    failures += expect_empty_result(result, 0, "use performance_schema built-in schema");
    mylite_result_free(result);
    result = NULL;
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT DATABASE()",
            .expected = "performance_schema",
            .context = "selected performance_schema built-in schema",
        }
    );
    failures += execute_ok(database, "USE sys", &result);
    failures += expect_empty_result(result, 0, "use sys built-in schema");
    mylite_result_free(result);
    result = NULL;
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT DATABASE()",
            .expected = "sys",
            .context = "selected sys built-in schema",
        }
    );

    failures += execute_ok(database, "CREATE SCHEMA archive", &result);
    failures += expect_empty_result(result, 1, "create schema result");
    mylite_result_free(result);
    result = NULL;

    failures += expect_show_databases(database, initial_schemas, 2U, "show created schemas");
    failures += expect_show_schemas(database, initial_schemas, 2U, "show schemas alias");
    failures += expect_show_schema_statement(
        database,
        "SHOW DATABASES LIKE 'app'",
        show_app,
        1U,
        "show user schema like filter"
    );

    failures += execute_ok(database, "USE app", &result);
    failures += expect_empty_result(result, 0, "use result");
    mylite_result_free(result);
    result = NULL;

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_bool(session->has_selected_schema, true, "use selects schema");
        failures += expect_text(session->selected_schema, "app", "selected schema name");
    }

    failures += execute_ok(database, "CREATE TABLE numbers (id INT NOT NULL, n INT)", &result);
    failures += expect_empty_result(result, 0, "create numbers table");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "INSERT INTO numbers VALUES (1, 10), (2, 20)", &result);
    failures += expect_empty_result(result, 2, "insert numbers rows");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DROP SCHEMA archive", &result);
    failures += expect_empty_result(result, 0, "drop empty archive");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE DATABASE archive", &result);
    failures += expect_empty_result(result, 1, "recreate archive");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE TABLE other_table (id INT)", &result);
    failures += expect_empty_result(result, 0, "create other table");
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &app_schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "archive", &archive_schema),
        MYLITE_OK,
        "read archive schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            app_schema.schema_id,
            "numbers",
            &numbers_table
        ),
        MYLITE_OK,
        "read numbers descriptor"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(
            database,
            app_schema.schema_id,
            "other_table",
            &other_table
        ),
        MYLITE_OK,
        "read other descriptor"
    );
    memcpy(numbers_physical, numbers_table.physical_name, sizeof(numbers_physical));
    memcpy(other_physical, other_table.physical_name, sizeof(other_physical));
    app_schema_id = app_schema.schema_id;

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        generation_before_drop = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before_drop = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "DROP DATABASE app", &result);
    failures += expect_empty_result(result, 2, "drop app with tables");
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures +=
            expect_uint64(catalog->generation, generation_before_drop + 1U, "drop app generation");
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_drop + 1U,
            "drop app SQLite schema generation"
        );
        failures += expect_bool(session->has_selected_schema, false, "drop selected clears schema");
        failures += expect_text(session->selected_schema, "", "selected schema cleared");
    }

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &app_schema),
        MYLITE_ERROR,
        "app schema removed"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app_schema_id, "numbers", &numbers_table),
        MYLITE_ERROR,
        "numbers descriptor removed"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, app_schema_id, "other_table", &other_table),
        MYLITE_ERROR,
        "other descriptor removed"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += table_exists(sqlite, numbers_physical, &has_numbers_physical);
        failures += table_exists(sqlite, other_physical, &has_other_physical);
    }
    failures += expect_int(has_numbers_physical, 0, "numbers physical table removed");
    failures += expect_int(has_other_physical, 0, "other physical table removed");

    failures += execute_error(
        database,
        "CREATE TABLE no_default (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += expect_show_databases(database, archive_only, 1U, "archive remains after drop app");

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "schema lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen schema file");
    failures += expect_show_databases(database, archive_only, 1U, "reopen archive schema");
    failures += execute_error(
        database,
        "USE app",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'app'",
        }
    );
    failures += execute_ok(database, "DROP DATABASE archive", &result);
    failures += expect_empty_result(result, 0, "drop archive after reopen");
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_schema_if_exists_noops_and_diagnostics(void) {
    static const char *const app_schema[] = {"app"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "if_exists") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open schema if exists file");

    failures += execute_ok(database, "CREATE DATABASE IF NOT EXISTS app", &result);
    failures += expect_empty_result_with_warnings(
        result,
        (struct expected_empty_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "create database if not exists missing",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT ROW_COUNT()",
            .expected = "1",
            .context = "create database if not exists row count",
        }
    );
    failures += expect_show_databases(database, app_schema, 1U, "created IF NOT EXISTS schema");

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation = catalog->generation;
    }
    if (session != NULL) {
        sqlite_schema_generation = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "CREATE SCHEMA IF NOT EXISTS app", &result);
    failures += expect_empty_result_with_warnings(
        result,
        (struct expected_empty_statement_result){
            .affected_rows = 1,
            .warning_count = 1U,
            .context = "create schema if not exists existing",
        }
    );
    mylite_result_free(result);
    result = NULL;
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation,
            "existing schema create keeps catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "existing schema create keeps sqlite generation"
        );
    }
    failures += expect_show_count_warnings(database, "1", "existing schema create warning count");
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 1U,
            .level = "Note",
            .code = "1007",
            .message_part = "Can't create database 'app'; database exists",
            .context = "existing schema create warning row",
        }
    );
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT @@warning_count",
            .expected = "1",
            .context = "existing schema create warning variable",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    failures += expect_empty_result(result, 0, "use app before drop");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE first_table (id INT)", &result);
    failures += expect_empty_result(result, 0, "create first table before schema drop");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE second_table (id INT)", &result);
    failures += expect_empty_result(result, 0, "create second table before schema drop");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DROP DATABASE IF EXISTS app", &result);
    failures += expect_empty_result(result, 2, "drop schema if exists existing");
    mylite_result_free(result);
    result = NULL;
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT ROW_COUNT()",
            .expected = "-1",
            .context = "existing schema drop row count",
        }
    );
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures +=
            expect_bool(session->has_selected_schema, false, "drop if exists clears schema");
        failures +=
            expect_text(session->selected_schema, "", "drop if exists selected schema text");
    }
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation = catalog->generation;
    }
    if (session != NULL) {
        sqlite_schema_generation = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "DROP SCHEMA IF EXISTS app", &result);
    failures += expect_empty_result_with_warnings(
        result,
        (struct expected_empty_statement_result){
            .affected_rows = 0,
            .warning_count = 1U,
            .context = "drop schema if exists missing",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT ROW_COUNT()",
            .expected = "-1",
            .context = "missing schema drop row count",
        }
    );
    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation,
            "missing schema drop keeps catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "missing schema drop keeps sqlite generation"
        );
    }
    failures += expect_show_count_warnings(database, "0", "missing schema drop stored warnings");
    failures += expect_show_warnings_result(
        database,
        (struct expected_show_warnings){
            .sql = "SHOW WARNINGS",
            .row_count = 0U,
            .level = NULL,
            .code = NULL,
            .message_part = NULL,
            .context = "missing schema drop leaves no warning row",
        }
    );
    failures += expect_select_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT @@warning_count",
            .expected = "0",
            .context = "missing schema drop warning variable",
        }
    );

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read schema if exists preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "schema if exists preamble unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen schema if exists file");
    failures += execute_ok(database, "CREATE DATABASE IF NOT EXISTS app", &result);
    failures += expect_empty_result_with_warnings(
        result,
        (struct expected_empty_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
            .context = "recreate schema after reopen",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE DATABASE IF NOT EXISTS app", &result);
    failures += expect_empty_result_with_warnings(
        result,
        (struct expected_empty_statement_result){
            .affected_rows = 1,
            .warning_count = 1U,
            .context = "existing schema after reopen",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "DROP DATABASE IF EXISTS app", &result);
    failures += expect_empty_result(result, 0, "drop empty schema after reopen");
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_schema_diagnostics_and_unsupported_syntax(void) {
    static const char *const show_app[] = {"app"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "CREATE DATABASE app",
        (struct expected_sql_error){
            .code = mysql_error_database_exists,
            .sqlstate = "HY000",
            .message_part = "Can't create database 'app'; database exists",
        }
    );
    failures += execute_error(
        database,
        "CREATE SCHEMA app",
        (struct expected_sql_error){
            .code = mysql_error_database_exists,
            .sqlstate = "HY000",
            .message_part = "Can't create database 'app'; database exists",
        }
    );
    failures += execute_error(
        database,
        "DROP DATABASE missing_app",
        (struct expected_sql_error){
            .code = mysql_error_cant_drop_database,
            .sqlstate = "HY000",
            .message_part = "Can't drop database 'missing_app'; database doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "DROP SCHEMA missing_app",
        (struct expected_sql_error){
            .code = mysql_error_cant_drop_database,
            .sqlstate = "HY000",
            .message_part = "Can't drop database 'missing_app'; database doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "USE missing_app",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_app'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "DROP DATABASE _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE mysql",
        (struct expected_sql_error){
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'mysql' is rejected.",
        }
    );
    failures += execute_error(
        database,
        "DROP DATABASE mysql",
        (struct expected_sql_error){
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'mysql' is rejected.",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE performance_schema",
        (struct expected_sql_error){
            .code = mysql_error_database_access_denied,
            .sqlstate = "42000",
            .message_part = "Access denied for user 'root'@'%' to database 'performance_schema'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE mysql.t (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'mysql' is rejected.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE sys.t (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'sys' is rejected.",
        }
    );
    failures += execute_ok(database, "USE sys", &result);
    failures += expect_empty_result(result, 0, "use sys before selected write diagnostic");
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "CREATE TABLE selected_sys_write (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_system_schema_access,
            .sqlstate = "HY000",
            .message_part = "Access to system schema 'sys' is rejected.",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE IF EXISTS app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "DROP DATABASE IF NOT EXISTS app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE IF NOT EXISTS _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "DROP DATABASE IF EXISTS _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "CREATE DATABASE options DEFAULT ENCRYPTION='N'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += expect_show_schema_statement(
        database,
        "SHOW DATABASES WHERE `Database` = 'app'",
        show_app,
        sizeof(show_app) / sizeof(show_app[0]),
        "show databases where app"
    );
    failures += execute_error(
        database,
        "SHOW DATABASES LIKE N'app%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_schema_if_exists_catalog_lookup_failures_are_not_noops(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "if_exists_create_catalog_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open create catalog failure");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_schemas");
    }
    failures += execute_error(
        database,
        "CREATE DATABASE IF NOT EXISTS app",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "failed to read schema descriptor",
        }
    );
    mylite_close(database);
    database = NULL;
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "if_exists_drop_catalog_failure") != 0) {
        return failures + 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop catalog failure");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "DROP TABLE _mylite_catalog_schemas");
    }
    failures += execute_error(
        database,
        "DROP DATABASE IF EXISTS missing_app",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "failed to read schema descriptor",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_drop_schema_physical_failure_preserves_catalog(void) {
    char path[test_path_capacity];
    char drop_sql[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY + sizeof("DROP TABLE \"\"")];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "physical_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open physical failure file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE numbers (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read failure table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        int written =
            snprintf(drop_sql, sizeof(drop_sql), "DROP TABLE \"%s\"", table.physical_name);

        if (written < 0 || (size_t)written >= sizeof(drop_sql)) {
            fprintf(stderr, "drop physical SQL is too long\n");
            failures += 1;
        } else {
            failures += execute_sql(sqlite, drop_sql);
        }
    }

    failures += execute_error(
        database,
        "DROP DATABASE app",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite schema operation failed",
        }
    );
    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "schema preserved after failed drop"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "table preserved after failed drop"
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_same_file_schema_handles(void) {
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor first_table = {0};
    struct mylite_catalog_table_descriptor second_table = {0};
    char first_physical[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char second_physical[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    int has_first_physical = 1;
    int has_second_physical = 1;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "same_file_handles") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first same-file handle");
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second same-file handle");

    failures += execute_ok(first, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "CREATE TABLE first_table (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE TABLE second_table (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_int(
        mylite_catalog_read_schema_by_name(first, "app", &schema),
        MYLITE_OK,
        "read shared app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(first, schema.schema_id, "first_table", &first_table),
        MYLITE_OK,
        "read first shared table"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(first, schema.schema_id, "second_table", &second_table),
        MYLITE_OK,
        "read second shared table"
    );
    memcpy(first_physical, first_table.physical_name, sizeof(first_physical));
    memcpy(second_physical, second_table.physical_name, sizeof(second_physical));

    failures += execute_ok(first, "DROP DATABASE app", &result);
    failures += expect_empty_result(result, 2, "drop shared schema tables");
    mylite_result_free(result);
    result = NULL;

    sqlite = mylite_connection_sqlite_for_test(first);
    if (sqlite != NULL) {
        failures += table_exists(sqlite, first_physical, &has_first_physical);
        failures += table_exists(sqlite, second_physical, &has_second_physical);
    }
    failures += expect_int(has_first_physical, 0, "first shared physical table removed");
    failures += expect_int(has_second_physical, 0, "second shared physical table removed");
    failures += expect_show_databases(second, NULL, 0U, "second handle observes dropped schema");
    failures += execute_error(
        second,
        "CREATE TABLE after_drop (id INT)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'app'",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);

    return failures;
}

static int test_independent_schema_handles(void) {
    static const char *const first_schemas[] = {"first_app"};
    static const char *const second_schemas[] = {"second_app"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");

    failures += execute_ok(first, "CREATE DATABASE first_app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE DATABASE second_app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += expect_show_databases(first, first_schemas, 1U, "first schemas");
    failures += expect_show_databases(second, second_schemas, 1U, "second schemas");

    failures += execute_ok(first, "DROP DATABASE first_app", &result);
    failures += expect_empty_result(result, 0, "drop first app");
    mylite_result_free(result);
    result = NULL;

    failures += expect_show_databases(first, NULL, 0U, "first empty after drop");
    failures += expect_show_databases(second, second_schemas, 1U, "second unaffected");

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
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

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_empty_result(
    const mylite_result *result,
    int64_t expected_affected_rows,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), expected_affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    return failures;
}

static int expect_empty_result_with_warnings(
    const mylite_result *result,
    struct expected_empty_statement_result expectation
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, expectation.context);
    failures += expect_size(mylite_result_row_count(result), 0U, expectation.context);
    failures += expect_int64(
        mylite_result_affected_rows(result),
        expectation.affected_rows,
        expectation.context
    );
    failures += expect_size(
        mylite_result_warning_count(result),
        expectation.warning_count,
        expectation.context
    );

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
        failures += expect_text(
            mylite_result_column_name(result, column_index),
            show_warnings_names[column_index],
            expectation.context
        );
    }
    failures +=
        expect_size(mylite_result_row_count(result), expectation.row_count, expectation.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expectation.context);
    if (expectation.row_count > 0U) {
        failures += expect_text(
            mylite_result_value_text(result, 0U, 0U),
            expectation.level,
            expectation.context
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, 1U),
            expectation.code,
            expectation.context
        );
        failures += expect_contains(
            mylite_result_value_text(result, 0U, 2U),
            expectation.message_part,
            expectation.context
        );
    }

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
    failures +=
        expect_text(mylite_result_column_name(result, 0U), "@@session.warning_count", context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

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
    failures += expect_text(
        mylite_result_value_text(result, 0U, 0U),
        expectation.expected,
        expectation.context
    );

    mylite_result_free(result);
    return failures;
}

static int expect_show_databases(
    mylite_db *database,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
) {
    return expect_show_schema_statement_with_builtins(
        database,
        "SHOW DATABASES",
        expected_names,
        expected_count,
        context
    );
}

static int expect_show_schemas(
    mylite_db *database,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
) {
    return expect_show_schema_statement_with_builtins(
        database,
        "SHOW SCHEMAS",
        expected_names,
        expected_count,
        context
    );
}

static int expect_show_schema_statement_with_builtins(
    mylite_db *database,
    const char *sql,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
) {
    static const char *const builtin_schemas[] = {
        "information_schema",
        "mysql",
        "performance_schema",
        "sys",
    };
    size_t builtin_count = sizeof(builtin_schemas) / sizeof(builtin_schemas[0]);
    size_t merged_count = builtin_count + expected_count;
    const char **merged_names =
        (const char **)calloc(merged_count == 0U ? 1U : merged_count, sizeof(*merged_names));
    int failures = 0;

    if (merged_names == NULL) {
        fprintf(stderr, "failed to allocate expected SHOW DATABASES rows for %s\n", context);
        return 1;
    }
    for (size_t index = 0U; index < builtin_count; ++index) {
        merged_names[index] = builtin_schemas[index];
    }
    for (size_t index = 0U; index < expected_count; ++index) {
        merged_names[builtin_count + index] = expected_names[index];
    }
    for (size_t index = 1U; index < merged_count; ++index) {
        const char *name = merged_names[index];
        size_t insert_at = index;

        while (insert_at > 0U && strcmp(merged_names[insert_at - 1U], name) > 0) {
            merged_names[insert_at] = merged_names[insert_at - 1U];
            --insert_at;
        }
        merged_names[insert_at] = name;
    }

    failures = expect_show_schema_statement(database, sql, merged_names, merged_count, context);
    free((void *)merged_names);
    return failures;
}

static int expect_show_schema_statement(
    mylite_db *database,
    const char *sql,
    const char *const *expected_names,
    size_t expected_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);
    const char *expected_column_name = "Database";
    const char *like_pattern = strstr(sql, " LIKE '");
    char like_column_name[show_database_like_column_name_capacity];

    if (like_pattern != NULL) {
        const char *pattern_start = like_pattern + strlen(" LIKE '");
        const char *pattern_end = strchr(pattern_start, '\'');

        if (pattern_end != NULL) {
            int written = snprintf(
                like_column_name,
                sizeof(like_column_name),
                "Database (%.*s)",
                (int)(pattern_end - pattern_start),
                pattern_start
            );

            if (written >= 0 && (size_t)written < sizeof(like_column_name)) {
                expected_column_name = like_column_name;
            }
        }
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_text(mylite_result_column_name(result, 0U), expected_column_name, context);
    failures += expect_size(mylite_result_row_count(result), expected_count, context);
    for (size_t index = 0U; index < expected_count; ++index) {
        failures += expect_text(
            mylite_result_value_text(result, index, 0U),
            expected_names[index],
            context
        );
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_schema_lifecycle_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
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
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        failures += 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        failures += 1;
    }
    if (fclose(file) != 0) {
        failures += 1;
    }

    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "sqlite exec '%s' failed: %s\n", sql, message == NULL ? "" : message);
        sqlite3_free(message);
        return 1;
    }
    sqlite3_free(message);

    return 0;
}

static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_schema WHERE type = 'table' AND name = ?1",
        -1,
        &statement,
        NULL
    );

    *out_exists = 0;
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(statement, 1, table_name, -1, SQLITE_TRANSIENT);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW) {
            *out_exists = sqlite3_column_int(statement, 0) == 0 ? 0 : 1;
            rc = SQLITE_OK;
        }
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to query table existence for %s: %d\n", table_name, rc);
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

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
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

static int expect_bool(bool actual, bool expected, const char *context) {
    if (actual != expected) {
        int actual_value = 0;
        int expected_value = 0;

        if (actual) {
            actual_value = 1;
        }
        if (expected) {
            expected_value = 1;
        }

        fprintf(stderr, "%s: expected %d, got %d\n", context, expected_value, actual_value);
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
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
