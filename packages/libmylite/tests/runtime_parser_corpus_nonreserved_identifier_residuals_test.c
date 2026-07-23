#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    projection_column_count = 4,
    projection_row_count = 1,
    optimize_column_count = 4,
    optimize_row_count = 6,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_nonreserved_identifier_runtime(void);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);

int main(void) {
    return test_nonreserved_identifier_runtime() == 0 ? 0 : 1;
}

static int test_nonreserved_identifier_runtime(void) {
    static const char *const projection_values[] = {"1", "2", "3", "4"};
    static const char *const optimize_values[] = {
        "app.columns_priv",
        "optimize",
        "note",
        "Table does not support optimize, doing recreate + analyze instead",
        "app.columns_priv",
        "optimize",
        "status",
        "OK",
        "app.db",
        "optimize",
        "note",
        "Table does not support optimize, doing recreate + analyze instead",
        "app.db",
        "optimize",
        "status",
        "OK",
        "app.user",
        "optimize",
        "note",
        "Table does not support optimize, doing recreate + analyze instead",
        "app.user",
        "optimize",
        "status",
        "OK",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open test database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE t1 (current INT, diagnostics INT, number INT, returned_sqlstate INT)"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO t1 (current, diagnostics, number, returned_sqlstate) VALUES (1,2,3,4)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT current, diagnostics, number, returned_sqlstate "
                   "FROM t1 WHERE number = 3",
            .values = projection_values,
            .column_count = projection_column_count,
            .row_count = projection_row_count,
            .context = "nonreserved identifier projection",
        }
    );
    failures +=
        execute_statement_ok(database, "CREATE TABLE t0 (skip INT, locked INT, nowait INT)");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE diag_non_reserved ("
        "diagnostics INT, current INT, stacked INT, exception INT)"
    );
    failures += execute_statement_ok(database, "CREATE TABLE SESSION_USER(a INT)");
    failures += execute_statement_ok(database, "CREATE TABLE SYSTEM_USER(a INT)");
    failures += execute_statement_ok(database, "CREATE TABLE columns_priv(a INT)");
    failures += execute_statement_ok(database, "CREATE TABLE db(a INT)");
    failures += execute_statement_ok(database, "CREATE TABLE user(a INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "OPTIMIZE TABLES columns_priv, db, user",
            .values = optimize_values,
            .column_count = optimize_column_count,
            .row_count = optimize_row_count,
            .context = "optimize plural nonreserved targets",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "diagnostic: %d %s %s\n",
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
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
        fprintf(
            stderr,
            "diagnostic: %d %s %s\n",
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    if (rc == MYLITE_OK) {
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
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;
                failures += mylite_test_expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[value_index],
                    query.context
                );
            }
        }
    }

    mylite_result_free(result);
    return failures;
}
