#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    show_columns_column_count = 6,
    explain_placeholder_column_count = 12,
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

static int test_admin_set_residual_runtime(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_admin_set_residual_runtime() == 0 ? 0 : 1;
}

static int test_admin_set_residual_runtime(void) {
    static const char *const describe_id[] = {"id", "int", "NO", "", NULL, ""};
    static const char *const describe_pattern[] = {"f1", "int", "YES", "", NULL, ""};
    static const char *const time_zone[] = {"UTC"};
    static const char *const explain_extra[] = {
        "1",
        "SIMPLE",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "MyLite EXPLAIN placeholder",
    };
    static const char *const explain_analyze[] = {"-> MyLite EXPLAIN ANALYZE placeholder\n"};
    struct expected_sql_error unsupported = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "utility statement is not supported",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t1 (id INT NOT NULL, f1 INT, name VARCHAR(20))");
    failures += execute_ok(database, "ANALYZE TABLES t1");
    failures += execute_ok(database, "OPTIMIZE TABLES t1");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE t1 id",
            .values = describe_id,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "describe identifier filter",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE t1 'f%'",
            .values = describe_pattern,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "describe string filter",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW EXTENDED COLUMNS FROM t1 LIKE 'id'",
            .values = describe_id,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "show extended columns",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE SELECT * FROM t1 WHERE id = 1",
            .values = explain_extra,
            .column_count = explain_placeholder_column_count,
            .row_count = 1U,
            .context = "describe select explain placeholder",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN ANALYZE DELETE FROM t1 WHERE id = 0",
            .values = explain_analyze,
            .column_count = 1U,
            .row_count = 1U,
            .context = "explain analyze dml placeholder",
        }
    );
    failures += execute_ok(database, "SET @@time_zone := 'UTC'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@time_zone",
            .values = time_zone,
            .column_count = 1U,
            .row_count = 1U,
            .context = "set system variable assignment operator",
        }
    );

    failures += execute_error(database, "SHOW ENGINE CSV LOGS", unsupported);
    failures += execute_error(database, "SHOW ENGINE MyISAM MUTEX", unsupported);
    failures += execute_error(database, "SHOW TRIGGERS WHERE 0", unsupported);
    failures += execute_error(database, "SHOW OPEN TABLES WHERE f1()=0", unsupported);
    failures += execute_error(
        database,
        "SET sql_mode = sys.LIST_ADD(@@sql_mode, 'ANSI_QUOTES')",
        unsupported
    );
    failures +=
        execute_error(database, "SET optimizer_switch=`mrr=on,mrr_cost_based=off`", unsupported);
    failures +=
        execute_error(database, "SET autocommit=0, PERSIST auto_increment_offset=10", unsupported);

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = expect_int(rc, MYLITE_OK, sql);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: %s / %s\n",
            query.context,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t value_index = (row * query.column_count) + column;
                failures += expect_text(
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

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
