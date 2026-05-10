#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    charset_variable_column_count = 6,
    scoped_variable_column_count = 4,
    server_variable_column_count = 10,
    database_variable_full_column_count = 10,
    system_variable_column_count = 4,
    filesystem_variable_column_count = 6,
    mixed_variable_column_count = 19,
    diagnostics_variable_column_count = 7,
    reopen_variable_column_count = 8,
    database_variable_column_count = 5,
    mysql_error_parse = 1064,
    mysql_error_session_variable_only = 1238,
    mysql_error_unknown_system_variable = 1193,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

static int test_character_set_system_variable_values_and_persistence(void);
static int test_character_set_system_variable_qualifiers_and_errors(void);
static int test_independent_character_set_system_variable_handles(void);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int expect_select_numbers_row(mylite_db *database);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_show_count_errors(mylite_db *database, const char *expected, const char *context);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
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

    failures += test_character_set_system_variable_values_and_persistence();
    failures += test_character_set_system_variable_qualifiers_and_errors();
    failures += test_independent_character_set_system_variable_handles();

    return failures == 0 ? 0 : 1;
}

static int test_character_set_system_variable_values_and_persistence(void) {
    static const char *const charset_columns[] = {
        "@@character_set_client",
        "@@character_set_connection",
        "@@character_set_results",
        "@@collation_connection",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const charset_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "0",
        "-1",
    };
    static const char *const scoped_columns[] = {
        "@@session.character_set_client",
        "@@local.character_set_connection",
        "@@global.character_set_results",
        "@@global.collation_connection",
    };
    static const char *const scoped_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
    };
    static const char *const server_columns[] = {
        "@@character_set_server",
        "@@global.character_set_server",
        "@@session.character_set_server",
        "@@local.character_set_server",
        "@@collation_server",
        "@@global.collation_server",
        "@@session.collation_server",
        "@@local.collation_server",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const server_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "0",
        "-1",
    };
    static const char *const database_full_columns[] = {
        "@@character_set_database",
        "@@global.character_set_database",
        "@@session.character_set_database",
        "@@local.character_set_database",
        "@@collation_database",
        "@@global.collation_database",
        "@@session.collation_database",
        "@@local.collation_database",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const database_full_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "0",
        "-1",
    };
    static const char *const system_columns[] = {
        "@@character_set_system",
        "@@global.character_set_system",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const system_values[] = {
        "utf8mb3",
        "utf8mb3",
        "0",
        "-1",
    };
    static const char *const filesystem_columns[] = {
        "@@character_set_filesystem",
        "@@global.character_set_filesystem",
        "@@session.character_set_filesystem",
        "@@local.character_set_filesystem",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const filesystem_values[] = {
        "binary",
        "binary",
        "binary",
        "binary",
        "0",
        "-1",
    };
    static const char *const warning_columns[] = {
        "@@character_set_filesystem",
        "@@character_set_system",
        "@@character_set_database",
        "@@collation_database",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {
        "binary",
        "utf8mb3",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "1",
        "0",
        "-1",
    };
    static const char *const error_columns[] = {
        "@@character_set_filesystem",
        "@@character_set_system",
        "@@character_set_database",
        "@@collation_database",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const error_values[] = {
        "binary",
        "utf8mb3",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "1",
        "1",
        "-1",
    };
    static const char *const database_columns[] = {
        "@@character_set_filesystem",
        "@@character_set_system",
        "@@character_set_database",
        "@@collation_database",
        "DATABASE()",
    };
    static const char *const database_values[] = {
        "binary",
        "utf8mb3",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "app",
    };
    static const char *const dropped_database_values[] = {
        "binary",
        "utf8mb3",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        NULL,
    };
    static const char *const reopen_columns[] = {
        "@@character_set_client",
        "@@collation_connection",
        "@@character_set_server",
        "@@collation_server",
        "@@character_set_database",
        "@@collation_database",
        "@@character_set_system",
        "@@character_set_filesystem",
    };
    static const char *const reopen_values[] = {
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb3",
        "binary",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open charset variables file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(
        database,
        "SELECT @@character_set_client, @@character_set_connection, "
        "@@character_set_results, @@collation_connection, @@warning_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = charset_columns,
            .values = charset_values,
            .count = charset_variable_column_count,
            .context = "charset variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@session.character_set_client, @@local.character_set_connection, "
        "@@global.character_set_results, @@global.collation_connection FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .count = scoped_variable_column_count,
            .context = "scoped charset variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@character_set_server, @@global.character_set_server, "
        "@@session.character_set_server, @@local.character_set_server, "
        "@@collation_server, @@global.collation_server, @@session.collation_server, "
        "@@local.collation_server, @@warning_count, ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = server_columns,
            .values = server_values,
            .count = server_variable_column_count,
            .context = "server charset variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@character_set_database, @@global.character_set_database, "
        "@@session.character_set_database, @@local.character_set_database, "
        "@@collation_database, @@global.collation_database, @@session.collation_database, "
        "@@local.collation_database, @@warning_count, ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = database_full_columns,
            .values = database_full_values,
            .count = database_variable_full_column_count,
            .context = "database charset variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@character_set_system, @@global.character_set_system, @@warning_count, "
        "ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = system_columns,
            .values = system_values,
            .count = system_variable_column_count,
            .context = "system charset variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@character_set_filesystem, @@global.character_set_filesystem, "
        "@@session.character_set_filesystem, @@local.character_set_filesystem, "
        "@@warning_count, ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = filesystem_columns,
            .values = filesystem_values,
            .count = filesystem_variable_column_count,
            .context = "filesystem charset variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@character_set_filesystem, @@character_set_system, @@character_set_database, "
        "@@collation_database, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = warning_columns,
            .values = warning_values,
            .count = diagnostics_variable_column_count,
            .context = "charset variable warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_warnings(database, "0", "charset scalar clears warnings");

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "BAD",
        }
    );
    failures += execute_ok(
        database,
        "SELECT @@character_set_filesystem, @@character_set_system, @@character_set_database, "
        "@@collation_database, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = error_columns,
            .values = error_values,
            .count = diagnostics_variable_column_count,
            .context = "charset variable error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_errors(database, "0", "charset scalar clears errors");
    failures += expect_show_count_warnings(database, "0", "charset scalar clears error warnings");

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by charset reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by charset reads"
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_ok(
        database,
        "SELECT @@character_set_filesystem, @@character_set_system, @@character_set_database, "
        "@@collation_database, DATABASE()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = database_columns,
            .values = database_values,
            .count = database_variable_column_count,
            .context = "database charset variables with selected database",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "DROP DATABASE app");
    failures += execute_ok(
        database,
        "SELECT @@character_set_filesystem, @@character_set_system, @@character_set_database, "
        "@@collation_database, DATABASE()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = database_columns,
            .values = dropped_database_values,
            .count = database_variable_column_count,
            .context = "database charset variables after dropped selected database",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE numbers (id INT NOT NULL, value INT)");
    failures += execute_statement_ok(database, "INSERT INTO numbers VALUES (1, 2)");

    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen charset variables file");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_ok(
        database,
        "SELECT @@character_set_client, @@collation_connection, @@character_set_server, "
        "@@collation_server, @@character_set_database, @@collation_database, "
        "@@character_set_system, @@character_set_filesystem",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = reopen_columns,
            .values = reopen_values,
            .count = reopen_variable_column_count,
            .context = "reopened charset variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_select_numbers_row(database);

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read charset variable preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "charset variable preamble unchanged"
    );
    remove_related_files(path);

    return failures;
}

static int test_character_set_system_variable_qualifiers_and_errors(void) {
    static const char *const mixed_columns[] = {
        "@@CHARACTER_SET_CLIENT",
        "@@Session.Character_Set_Connection",
        "@@LOCAL.Character_Set_Results",
        "@@GLOBAL.Collation_Connection",
        "@@session.`character_set_client`",
        "@@`character_set_results`",
        "@@global.`collation_connection`",
        "@@CHARACTER_SET_SERVER",
        "@@Session.Collation_Server",
        "@@session.`character_set_server`",
        "@@global.`collation_server`",
        "@@CHARACTER_SET_DATABASE",
        "@@Session.Collation_Database",
        "@@session.`character_set_database`",
        "@@global.`collation_database`",
        "@@CHARACTER_SET_SYSTEM",
        "@@global.`character_set_system`",
        "@@CHARACTER_SET_FILESYSTEM",
        "@@session.`character_set_filesystem`",
    };
    static const char *const mixed_values[] = {
        "utf8mb4",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb3",
        "utf8mb3",
        "binary",
        "binary",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open charset variables memory");
    failures += execute_ok(
        database,
        "SELECT @@CHARACTER_SET_CLIENT, @@Session.Character_Set_Connection, "
        "@@LOCAL.Character_Set_Results, @@GLOBAL.Collation_Connection, "
        "@@session.`character_set_client`, @@`character_set_results`, "
        "@@global.`collation_connection`, @@CHARACTER_SET_SERVER, "
        "@@Session.Collation_Server, @@session.`character_set_server`, "
        "@@global.`collation_server`, @@CHARACTER_SET_DATABASE, "
        "@@Session.Collation_Database, @@session.`character_set_database`, "
        "@@global.`collation_database`, @@CHARACTER_SET_SYSTEM, "
        "@@global.`character_set_system`, @@CHARACTER_SET_FILESYSTEM, "
        "@@session.`character_set_filesystem`",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = mixed_variable_column_count,
            .context = "charset variable qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@no_such_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.`no_such_charset_variable`",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_server_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_server_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_server_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_server_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_database_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_database_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_database_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_database_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.character_set_system",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@local.character_set_system",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_system_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_system_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_system_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_system_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_filesystem_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_filesystem_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_filesystem_charset_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_filesystem_charset_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.character_set_client",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@character_set_filesystem + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_character_set_system_variable_handles(void) {
    static const char *const columns[] = {
        "@@character_set_client",
        "@@collation_connection",
        "@@character_set_server",
        "@@collation_server",
        "@@character_set_database",
        "@@collation_database",
        "@@character_set_system",
        "@@character_set_filesystem",
        "@@warning_count",
    };
    static const char *const first_values[] = {
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb3",
        "binary",
        "1",
    };
    static const char *const second_values[] = {
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "utf8mb3",
        "binary",
        "0",
    };
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first charset handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second charset handle");
    failures += execute_statement_ok(first, "SHOW PROCESSLIST");

    failures += execute_ok(
        first,
        "SELECT @@character_set_client, @@collation_connection, @@character_set_server, "
        "@@collation_server, @@character_set_database, @@collation_database, "
        "@@character_set_system, @@character_set_filesystem, @@warning_count",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = first_values,
            .count = reopen_variable_column_count + 1U,
            .context = "first handle charset variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        second,
        "SELECT @@character_set_client, @@collation_connection, @@character_set_server, "
        "@@collation_server, @@character_set_database, @@collation_database, "
        "@@character_set_system, @@character_set_filesystem, @@warning_count",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = second_values,
            .count = reopen_variable_column_count + 1U,
            .context = "second handle charset variables",
        }
    );
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), expected.count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }

    return failures;
}

static int expect_select_numbers_row(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT * FROM numbers", &result);

    failures += expect_size(mylite_result_column_count(result), 2U, "numbers column count");
    failures += expect_size(mylite_result_row_count(result), 1U, "numbers row count");
    failures +=
        expect_text_or_null(mylite_result_column_name(result, 0U), "id", "numbers id column");
    failures +=
        expect_text_or_null(mylite_result_column_name(result, 1U), "value", "numbers value column");
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), "1", "numbers id");
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 1U), "2", "numbers value");
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

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_count_errors(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) ERRORS", &result);

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got rc=%d err=%d state=%s message=%s\n",
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_character_set_system_variables_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long\n");
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return -1;
    }

    fclose(file);
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
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
