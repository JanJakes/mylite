#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_collation_binary_id = 63,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_approximate_decimals = 31,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

struct expected_column_metadata {
    enum mylite_result_column_type type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    int nullable;
    const char *context;
};

static int test_values_outside_odku_select(void);
static int test_values_outside_odku_metadata(void);
static int test_values_outside_odku_diagnostics(void);
static int setup_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_values_outside_odku_select();
    failures += test_values_outside_odku_metadata();
    failures += test_values_outside_odku_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_values_outside_odku_select(void) {
    static const char *const inline_warning_count_columns[] = {"vv", "wc"};
    static const char *const inline_warning_count_rows[] = {NULL, "1", NULL, "1"};
    static const char *const values_columns[] = {"vv", "ss", "qv", "fqv"};
    static const char *const values_rows[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    static const char *const warning_count_columns[] = {"@@warning_count", "@@error_count"};
    static const char *const single_warning_count_values[] = {"1", "0"};
    static const char *const warning_count_values[] = {"4", "0"};
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VALUES(v) AS vv, @@warning_count AS wc FROM t ORDER BY id",
            .columns = inline_warning_count_columns,
            .column_count =
                sizeof(inline_warning_count_columns) / sizeof(inline_warning_count_columns[0]),
            .values = inline_warning_count_rows,
            .row_count = 2U,
            .warning_count = 1U,
            .context = "values outside odku inline warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count",
            .columns = warning_count_columns,
            .column_count = sizeof(warning_count_columns) / sizeof(warning_count_columns[0]),
            .values = single_warning_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "values outside odku single warning count variable",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VALUES(v) AS vv, VALUES(s) AS ss, VALUES(t.v) AS qv, "
                   "VALUES(app.t.v) AS fqv FROM t ORDER BY id",
            .columns = values_columns,
            .column_count = sizeof(values_columns) / sizeof(values_columns[0]),
            .values = values_rows,
            .row_count = 2U,
            .warning_count = 4U,
            .context = "values outside odku null rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COUNT(*) WARNINGS",
            .columns = (const char *const[]){"@@session.warning_count"},
            .column_count = 1U,
            .values = (const char *const[]){"0"},
            .row_count = 1U,
            .warning_count = 0U,
            .context = "values outside odku hidden show count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = (const char *const[]){"Level", "Code", "Message"},
            .column_count = 3U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "values outside odku hidden show warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, @@error_count",
            .columns = warning_count_columns,
            .column_count = sizeof(warning_count_columns) / sizeof(warning_count_columns[0]),
            .values = warning_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "values outside odku warning count variable",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_values_outside_odku_metadata(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = setup_database(&database);

    failures += execute_ok(
        database,
        "SELECT VALUES(v) AS vv, VALUES(s) AS ss, VALUES(b) AS bb FROM t LIMIT 1",
        &result
    );
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 3U, "values metadata cols");
        failures += expect_size(mylite_result_row_count(result), 1U, "values metadata rows");
        failures +=
            expect_size(mylite_result_warning_count(result), 3U, "values metadata warnings");
        for (size_t column = 0U; column < 3U; ++column) {
            failures += expect_column_metadata(
                result,
                column,
                (struct expected_column_metadata){
                    .type = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
                    .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                    .charset_id = mysql_collation_binary_id,
                    .collation_id = mysql_collation_binary_id,
                    .display_length = 0U,
                    .decimals = mysql_approximate_decimals,
                    .nullable = 1,
                    .context = "values metadata",
                }
            );
            failures += expect_result_value(result, 0U, column, NULL, "values metadata value");
        }
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_values_outside_odku_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = setup_database(&database);

    failures += execute_error(
        database,
        "SELECT VALUES(v)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'v' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VALUES(nope) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VALUES() FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT VALUES(v, id) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT VALUES(1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );

    mylite_close(database);
    return failures;
}

static int setup_database(mylite_db **out_database) {
    mylite_db *database = NULL;
    int rc = mylite_test_open_temporary(&database);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "open temporary database failed: %d\n", rc);
        return 1;
    }

    if (execute_ok(database, "CREATE DATABASE app", NULL) != 0 ||
        execute_ok(database, "USE app", NULL) != 0 ||
        execute_ok(
            database,
            "CREATE TABLE t(id INT PRIMARY KEY, v INT, s VARCHAR(10), b VARBINARY(10))",
            NULL
        ) != 0 ||
        execute_ok(
            database,
            "INSERT INTO t VALUES(1, 11, 'aa', X'61'), (2, 22, 'bb', X'62')",
            NULL
        ) != 0) {
        mylite_close(database);
        return 1;
    }

    *out_database = database;
    return 0;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
                expected.context
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
            fprintf(stderr, "%s: expected NULL at %zu,%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int expect_column_metadata(
    const mylite_result *result,
    size_t column,
    struct expected_column_metadata expected
) {
    int failures = 0;

    failures +=
        expect_int(mylite_result_column_type(result, column), expected.type, expected.context);
    failures +=
        expect_int64(mylite_result_column_flags(result, column), expected.flags, expected.context);
    failures += expect_int64(
        mylite_result_column_charset_id(result, column),
        expected.charset_id,
        expected.context
    );
    failures += expect_int64(
        mylite_result_column_collation_id(result, column),
        expected.collation_id,
        expected.context
    );
    failures += expect_int64(
        (int64_t)mylite_result_column_display_length(result, column),
        (int64_t)expected.display_length,
        expected.context
    );
    failures += expect_int(
        mylite_result_column_decimals(result, column),
        expected.decimals,
        expected.context
    );
    failures += expect_int(
        mylite_result_column_nullable(result, column),
        expected.nullable,
        expected.context
    );
    return failures;
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
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
