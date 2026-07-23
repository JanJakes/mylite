#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    mysql_error_no_database_selected = 1046,
    mysql_error_duplicate_variable = 1331,
    mysql_error_unknown_system_variable = 1193,
    test_path_capacity = 1024,
    test_path_suffix_capacity = 8,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_local_variable_procedure_calls(void);
static int test_local_variable_procedure_errors(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_local_variable_procedure_calls();
    failures += test_local_variable_procedure_errors();

    return failures == 0 ? 0 : 1;
}

static int test_local_variable_procedure_calls(void) {
    static const char *const default_columns[] = {"a", "b", "c"};
    static const char *const default_values[] = {NULL, "7", "hi"};
    static const char *const set_columns[] = {"a", "b"};
    static const char *const set_values[] = {"5", "x5"};
    static const char *const multi_columns[] = {"a", "b"};
    static const char *const multi_values[] = {"3", "5"};
    static const char *const case_columns[] = {"Value"};
    static const char *const case_values[] = {"10"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "procedure_local_variables") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open local variable db");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE PROCEDURE p_defaults() "
        "BEGIN DECLARE a INT; DECLARE b INT DEFAULT 7; "
        "DECLARE c VARCHAR(10) DEFAULT 'hi'; SELECT a, b, c; END"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "CALL p_defaults()",
            .column_names = default_columns,
            .values = default_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "default local variable values",
        }
    );
    failures += execute_ok(database, "SHOW CREATE PROCEDURE p_defaults", &result);
    if (result != NULL && mylite_result_row_count(result) == 1U &&
        mylite_result_column_count(result) >= 3U) {
        failures += mylite_test_expect_contains(
            mylite_result_value_text(result, 0U, 2U),
            "DECLARE a INT",
            "show create local declarations"
        );
    } else {
        failures += 1;
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(
        database,
        "CREATE PROCEDURE p_set() "
        "BEGIN DECLARE a INT DEFAULT 1; DECLARE b VARCHAR(10) DEFAULT '5'; "
        "SET a = a + 4; SET b = CONCAT('x', b); SELECT a, b; END"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "CALL p_set()",
            .column_names = set_columns,
            .values = set_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "assigned local variable values",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE PROCEDURE p_multi() "
        "BEGIN DECLARE a, b INT DEFAULT 3; SET b = a + 2; SELECT a, b; END"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "CALL p_multi()",
            .column_names = multi_columns,
            .values = multi_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "multi-name local declaration",
        }
    );

    failures += execute_statement_ok(
        database,
        "CREATE PROCEDURE p_case() "
        "BEGIN DECLARE Value INT DEFAULT 9; SET value = value + 1; SELECT Value; END"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "CALL p_case()",
            .column_names = case_columns,
            .values = case_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "case-insensitive local variable",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_local_variable_procedure_errors(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "procedure_local_variable_errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open local errors db");
    failures += execute_error(
        database,
        "CREATE PROCEDURE p_no_db() BEGIN DECLARE a INT; SELECT a; END",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE PROCEDURE p_duplicate() "
        "BEGIN DECLARE a INT; DECLARE A INT; SELECT a; END",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_variable,
            .sqlstate = "42000",
            .message_part = "Duplicate variable: A",
        }
    );
    failures += execute_error(
        database,
        "CREATE PROCEDURE p_unknown_set() BEGIN SET missing = 1; SELECT missing; END",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'missing'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
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

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        failures += 1;
    } else {
        failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
        failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
        failures +=
            mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_index = 0U;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
    }
    for (size_t column = 0U; failures == 0 && column < query.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            query.column_names[column],
            query.context
        );
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; failures == 0 && column < query.column_count; ++column) {
            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
            ++value_index;
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL && actual == NULL) {
        return 0;
    }
    if (expected == NULL || actual == NULL) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}
