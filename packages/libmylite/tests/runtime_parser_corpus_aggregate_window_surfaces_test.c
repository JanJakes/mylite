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

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_aggregate_window_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_aggregate_window_surfaces() == 0 ? 0 : 1;
}

static int test_aggregate_window_surfaces(void) {
    static const char *const supported_sum_rows[] = {"30"};
    static const char *const supported_sum_window_rows[] = {"2", "2"};
    static const char *const supported_group_concat_rows[] = {"ann|bob"};
    static const char *const supported_group_concat_multi_arg_rows[] = {"ann|,bob|"};
    static const char *const supported_statistical_window_rows[] = {
        NULL,
        "7.0710678118654755",
    };
    static const char *const nth_from_first_rows[] = {"1", "1", "2", "1"};
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
        "CREATE TABLE numbers ("
        "id INT NOT NULL, a INT, b INT, c INT, d INT, k INT, j INT, name VARCHAR(16), dt DATE)"
    );
    failures += execute_ok(
        database,
        "INSERT INTO numbers VALUES "
        "(1,1,2,3,4,1,10,'ann','2024-01-01'), "
        "(2,2,4,6,8,1,20,'bob','2024-01-02')"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT SUM(j) FROM numbers",
            .values = supported_sum_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "existing supported SUM subset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM numbers",
            .values = supported_group_concat_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "existing supported GROUP_CONCAT subset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, NTH_VALUE(id, 1) FROM FIRST OVER (ORDER BY id) "
                   "FROM numbers ORDER BY id",
            .values = nth_from_first_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "NTH_VALUE FROM FIRST",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT SUM(1) OVER () FROM numbers",
            .values = supported_sum_window_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "supported SUM aggregate window subset",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*) OVER (ORDER BY dt RANGE INTERVAL 1 DAY PRECEDING) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "COUNT() RANGE frame offsets are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAYAGG(j) OVER () FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate window functions are not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT STDDEV_SAMP(j) OVER (ORDER BY id) FROM numbers",
            .values = supported_statistical_window_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "supported STDDEV_SAMP aggregate window subset",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM numbers ORDER BY RANK() OVER (ORDER BY a DESC,b,c)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "window functions support only descriptor columns in ORDER BY",
        }
    );
    failures += execute_error(
        database,
        "SELECT SUM(c/d) FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate row-scalar arguments do not support / division",
        }
    );
    failures += execute_error(
        database,
        "SELECT k, SUM(j) FROM numbers GROUP BY (k)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports only descriptor group columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name,'|') FROM numbers",
            .values = supported_group_concat_multi_arg_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "supported GROUP_CONCAT multi-argument row value",
        }
    );
    failures += execute_error(
        database,
        "SELECT GROUP_CONCAT(name,'|') FROM numbers GROUP BY 'x' WITH ROLLUP",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "GROUP BY supports only descriptor group columns",
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
