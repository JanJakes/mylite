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
    version_system_variable_column_count = 7,
    version_system_variable_quoted_column_count = 4,
    version_system_variable_diagnostics_column_count = 5,
    diagnostics_count_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_session_variable_only = 1238,
    mysql_error_incorrect_parameter_count = 1582,
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

static int test_version_function_values(void);
static int test_version_function_independent_handles(void);
static int test_version_function_unsupported_forms(void);
static int test_version_system_variable_values_and_persistence(void);
static int test_version_system_variable_independent_handles(void);
static int test_version_system_variable_scope_and_errors(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
);
static int expect_diagnostics_counts_cleared(mylite_db *database, const char *context);
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
static int expect_true(int condition, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_version_function_values();
    failures += test_version_function_independent_handles();
    failures += test_version_function_unsupported_forms();
    failures += test_version_system_variable_values_and_persistence();
    failures += test_version_system_variable_independent_handles();
    failures += test_version_system_variable_scope_and_errors();

    return failures == 0 ? 0 : 1;
}

static int test_version_function_values(void) {
    static const char *const version_columns[] = {"VERSION()"};
    static const char *const version_values[] = {MYLITE_VERSION_STRING};
    static const char *const lower_columns[] = {"version()"};
    static const char *const mixed_case_columns[] = {"Version()"};
    static const char *const spaced_columns[] = {"VERSION ()"};
    static const char *const parenthesized_columns[] = {"(VERSION())"};
    static const char *const mixed_columns[] =
        {"VERSION()", "DATABASE()", "USER()", "CURRENT_USER"};
    static const char *const mixed_no_schema_values[] = {
        MYLITE_VERSION_STRING,
        NULL,
        "root@%",
        "root@%",
    };
    static const char *const mixed_app_values[] = {
        MYLITE_VERSION_STRING,
        "app",
        "root@%",
        "root@%",
    };
    static const char *const version_database_columns[] = {"VERSION()", "DATABASE()"};
    static const char *const version_app_values[] = {MYLITE_VERSION_STRING, "app"};
    static const char *const version_no_schema_values[] = {MYLITE_VERSION_STRING, NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");

    failures += execute_ok(database, "SELECT VERSION()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_columns,
            .values = version_values,
            .count = 1U,
            .context = "version value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT ALL VERSION()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_columns,
            .values = version_values,
            .count = 1U,
            .context = "all version value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT version() FROM DUAL", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = lower_columns,
            .values = version_values,
            .count = 1U,
            .context = "lower version value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT ALL version() FROM DUAL", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = lower_columns,
            .values = version_values,
            .count = 1U,
            .context = "all lower version dual value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT Version()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_case_columns,
            .values = version_values,
            .count = 1U,
            .context = "mixed-case version value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT VERSION ()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = spaced_columns,
            .values = version_values,
            .count = 1U,
            .context = "spaced version value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT (VERSION())", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = parenthesized_columns,
            .values = version_values,
            .count = 1U,
            .context = "parenthesized version value",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT VERSION(), DATABASE(), USER(), CURRENT_USER", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_no_schema_values,
            .count = 4U,
            .context = "mixed scalar functions without selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT VERSION(), DATABASE(), USER(), CURRENT_USER", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = mixed_columns,
            .values = mixed_app_values,
            .count = 4U,
            .context = "mixed scalar functions with selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DROP DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT VERSION(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_database_columns,
            .values = version_no_schema_values,
            .count = 2U,
            .context = "drop clears schema but not version",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    mylite_close(database);
    database = NULL;
    result = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_ok(database, "SELECT VERSION(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_database_columns,
            .values = version_no_schema_values,
            .count = 2U,
            .context = "version after reopen without selected schema",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT VERSION(), DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_database_columns,
            .values = version_app_values,
            .count = 2U,
            .context = "version after reopen with selected schema",
        }
    );
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_version_function_independent_handles(void) {
    static const char *const version_columns[] = {"VERSION()"};
    static const char *const version_values[] = {MYLITE_VERSION_STRING};
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "independent") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second handle");

    failures += execute_ok(first, "SELECT VERSION()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_columns,
            .values = version_values,
            .count = 1U,
            .context = "first handle version",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT VERSION()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_columns,
            .values = version_values,
            .count = 1U,
            .context = "second handle version",
        }
    );
    mylite_result_free(result);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(path);

    return failures;
}

