#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    show_columns_column_count = 6,
    show_index_column_count = 15,
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

static int test_ddl_key_type_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_not_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_ddl_key_type_surfaces() == 0 ? 0 : 1;
}

static int test_ddl_key_type_surfaces(void) {
    static const char *const inline_key_column_rows[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
    };
    static const char *const inline_key_create_rows[] = {
        "inline_key",
        "CREATE TABLE `inline_key` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE inline_key (id INT KEY, v INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM inline_key LIKE 'id'",
            .values = inline_key_column_rows,
            .column_count = show_columns_column_count,
            .row_count = 1U,
            .context = "inline KEY column metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE inline_key",
            .values = inline_key_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "inline KEY show create",
        }
    );

    failures += execute_ok(database, "CREATE TABLE keyed (a INT, KEY k_a (a) KEY_BLOCK_SIZE=1024)");
    failures += execute_ok(database, "CREATE INDEX k_a2 ON keyed (a) KEY_BLOCK_SIZE 512");
    failures += execute_ok(database, "ALTER TABLE keyed ADD KEY k_a3 (a) KEY_BLOCK_SIZE=256");
    failures += expect_int(
        mylite_execute(
            database,
            "SHOW CREATE TABLE keyed",
            strlen("SHOW CREATE TABLE keyed"),
            &result
        ),
        MYLITE_OK,
        "show create keyed"
    );
    if (result != NULL && mylite_result_row_count(result) == 1U) {
        const char *create_sql = mylite_result_value_text(result, 0U, 1U);

        failures += expect_contains(create_sql, "KEY `k_a` (`a`)", "created key_block_size index");
        failures +=
            expect_contains(create_sql, "KEY `k_a2` (`a`)", "standalone key_block_size index");
        failures += expect_contains(create_sql, "KEY `k_a3` (`a`)", "alter key_block_size index");
        failures +=
            expect_not_contains(create_sql, "KEY_BLOCK_SIZE", "ignored index key_block_size");
    } else {
        failures += expect_size(
            result == NULL ? 0U : mylite_result_row_count(result),
            1U,
            "show create keyed row count"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "CREATE TABLE primary_prefix (name VARCHAR(100), PRIMARY KEY (name(10)))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "PRIMARY KEY prefix key parts are not yet supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'primary_prefix'",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "primary prefix create rollback",
        }
    );

    failures += execute_ok(database, "CREATE TABLE alter_prefix (name VARCHAR(100), code INT)");
    failures += execute_error(
        database,
        "ALTER TABLE alter_prefix ADD PRIMARY KEY (name(10), code)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "PRIMARY KEY prefix key parts are not yet supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM alter_prefix",
            .values = NULL,
            .column_count = show_index_column_count,
            .row_count = 0U,
            .context = "primary prefix alter no mutation",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE zint (a INT ZEROFILL)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLES LIKE 'zint'",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "zerofill create no mutation",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_prefix ADD COLUMN z INT ZEROFILL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_prefix LIKE 'z'",
            .values = NULL,
            .column_count = show_columns_column_count,
            .row_count = 0U,
            .context = "zerofill alter no mutation",
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
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t index = (row * query.column_count) + column;

            failures += expect_text(
                mylite_result_value_text(result, row, column),
                query.values == NULL ? NULL : query.values[index],
                query.context
            );
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

static int expect_not_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        return 0;
    }
    fprintf(stderr, "%s: expected [%s] not to contain [%s]\n", context, actual, needle);
    return 1;
}
