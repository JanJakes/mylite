#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_select_row_scalar_predicates(void);
static int open_app_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);

int main(void) {
    return test_select_row_scalar_predicates() == 0 ? 0 : 1;
}

static int test_select_row_scalar_predicates(void) {
    static const char *const ids_1[] = {"1"};
    static const char *const ids_2[] = {"2"};
    static const char *const ids_123[] = {"1", "2", "3"};
    static const char *const ids_13[] = {"1", "3"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE expr_pred("
        "id INT PRIMARY KEY, "
        "i INT, "
        "v VARCHAR(64), "
        "b VARBINARY(16), "
        "js JSON, "
        "dt DATETIME, "
        "tm TIME"
        ")"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO expr_pred(id, i, v, b, js, dt, tm) VALUES "
        "(1, 10, 'Alpha', UNHEX('4142'), JSON_OBJECT('a', 1), "
        "'2024-01-02 03:04:05', '00:00:59'), "
        "(2, 0, 'beta', UNHEX('4344'), JSON_OBJECT('a', 2), "
        "'2024-01-03 04:05:06', '00:01:01'), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL)"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE HEX(b) = '4142' ORDER BY id",
            .values = ids_1,
            .column_count = 1U,
            .row_count = 1U,
            .context = "hex equality predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE LOWER(v) = 'alpha' ORDER BY id",
            .values = ids_1,
            .column_count = 1U,
            .row_count = 1U,
            .context = "lower equality predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE UPPER(v) = UPPER('beta') ORDER BY id",
            .values = ids_2,
            .column_count = 1U,
            .row_count = 1U,
            .context = "upper rhs predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE COALESCE(i, 0) BETWEEN 0 AND 10 "
                   "ORDER BY id",
            .values = ids_123,
            .column_count = 1U,
            .row_count = 3U,
            .context = "coalesce between predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE GREATEST(i, 5) = 10 ORDER BY id",
            .values = ids_1,
            .column_count = 1U,
            .row_count = 1U,
            .context = "greatest equality predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE LEAST(i, 5) NOT BETWEEN 0 AND 4 "
                   "ORDER BY id",
            .values = ids_1,
            .column_count = 1U,
            .row_count = 1U,
            .context = "least not between predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE NULLIF(v, 'Alpha') IS NULL ORDER BY id",
            .values = ids_13,
            .column_count = 1U,
            .row_count = 2U,
            .context = "nullif is null predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE IFNULL(v, 'fallback') IS NOT NULL "
                   "ORDER BY id",
            .values = ids_123,
            .column_count = 1U,
            .row_count = 3U,
            .context = "ifnull is not null predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE TIMESTAMP(dt) BETWEEN "
                   "'2024-01-02 00:00:00' AND '2024-01-02 23:59:59' ORDER BY id",
            .values = ids_1,
            .column_count = 1U,
            .row_count = 1U,
            .context = "timestamp between predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE DATEDIFF(dt, '2024-01-01') = 1 "
                   "ORDER BY id",
            .values = ids_1,
            .column_count = 1U,
            .row_count = 1U,
            .context = "datediff equality predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM expr_pred WHERE JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')) = '2' "
                   "ORDER BY id",
            .values = ids_2,
            .column_count = 1U,
            .row_count = 1U,
            .context = "json extract equality predicate",
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return execute_ok(database, sql, NULL);
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

    return expect_text(actual, expected, context);
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
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}
