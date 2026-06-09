#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    scalar_expression_column_count = 10,
    mysql_error_parse = 1064,
    mysql_error_truncated_wrong_value = 1366,
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_scalar_expression_insert_replace_update(void);
static int test_scalar_expression_diagnostics(void);
static int open_app_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_scalar_expression_insert_replace_update();
    failures += test_scalar_expression_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_expression_insert_replace_update(void) {
    static const char *const inserted_rows[] = {
        "1",
        "7",
        "beta",
        "AB",
        "12.00",
        "8",
        "{\"a\": 1}",
        "2024-01-02 03:04:05",
        "00:00:59",
        "1",
    };
    static const char *const duplicate_rows[] = {"1", "8", "dup", "1"};
    static const char *const replace_rows[] = {
        "1", "8",  "dup", "AB", "12.00", "8", "{\"a\": 1}", "2024-01-02 03:04:05", "00:00:59", "1",
        "2", "10", "x-b", "EF", "3.00",  "3", "[2, \"b\"]", "2024-01-03 04:05:06", "00:01:01", "1",
    };
    static const char *const updated_rows[] = {
        "1",
        "4",
        "b",
        "CD",
        "19.00",
        "0",
        "{\"updated\": true}",
        "2024-01-02 03:04:06",
        "00:01:00",
        "2",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE exprs("
        "id INT PRIMARY KEY, "
        "i INT, "
        "v VARCHAR(64), "
        "b VARBINARY(16), "
        "d DECIMAL(8,2), "
        "f DOUBLE, "
        "js JSON, "
        "dt DATETIME, "
        "tm TIME, "
        "flag INT"
        ")"
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO exprs(id, i, v, b, d, f, js, dt, tm, flag) VALUES ("
        "1, "
        "1 + 2 * 3, "
        "GREATEST('alpha', 'beta'), "
        "UNHEX('4142'), "
        "ABS(-12), "
        "POW(2, 3), "
        "JSON_OBJECT('a', 1), "
        "DATE_ADD('2024-01-02 03:04:04', INTERVAL 1 SECOND), "
        "SEC_TO_TIME(59), "
        "IF(1, 1, 0)"
        ")",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, v, b, d, f, js, dt, tm, flag "
                   "FROM exprs WHERE id = 1",
            .values = inserted_rows,
            .column_count = scalar_expression_column_count,
            .row_count = 1U,
            .context = "insert scalar expression row",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO exprs(id, i, v, b, d, f, js, dt, tm, flag) VALUES "
        "(1, 0, 'ignored', X'', 0, 0, JSON_OBJECT(), '2024-01-01', '00:00:00', 0) "
        "ON DUPLICATE KEY UPDATE "
        "i = GREATEST(5, 8), "
        "v = GREATEST('abc', 'dup'), "
        "flag = IF(1, 1, 0)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, v, flag FROM exprs WHERE id = 1",
            .values = duplicate_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "duplicate scalar expression row",
        }
    );
    failures += expect_dml_result(
        database,
        "REPLACE INTO exprs(id, i, v, b, d, f, js, dt, tm, flag) VALUES ("
        "2, "
        "IF(1, 10, 20), "
        "CONCAT_WS('-', 'x', 'b'), "
        "UNHEX('4546'), "
        "FLOOR(3), "
        "LOG2(8), "
        "JSON_ARRAY(2, 'b'), "
        "TIMESTAMP('2024-01-03', '04:05:06'), "
        "SEC_TO_TIME(61), "
        "IF(0, 0, 1)"
        ")",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, v, b, d, f, js, dt, tm, flag "
                   "FROM exprs ORDER BY id",
            .values = replace_rows,
            .column_count = scalar_expression_column_count,
            .row_count = 2U,
            .context = "replace scalar expression rows",
        }
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET i = BIT_COUNT(15) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET v = LEAST('b', 'c') WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET b = UNHEX('4344') WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET d = ROUND(19, 0) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET f = ACOS(1) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET js = JSON_OBJECT('updated', TRUE) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET dt = TIMESTAMP('2024-01-02', '03:04:06') WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET tm = ADDTIME('00:00:31', '00:00:29') WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_result(
        database,
        "UPDATE exprs SET flag = IF(1, 2, 0) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, v, b, d, f, js, dt, tm, flag "
                   "FROM exprs WHERE id = 1",
            .values = updated_rows,
            .column_count = scalar_expression_column_count,
            .row_count = 1U,
            .context = "update scalar expression row",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_scalar_expression_diagnostics(void) {
    static const char *const ignore_rows[] = {"3", "0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database);
    failures +=
        expect_statement_ok(database, "CREATE TABLE exprs(id INT PRIMARY KEY, i INT NOT NULL)");
    failures += execute_error(
        database,
        "INSERT INTO exprs(id, i) VALUES (2, GREATEST((SELECT 1), 2))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT supports only integer",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO exprs(id, i) VALUES (4, GREATEST('abc', 'def'))",
        (struct expected_sql_error){
            .code = mysql_error_truncated_wrong_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO exprs(id, i) VALUES (3, GREATEST('abc', 'def'))",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM exprs WHERE id = 3",
            .values = ignore_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ignore scalar expression row",
        }
    );

    mylite_close(database);
    return failures;
}

static int open_app_database(mylite_db **out_database) {
    int rc = mylite_test_open_temporary(out_database);

    if (rc != MYLITE_OK) {
        return expect_int(rc, MYLITE_OK, "open temporary database");
    }
    return expect_statement_ok(*out_database, "CREATE DATABASE app") +
           expect_statement_ok(*out_database, "USE app");
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected SQL to succeed: %s\nerror %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected SQL to fail: %s\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return execute_ok(database, sql, NULL);
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            "affected rows"
        );
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            "warning count"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t expected_value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t index = 0U; index < expected_value_count; ++index) {
            failures += expect_result_value(
                result,
                index / query.column_count,
                index % query.column_count,
                query.values[index],
                query.context
            );
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL at %zu/%zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
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
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
