#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_text {
    const char *sql;
    const char *expected;
    const char *context;
};

static int test_expression_value_placeholders(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_scalar_text(mylite_db *database, struct expected_scalar_text expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_expression_value_placeholders() == 0 ? 0 : 1;
}

static int test_expression_value_placeholders(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE articles ("
        "title TEXT, body TEXT, FULLTEXT KEY ft_articles (title, body))"
    );
    failures += execute_ok(database, "CREATE TABLE sales (region VARCHAR(16), amount INT)");
    failures += execute_ok(database, "INSERT INTO sales VALUES ('east', 10), ('west', 20)");
    failures += execute_ok(database, "CREATE TABLE spatial_values (g GEOMETRY)");
    failures += execute_ok(database, "CREATE TABLE dml_values (id INT, v VARCHAR(64), n INT)");
    failures += execute_ok(
        database,
        "INSERT INTO dml_values VALUES "
        "(1, REPLACE('abc', 'b', 'B'), 5), "
        "(2, REGEXP_REPLACE('abc', 'b', 'B'), LAST_INSERT_ID(11))"
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text){
            .sql = "SELECT v FROM dml_values WHERE id = 1",
            .expected = "aBc",
            .context = "DML REPLACE() value",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text){
            .sql = "SELECT v FROM dml_values WHERE id = 2",
            .expected = "aBc",
            .context = "DML REGEXP_REPLACE() value",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text){
            .sql = "SELECT n FROM dml_values WHERE id = 2",
            .expected = "11",
            .context = "DML LAST_INSERT_ID(expr) stored value",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text){
            .sql = "SELECT LAST_INSERT_ID()",
            .expected = "11",
            .context = "DML LAST_INSERT_ID(expr) side effect",
        }
    );

    failures += execute_error(
        database,
        "SELECT MATCH(title, body) AGAINST ('needle' IN BOOLEAN MODE) FROM articles",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "support",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO spatial_values VALUES (ST_GeomFromText('POINT(1 1)'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "support",
        }
    );
    failures += execute_error(
        database,
        "SELECT region, SUM(amount) FROM sales GROUP BY region WITH ROLLUP",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY WITH ROLLUP is not yet supported",
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

static int expect_scalar_text(mylite_db *database, struct expected_scalar_text expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d: %s\n",
            expected.sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 0U), expected.expected, expected.context);
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
