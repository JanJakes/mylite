#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_unknown_character_set = 1115,
    mysql_error_operand_should_contain_one_column = 1241,
    mysql_error_regexp_character_set_mismatch = 3995,
    row_not_comparison_column_count = 2,
    row_string_comparison_column_count = 5,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_expression_query_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_expression_query_surfaces() == 0 ? 0 : 1;
}

static int test_expression_query_surfaces(void) {
    static const char *const scalar_rows[] = {"1", "0", "1"};
    static const char *const row_comparison_values[] = {"1", "0", "1"};
    static const char *const row_null_comparison_values[] = {NULL, "1", "1"};
    static const char *const parenthesized_row_comparison_values[] = {"1", "0", "1"};
    static const char *const parenthesized_row_null_comparison_values[] = {NULL, "1", "1"};
    static const char *const row_not_comparison_values[] = {"1", NULL};
    static const char *const row_string_comparison_values[] = {"1", "1", "1", "0", "1"};
    static const char *const logical_not_values[] = {"5", "1", "0"};
    static const char *const sum_distinct_values[] = {"30"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t (id INT PRIMARY KEY, c VARCHAR(16), n INT)");
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'abc', 10), (2, 'def', 20)");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT 'abc' REGEXP 'B', BINARY 'abc' REGEXP BINARY 'B', "
                   "'abc' NOT REGEXP 'z'",
            .values = scalar_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "scalar REGEXP",
        }
    );
    failures += execute_error(
        database,
        "SELECT BINARY 'abc' REGEXP 'B'",
        (struct expected_sql_error){
            .code = mysql_error_regexp_character_set_mismatch,
            .sqlstate = "HY000",
            .message_part = "cannot be used in conjunction",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT !0 * 5, !1 + 1, NOT 1 + 1",
            .values = logical_not_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "logical not operator precedence",
        }
    );

    failures += execute_error(
        database,
        "SELECT ROW(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 1 column(s)",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW(1, 2) = ROW(1, 2), ROW(1, 2) = ROW(1, 3), "
                   "ROW(1, 2) <> ROW(1, 3)",
            .values = row_comparison_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "row constructor comparison",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW(1, NULL) = ROW(1, NULL), "
                   "ROW(1, NULL) <=> ROW(1, NULL), ROW(2, NULL) > ROW(1, 9)",
            .values = row_null_comparison_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "row constructor NULL comparison",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT (1, 2) = (1, 2), (1, 2) = (1, 3), (1, 2) <> (1, 3)",
            .values = parenthesized_row_comparison_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "parenthesized row constructor comparison",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT (1, NULL) = (1, NULL), "
                   "(1, NULL) <=> (1, NULL), (2, NULL) > (1, 9)",
            .values = parenthesized_row_null_comparison_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "parenthesized row constructor NULL comparison",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT NOT ROW(1, 2) = ROW(1, 3), "
                   "NOT ROW(1, NULL) = ROW(1, NULL)",
            .values = row_not_comparison_values,
            .column_count = row_not_comparison_column_count,
            .row_count = 1U,
            .context = "row constructor NOT comparison",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW('b', 1) > ROW('a', 9), ROW('b', 1) = ROW('B', 1), "
                   "ROW('b ', 1) = ROW('b', 1), ROW('b', 1) > ROW('B', 9), "
                   "ROW('123', 1) = ROW(123, 1)",
            .values = row_string_comparison_values,
            .column_count = row_string_comparison_column_count,
            .row_count = 1U,
            .context = "row constructor string comparison",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROW(1, 2) = ROW(1, 2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 2 column(s)",
        }
    );
    failures += execute_error(
        database,
        "SELECT (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 1 column(s)",
        }
    );
    failures += execute_error(
        database,
        "SELECT CHAR(0x41 USING ucs2)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'ucs2'",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUPING(c) FROM t GROUP BY c WITH ROLLUP",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT c, COUNT(*) FROM t GROUP BY 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT SUM(DISTINCT n) FROM t",
            .values = sum_distinct_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "distinct sum aggregate",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(DISTINCT c ORDER BY c) FROM t GROUP BY c",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "support",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO t VALUES (1, 'abc', 30) ON DUPLICATE KEY UPDATE n = GREATEST(n, VALUES(n))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "support",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "failed result columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", query.sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (failures == 0 && query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
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