static int test_version_function_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE t (id INT)", &result);
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT VERSION(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'VERSION'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'VERSION'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION('x')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'VERSION'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'VERSION'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION() LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION() FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_version_system_variable_values_and_persistence(void) {
    static const char *const version_columns[] = {
        "@@version",
        "@@global.version",
        "@@version_comment",
        "@@global.version_comment",
        "VERSION()",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const version_values[] = {
        MYLITE_VERSION_STRING,
        MYLITE_VERSION_STRING,
        "MyLite",
        "MyLite",
        MYLITE_VERSION_STRING,
        "0",
        "-1",
    };
    static const char *const quoted_columns[] = {
        "@@VERSION",
        "@@GLOBAL.VERSION_COMMENT",
        "@@`version`",
        "@@global.`version_comment`",
    };
    static const char *const quoted_values[] = {
        MYLITE_VERSION_STRING,
        "MyLite",
        MYLITE_VERSION_STRING,
        "MyLite",
    };
    static const char *const diagnostic_columns[] = {
        "@@version",
        "@@version_comment",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {
        MYLITE_VERSION_STRING,
        "MyLite",
        "1",
        "0",
        "-1",
    };
    static const char *const error_values[] = {
        MYLITE_VERSION_STRING,
        "MyLite",
        "1",
        "1",
        "-1",
    };
    static const char *const reopen_columns[] = {"@@version", "@@version_comment", "DATABASE()"};
    static const char *const reopen_values[] = {MYLITE_VERSION_STRING, "MyLite", NULL};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "system_variables") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open version variables file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(
        database,
        "SELECT @@version, @@global.version, @@version_comment, "
        "@@global.version_comment, VERSION(), @@warning_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = version_columns,
            .values = version_values,
            .count = version_system_variable_column_count,
            .context = "version system variable values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@VERSION, @@GLOBAL.VERSION_COMMENT, @@`version`, "
        "@@global.`version_comment` FROM DUAL",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = quoted_columns,
            .values = quoted_values,
            .count = version_system_variable_quoted_column_count,
            .context = "version system variable quoted values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@version, @@version_comment, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = diagnostic_columns,
            .values = warning_values,
            .count = version_system_variable_diagnostics_column_count,
            .context = "version system variable warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_diagnostics_counts_cleared(database, "version variable clears warnings");

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
        "SELECT @@version, @@version_comment, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = diagnostic_columns,
            .values = error_values,
            .count = version_system_variable_diagnostics_column_count,
            .context = "version system variable error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_diagnostics_counts_cleared(database, "version variable clears errors");

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by version variable reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by version variable reads"
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE numbers (id INT NOT NULL)");
    failures += execute_statement_ok(database, "INSERT INTO numbers VALUES (1)");

    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen version variables file");
    failures += execute_ok(database, "SELECT @@version, @@version_comment, DATABASE()", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = reopen_columns,
            .values = reopen_values,
            .count = diagnostics_count_column_count,
            .context = "version variables after reopen",
        }
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read version variable preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "version variable preamble unchanged"
    );
    remove_related_files(path);

    return failures;
}

static int test_version_system_variable_independent_handles(void) {
    static const char *const columns[] = {"@@version", "@@version_comment", "@@warning_count"};
    static const char *const first_values[] = {MYLITE_VERSION_STRING, "MyLite", "1"};
    static const char *const second_values[] = {MYLITE_VERSION_STRING, "MyLite", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first version handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second version handle");
    failures += execute_statement_ok(first, "SHOW PROCESSLIST");

    failures += execute_ok(first, "SELECT @@version, @@version_comment, @@warning_count", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = first_values,
            .count = diagnostics_count_column_count,
            .context = "first handle version variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT @@version, @@version_comment, @@warning_count", &result);
    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = second_values,
            .count = diagnostics_count_column_count,
            .context = "second handle version variables",
        }
    );
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int test_version_system_variable_scope_and_errors(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open version errors memory");

    failures += execute_error(
        database,
        "SELECT @@session.version",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@local.version",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.version_comment",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@local.version_comment",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_version_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_version_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_version_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_version_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_version_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_version_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`global`.version",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@version + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@version LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );

    mylite_close(database);
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
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
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, "error");
    mylite_result_free(result);

    return failures;
}

static int expect_scalar_result(
    const mylite_result *result,
    struct expected_scalar_result expected
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), expected.count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    return failures;
}

static int expect_diagnostics_counts_cleared(mylite_db *database, const char *context) {
    static const char *const columns[] = {"@@warning_count", "@@error_count", "ROW_COUNT()"};
    static const char *const values[] = {"0", "0", "-1"};
    mylite_result *result = NULL;
    int failures =
        execute_ok(database, "SELECT @@warning_count, @@error_count, ROW_COUNT()", &result);

    failures += expect_scalar_result(
        result,
        (struct expected_scalar_result){
            .columns = columns,
            .values = values,
            .count = diagnostics_count_column_count,
            .context = context,
        }
    );
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
        "%s/mylite_version_function_%d_%s.mylite",
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
            "%s: expected [%s], got [%s]\n",
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
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
