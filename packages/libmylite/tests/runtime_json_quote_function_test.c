#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_invalid_json_charset = 3144,
    mysql_error_json_quote_incorrect_type = 3064,
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
    const char *context;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_no_source_dual_and_do_json_quote(void);
static int test_table_backed_json_quote_and_reopen(void);
static int test_json_quote_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
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

    failures += test_no_source_dual_and_do_json_quote();
    failures += test_table_backed_json_quote_and_reopen();
    failures += test_json_quote_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_json_quote(void) {
    static const char *const columns_literal[] = {
        "q_null_text",
        "q_quoted_text",
        "q_array_text",
        "q_null",
        "q_newline",
        "q_backslash_n",
        "q_quote",
    };
    static const char *const values_literal[] = {
        "\"null\"",
        "\"\\\"null\\\"\"",
        "\"[1, 2, 3]\"",
        NULL,
        "\"a\\nb\"",
        "\"a\\\\nb\"",
        "\"q\\\"b\"",
    };
    static const char *const columns_dual[] = {"q", "n"};
    static const char *const values_dual[] = {"\"abc\"", NULL};
    static const char *const columns_no_backslash[] = {"q1", "q2"};
    static const char *const values_no_backslash[] = {"\"a\\\\nb\"", "\"a\\\\\\\\nb\""};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_QUOTE('null') AS q_null_text, "
                   "JSON_QUOTE('\"null\"') AS q_quoted_text, "
                   "JSON_QUOTE('[1, 2, 3]') AS q_array_text, "
                   "JSON_QUOTE(NULL) AS q_null, JSON_QUOTE('a\\nb') AS q_newline, "
                   "JSON_QUOTE('a\\\\nb') AS q_backslash_n, JSON_QUOTE('q\"b') AS q_quote",
            .columns = columns_literal,
            .column_count = sizeof(columns_literal) / sizeof(columns_literal[0]),
            .values = values_literal,
            .row_count = 1U,
            .context = "literal json_quote values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_QUOTE('abc') AS q, JSON_QUOTE(NULL) AS n FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual json_quote values",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_QUOTE('a\\nb') AS q1, JSON_QUOTE('a\\\\nb') AS q2",
            .columns = columns_no_backslash,
            .column_count = sizeof(columns_no_backslash) / sizeof(columns_no_backslash[0]),
            .values = values_no_backslash,
            .row_count = 1U,
            .context = "json_quote no backslash escapes",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after json_quote select",
        }
    );

    failures += execute_ok(database, "DO JSON_QUOTE('abc'), JSON_QUOTE(NULL)", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "json_quote do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "json_quote do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "json_quote do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "json_quote do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after json_quote do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_json_quote_and_reopen(void) {
    static const char *const columns_table[] = {"id", "qs", "qt"};
    static const char *const values_table[] = {
        "1",
        "\"abc\"",
        "\"a\\nb\"",
        "2",
        NULL,
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, s VARCHAR(20), tx TEXT, j JSON, b VARBINARY(5), n INT)",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES (1, 'abc', 'a\\nb', '{\"a\":1}', X'616263', 42), "
        "(2, NULL, NULL, NULL, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_QUOTE(s) AS qs, JSON_QUOTE(tx) AS qt FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table json_quote values",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json_quote");
    if (failures == 0) {
        failures += execute_ok(database, "USE app", NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_QUOTE(s) AS qs, JSON_QUOTE(tx) AS qt FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "reopen table json_quote values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_quote_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, s VARCHAR(20), j JSON, b VARBINARY(5), n INT)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO t VALUES (1, 'abc', '{\"a\":1}', X'616263', 42)", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_QUOTE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE('a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_QUOTE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(123)",
        (struct expected_sql_error){
            .code = mysql_error_json_quote_incorrect_type,
            .sqlstate = "HY000",
            .message_part = "Incorrect type for argument 1 in function json_quote.",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(TRUE)",
        (struct expected_sql_error){
            .code = mysql_error_json_quote_incorrect_type,
            .sqlstate = "HY000",
            .message_part = "Incorrect type for argument 1 in function json_quote.",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(CAST('abc' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(j) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_json_quote_incorrect_type,
            .sqlstate = "HY000",
            .message_part = "Incorrect type for argument 1 in function json_quote.",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(n) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_json_quote_incorrect_type,
            .sqlstate = "HY000",
            .message_part = "Incorrect type for argument 1 in function json_quote.",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(b) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_QUOTE(CONCAT('a','b'))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "JSON_QUOTE() supports only string, NULL, and nonbinary string descriptor column "
                "arguments",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
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
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

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

static int expect_dml_ok(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-json-quote-function-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
