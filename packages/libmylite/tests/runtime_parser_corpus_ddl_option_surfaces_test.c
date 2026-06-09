#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_wrong_usage = 1221,
    mysql_error_must_have_visible_column = 4028,
    show_columns_column_count = 6,
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

static int test_ddl_option_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_ddl_option_surfaces() == 0 ? 0 : 1;
}

static int test_ddl_option_surfaces(void) {
    static const char *const hidden_column_rows[] = {
        "hidden_col",
        "int",
        "YES",
        "",
        NULL,
        "INVISIBLE",
    };
    static const char *const hidden_added_rows[] = {
        "hidden_added",
        "int",
        "YES",
        "",
        NULL,
        "INVISIBLE",
    };
    static const char *const modified_column_rows[] = {
        "c",
        "bigint",
        "YES",
        "",
        NULL,
        "INVISIBLE",
    };
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
        "CREATE TABLE option_t (id INT) TABLESPACE innodb_file_per_table STORAGE DISK "
        "ENGINE=InnoDB"
    );
    failures += execute_ok(
        database,
        "ALTER TABLE option_t TABLESPACE innodb_file_per_table STORAGE DISK, ENGINE=InnoDB"
    );
    failures += execute_ok(database, "ALTER TABLE option_t STORAGE MEMORY");

    failures += execute_ok(
        database,
        "CREATE TABLE visible_t (id INT VISIBLE, hidden_col INT INVISIBLE, g GEOMETRY SRID 0)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM visible_t LIKE 'hidden_col'",
            .values = hidden_column_rows,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "inline invisible column metadata",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE invalid_all_invisible (only_col INT INVISIBLE)",
        (struct expected_sql_error){
            .code = mysql_error_must_have_visible_column,
            .sqlstate = "HY000",
            .message_part = "A table must have at least one visible column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE invalid_srid (id INT SRID 0)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_usage,
            .sqlstate = "HY000",
            .message_part = "Incorrect usage of SRID and non-geometry column",
        }
    );

    failures += execute_ok(database, "CREATE TABLE multi_t (id INT PRIMARY KEY, c INT)");
    failures += execute_ok(
        database,
        "ALTER TABLE multi_t ADD COLUMN hidden_added INT INVISIBLE, ADD COLUMN visible_added INT"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM multi_t LIKE 'hidden_added'",
            .values = hidden_added_rows,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "multi add invisible column metadata",
        }
    );
    failures += execute_ok(database, "ALTER TABLE multi_t MODIFY c BIGINT INVISIBLE");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM multi_t LIKE 'c'",
            .values = modified_column_rows,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "modify invisible column metadata",
        }
    );

    failures += execute_error(
        database,
        "ALTER TABLE multi_t ADD COLUMN transient_col INT, RENAME TO multi_t2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "multi-action ALTER TABLE does not support this action",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM multi_t LIKE 'transient_col'",
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = 0U,
            .context = "unsupported multi-action rolls back add column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'multi_t2'",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "unsupported multi-action rolls back rename",
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
