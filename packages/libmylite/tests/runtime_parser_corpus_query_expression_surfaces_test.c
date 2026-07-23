#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_not_supported_yet = 1235,
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

static int test_query_expression_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);

int main(void) {
    return test_query_expression_surfaces() == 0 ? 0 : 1;
}

static int test_query_expression_surfaces(void) {
    static const char *const wrapped_select_rows[] = {"1"};
    static const char *const wrapped_values_rows[] = {"7"};
    static const char *const selected_variable_rows[] = {"1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transient database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t1 (id INT, shared INT)");
    failures += execute_ok(database, "CREATE TABLE t2 (id INT, shared INT)");
    failures += execute_ok(database, "INSERT INTO t1 VALUES (1, 100), (2, 200)");
    failures += execute_ok(database, "INSERT INTO t2 VALUES (1, 100), (3, 300)");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "(SELECT 1 AS x)",
            .values = wrapped_select_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "simple parenthesized SELECT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "((SELECT 1 AS x))",
            .values = wrapped_select_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "nested parenthesized SELECT",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "(VALUES ROW(7))",
            .values = wrapped_values_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "simple parenthesized VALUES",
        }
    );

    failures += execute_error(
        database,
        "(SELECT 1 AS x) ORDER BY x",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "outer ORDER BY or LIMIT",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM t1 JOIN t2 USING (shared)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "USING join conditions",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM t1 NATURAL JOIN t2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "NATURAL joins",
        }
    );
    failures += execute_error(
        database,
        "CREATE VIEW v_union AS SELECT id FROM t1 UNION SELECT id FROM t2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CREATE VIEW supports only a single SELECT statement",
        }
    );
    failures += execute_ok(database, "CREATE VIEW v_table AS TABLE t1");
    failures += execute_ok(database, "CREATE VIEW v_plain AS SELECT id FROM t1");
    failures += execute_error(
        database,
        "ALTER VIEW v_plain AS VALUES ROW(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CREATE VIEW supports only a single SELECT statement",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM (VALUES ROW(1)) AS dt(a)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "derived tables support only SELECT",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM (VALUES ROW(1) UNION SELECT 2) AS dt(a)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += execute_error(
        database,
        "VALUES ROW(1) UNION SELECT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNION supports only SELECT query blocks",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT 1 UNION SELECT 1 LIMIT 0",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "compound query limit tail",
        }
    );
    failures += execute_error(
        database,
        "select id from t1 union all select 99 order by 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "UNION ORDER BY supports only output column names",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 FROM DUAL LIMIT 1 INTO @var FOR UPDATE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_ok(database, "SET @var = NULL");
    failures += execute_ok(database, "SELECT 1 FROM DUAL LIMIT 1 FOR UPDATE INTO @var");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @var",
            .values = selected_variable_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "select limit locking before into",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 UNION SELECT 1 FOR UPDATE INTO @var",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 UNION SELECT 1 FROM DUAL INTO @var FOR UPDATE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT LEAD(id) IGNORE NULLS OVER (ORDER BY id) FROM t1",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "IGNORE NULLS",
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

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "failed result columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", query.sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (failures == 0 && query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += mylite_test_expect_text(
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
