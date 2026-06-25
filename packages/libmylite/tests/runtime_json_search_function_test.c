#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
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
    mysql_error_incorrect_arguments = 1210,
    mysql_error_invalid_json_text = 3141,
    mysql_error_invalid_json_path = 3143,
    mysql_error_invalid_json_charset = 3144,
    mysql_error_invalid_json_data = 3146,
    mysql_error_invalid_json_one_or_all = 3154,
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

static int test_literal_json_search_values(void);
static int test_table_backed_json_search_values(void);
static int test_json_search_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_literal_json_search_values();
    failures += test_table_backed_json_search_values();
    failures += test_json_search_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_literal_json_search_values(void) {
    static const char *const columns[] = {
        "one_hit",
        "all_hits",
        "key_hit",
        "wildcard_hits",
        "escaped_default",
        "escaped_custom",
        "empty_escape",
        "path_scope",
        "case_sensitive",
        "null_doc",
        "null_mode",
        "null_search",
        "null_path",
    };
    static const char *const values[] = {
        "\"$[0]\"",
        "[\"$[0]\", \"$[2].x\"]",
        "\"$[1][0].k\"",
        "[\"$.a\", \"$.b\", \"$.c\"]",
        "\"$.a\"",
        "\"$.a\"",
        NULL,
        "\"$[2].x\"",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const escape_columns[] = {
        "percent_escaped_value",
        "underscore_escaped_value",
        "literal_percent",
        "literal_underscore",
        "trailing_percent_escape",
        "trailing_underscore_escape",
    };
    static const char *const escape_values[] = {
        "\"$[0]\"",
        "\"$[0]\"",
        "\"$[2]\"",
        "\"$[3]\"",
        "\"$[0]\"",
        "\"$[1]\"",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}, "
                   "{\"y\":\"bcd\"}]', 'one', 'abc') AS one_hit, "
                   "JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}, "
                   "{\"y\":\"bcd\"}]', 'all', 'abc') AS all_hits, "
                   "JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}, "
                   "{\"y\":\"bcd\"}]', 'all', '10') AS key_hit, "
                   "JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\",\"c\":\"axc\"}', 'all', 'a_c') "
                   "AS wildcard_hits, "
                   "JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\"}', 'all', 'a\\_c') "
                   "AS escaped_default, "
                   "JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\"}', 'all', 'a!_c', '!') "
                   "AS escaped_custom, "
                   "JSON_SEARCH('{\"a\":\"a_c\",\"b\":\"abc\"}', 'all', 'a\\_c', '') "
                   "AS empty_escape, "
                   "JSON_SEARCH('[\"abc\", [{\"k\":\"10\"}, \"def\"], {\"x\":\"abc\"}]', "
                   "'one', 'abc', NULL, '$[2]') AS path_scope, "
                   "JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'ABC') AS case_sensitive, "
                   "JSON_SEARCH(NULL, 'one', 'abc') AS null_doc, "
                   "JSON_SEARCH('{\"a\":\"abc\"}', NULL, 'abc') AS null_mode, "
                   "JSON_SEARCH('{\"a\":\"abc\"}', 'one', NULL) AS null_search, "
                   "JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc', NULL, NULL) AS null_path",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "literal JSON_SEARCH values",
        }
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '%a', '%') "
                   "AS percent_escaped_value, "
                   "JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '_a', '_') "
                   "AS underscore_escaped_value, "
                   "JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '%%', '%') "
                   "AS literal_percent, "
                   "JSON_SEARCH('[\"a\",\"x\",\"%\",\"_\",\"abc\"]', 'all', '__', '_') "
                   "AS literal_underscore, "
                   "JSON_SEARCH('[\"%\",\"_\",\"a\"]', 'all', '%', '%') "
                   "AS trailing_percent_escape, "
                   "JSON_SEARCH('[\"%\",\"_\",\"a\"]', 'all', '_', '_') "
                   "AS trailing_underscore_escape",
            .columns = escape_columns,
            .column_count = sizeof(escape_columns) / sizeof(escape_columns[0]),
            .values = escape_values,
            .row_count = 1U,
            .context = "JSON_SEARCH wildcard escape characters",
        }
    );

    failures +=
        execute_ok(database, "SELECT JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 1U, "json_search metadata");
        failures += expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_JSON,
            "json_search metadata type"
        );
        failures += expect_int(
            mylite_result_column_nullable(result, 0U),
            1,
            "json_search metadata nullable"
        );
        failures += expect_result_value(result, 0U, 0U, "\"$.a\"", "json_search metadata value");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DO JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "json_search do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "json_search do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "json_search do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "json_search do warnings");
    }
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    return failures;
}

static int test_table_backed_json_search_values(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_SEARCH(j, 'all', pat)",
        "JSON_SEARCH(s, 'one', 'blue', NULL, '$.tags')",
    };
    static const char *const values_table[] = {
        "1",
        "[\"$.a\", \"$.b[1]\"]",
        "\"$.tags[0]\"",
        "2",
        NULL,
        NULL,
    };
    static const char *const columns_updated[] = {"id", "s"};
    static const char *const values_updated[] = {"1", "\"$.a\"", "2", "{\"tags\":[\"green\"]}"};
    static const char *const columns_predicate[] = {"id"};
    static const char *const values_predicate[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table-backed", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT PRIMARY KEY, j JSON, s TEXT, pat VARCHAR(10))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":\"abc\",\"b\":[\"def\",\"abc\"]}', '{\"tags\":[\"blue\",\"red\"]}', "
        "'abc'), "
        "(2, '{\"a\":\"zzz\"}', '{\"tags\":[\"green\"]}', 'abc')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_SEARCH(j, 'all', pat), "
                   "JSON_SEARCH(s, 'one', 'blue', NULL, '$.tags') FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table JSON_SEARCH projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_SEARCH(j, 'one', pat) IS NOT NULL ORDER BY id",
            .columns = columns_predicate,
            .column_count = sizeof(columns_predicate) / sizeof(columns_predicate[0]),
            .values = values_predicate,
            .row_count = 1U,
            .context = "table JSON_SEARCH predicate",
        }
    );
    failures +=
        execute_ok(database, "UPDATE t SET s = JSON_SEARCH(j, 'one', pat) WHERE id = 1", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, s FROM t ORDER BY id",
            .columns = columns_updated,
            .column_count = sizeof(columns_updated) / sizeof(columns_updated[0]),
            .values = values_updated,
            .row_count = 2U,
            .context = "table JSON_SEARCH update assignment",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_search_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH('{\"a\":\"abc\"}', 'bad', 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_one_or_all,
            .sqlstate = "42000",
            .message_part = "oneOrAll argument to json_search",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH('{bad}', 'one', 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH('{bad}', NULL, 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH('{bad}', 'one', 'abc', NULL, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH(CAST('{\"a\":\"abc\"}' AS BINARY), 'one', 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH(1, 'one', 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc', 'xx')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_arguments,
            .sqlstate = "HY000",
            .message_part = "Incorrect arguments to ESCAPE",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SEARCH('{\"a\":\"abc\"}', 'one', 'abc', NULL, '$.')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );

    mylite_close(database);
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
        "/tmp/mylite-json-search-function-%s-%d.mylite",
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

    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s row %zu column %zu: expected %s, got %s\n",
                context,
                row,
                column,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s row %zu column %zu: expected %s, got %s\n",
            context,
            row,
            column,
            expected,
            actual
        );
        return 1;
    }
    return 0;
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
