#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_select_clause_residuals(void);
static int execute_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);

int main(void) {
    return test_select_clause_residuals() == 0 ? 0 : 1;
}

static int test_select_clause_residuals(void) {
    static const char *const tableless_one[] = {"1"};
    static const char *const tableless_expr[] = {"9"};
    static const char *const constant_order_rows[] = {"1", "2"};
    static const char *const locking_rows[] = {"1", "1", "2", "2"};
    static const char *const having_string_rhs_rows[] = {"10", "hello"};
    static const char *const having_alias_equal_rows[] = {"10", "10"};
    static const char *const having_alias_not_equal_rows[] = {"20", "30"};
    static const char *const having_in_rows[] = {"10", "20"};
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
    failures += execute_ok(database, "CREATE TABLE t1 (id INT PRIMARY KEY, a INT, b VARCHAR(20))");
    failures += execute_ok(database, "CREATE TABLE t2 (id INT PRIMARY KEY, a INT, b VARCHAR(20))");
    failures += execute_ok(database, "INSERT INTO t1 VALUES (1, 10, 'hello'), (2, 20, 'world')");
    failures += execute_ok(database, "INSERT INTO t2 VALUES (1, 10, 'x'), (2, 30, 'y')");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT 1 LIMIT 1",
            .values = tableless_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "tableless limit one",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT 1 LIMIT 0",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "tableless limit zero",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT 1 LIMIT 1, 10",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "tableless limit offset",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT 9 FROM DUAL WHERE 1 LIMIT 1",
            .values = tableless_expr,
            .column_count = 1U,
            .row_count = 1U,
            .context = "tableless filtered limit",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT t1.id, t2.id FROM t1 JOIN t2 ON t1.id=t2.id "
                   "FOR SHARE OF t1 FOR UPDATE OF t2",
            .values = locking_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "multi locking clauses",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t1 ORDER BY NULL",
            .values = constant_order_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "constant order null",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t1 ORDER BY 'a' DESC",
            .values = constant_order_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "constant order string",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t1 ORDER BY @rank",
            .values = constant_order_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "user variable order key",
        }
    );
    failures += execute_ok(database, "SET @rank = 7");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t1 ORDER BY @rank DESC",
            .values = constant_order_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "assigned user variable order key",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a,b FROM t1 GROUP BY a,b HAVING b='hello'",
            .values = having_string_rhs_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "having string rhs",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT t1.a AS t1c1, t2.a AS t2c1 FROM t1 JOIN t2 ON t1.id=t2.id "
                   "HAVING t1c1 = t2c1",
            .values = having_alias_equal_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "joined having alias equality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT t1.a AS t1c1, t2.a AS t2c1 FROM t1 JOIN t2 ON t1.id=t2.id "
                   "HAVING t1c1 != t2c1",
            .values = having_alias_not_equal_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "joined having alias inequality",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a FROM t1 GROUP BY a HAVING a IN (10,20) ORDER BY a",
            .values = having_in_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "having in",
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
