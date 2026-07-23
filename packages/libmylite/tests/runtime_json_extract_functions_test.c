#include "mylite_test_support.h"

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
    mysql_error_invalid_json_text = 3141,
    mysql_error_invalid_json_path = 3143,
    mysql_error_invalid_json_data = 3146,
    mysql_error_json_unquote_incorrect_type = 3064,
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

static int test_no_source_dual_and_do_json_extract(void);
static int test_table_backed_json_extract_and_reopen(void);
static int test_json_extract_diagnostics(void);
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
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_json_extract();
    failures += test_table_backed_json_extract_and_reopen();
    failures += test_json_extract_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_json_extract(void) {
    static const char *const columns_extract[] = {
        "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.a')",
        "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.b')",
        "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.c')",
        "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.d')",
        "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.e[1]')",
        "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.o')",
        "JSON_EXTRACT('{\"a\":1}', '$.missing')",
        "JSON_EXTRACT(NULL, '$.a')",
        "JSON_EXTRACT('{\"a\":1}', NULL)",
    };
    static const char *const values_extract[] = {
        "1",
        "\"x\"",
        "null",
        "true",
        "20",
        "{\"k\": \"v\"}",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_path[] = {
        "JSON_EXTRACT('{\"a\":1}', '$')",
        "JSON_EXTRACT('{\"a-b\":2}', '$.\"a-b\"')",
        "JSON_EXTRACT('[10,20,[30,40]]', '$[2][1]')",
        "JSON_EXTRACT('{\"a\":{\"b\":[1,2]}}', '$.a.b[1]')",
    };
    static const char *const values_path[] = {"{\"a\": 1}", "2", "40", "2"};
    static const char *const columns_unquote[] = {
        "JSON_UNQUOTE('\"abc\"')",
        "JSON_UNQUOTE('abc')",
        "JSON_UNQUOTE('123')",
        "JSON_UNQUOTE('null')",
        "JSON_UNQUOTE('true')",
        "JSON_UNQUOTE('[1, 2]')",
        "JSON_UNQUOTE('{\"a\": 1}')",
        "JSON_UNQUOTE(NULL)",
    };
    static const char *const values_unquote[] = {
        "abc",
        "abc",
        "123",
        "null",
        "true",
        "[1, 2]",
        "{\"a\": 1}",
        NULL,
    };
    static const char *const columns_labels[] = {
        "JSON_EXTRACT('{\"a\":1}', '$.a')",
        "value",
    };
    static const char *const values_labels[] = {"1", "x"};
    static const char *const columns_escape[] = {"HEX(JSON_UNQUOTE('\"\\\\t\\\\u0032\"'))"};
    static const char *const values_escape_default[] = {"0932"};
    static const char *const values_escape_no_backslash[] = {"5C745C7530303332"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
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
            .sql = "SELECT "
                   "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.a'), "
                   "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.b'), "
                   "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.c'), "
                   "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.d'), "
                   "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.e[1]'), "
                   "JSON_EXTRACT('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.o'), "
                   "JSON_EXTRACT('{\"a\":1}', '$.missing'), JSON_EXTRACT(NULL, '$.a'), "
                   "JSON_EXTRACT('{\"a\":1}', NULL)",
            .columns = columns_extract,
            .column_count = sizeof(columns_extract) / sizeof(columns_extract[0]),
            .values = values_extract,
            .row_count = 1U,
            .context = "literal json_extract values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_EXTRACT('{\"a\":1}', '$'), "
                   "JSON_EXTRACT('{\"a-b\":2}', '$.\"a-b\"'), "
                   "JSON_EXTRACT('[10,20,[30,40]]', '$[2][1]'), "
                   "JSON_EXTRACT('{\"a\":{\"b\":[1,2]}}', '$.a.b[1]')",
            .columns = columns_path,
            .column_count = sizeof(columns_path) / sizeof(columns_path[0]),
            .values = values_path,
            .row_count = 1U,
            .context = "simple json path forms",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_UNQUOTE('\"abc\"'), JSON_UNQUOTE('abc'), "
                   "JSON_UNQUOTE('123'), JSON_UNQUOTE('null'), JSON_UNQUOTE('true'), "
                   "JSON_UNQUOTE('[1, 2]'), JSON_UNQUOTE('{\"a\": 1}'), JSON_UNQUOTE(NULL)",
            .columns = columns_unquote,
            .column_count = sizeof(columns_unquote) / sizeof(columns_unquote[0]),
            .values = values_unquote,
            .row_count = 1U,
            .context = "literal json_unquote values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_EXTRACT('{\"a\":1}', '$.a'), "
                   "JSON_UNQUOTE('\"x\"') AS value FROM DUAL",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .context = "json extraction labels",
        }
    );
    failures += execute_ok(database, "SET @@sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(JSON_UNQUOTE('\"\\\\t\\\\u0032\"'))",
            .columns = columns_escape,
            .column_count = sizeof(columns_escape) / sizeof(columns_escape[0]),
            .values = values_escape_default,
            .row_count = 1U,
            .context = "json_unquote escape decoding",
        }
    );
    failures += execute_ok(database, "SET @@sql_mode = 'NO_BACKSLASH_ESCAPES'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(JSON_UNQUOTE('\"\\\\t\\\\u0032\"'))",
            .columns = columns_escape,
            .column_count = sizeof(columns_escape) / sizeof(columns_escape[0]),
            .values = values_escape_no_backslash,
            .row_count = 1U,
            .context = "json_unquote no backslash escapes",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after json_extract select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_EXTRACT('{\"a\":1}', '$.a'), JSON_UNQUOTE('\"x\"'), "
        "JSON_EXTRACT('{\"a\":1}', '$.missing')",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "json_extract do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "json_extract do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "json_extract do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "json_extract do warnings"
        );
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after json_extract do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_json_extract_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_EXTRACT(j, '$.a')",
        "JSON_UNQUOTE(JSON_EXTRACT(j, '$.b'))",
        "j->>'$.b'",
        "JSON_EXTRACT(s, '$.a')",
        "JSON_UNQUOTE(JSON_EXTRACT(s, '$.b'))",
        "t.s->'$.a'",
    };
    static const char *const values_table[] = {
        "1", "1",    "x", "x", "1",  "x",  "1",  "2",  "null", "null", "null",
        "2", "null", "2", "3", NULL, NULL, NULL, NULL, NULL,   NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(128))", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"b\":\"x\"}', '{\"a\":1,\"b\":\"x\"}'), "
        "(2, '{\"a\":null,\"b\":\"null\"}', '{\"a\":2,\"b\":null}'), "
        "(3, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_EXTRACT(j, '$.a'), "
                   "JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')), j->>'$.b', "
                   "JSON_EXTRACT(s, '$.a'), JSON_UNQUOTE(JSON_EXTRACT(s, '$.b')), "
                   "t.s->'$.a' FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table json extraction values",
        }
    );

    mylite_close(database);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json extraction");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_EXTRACT(j, '$.a'), "
                   "JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')), j->>'$.b', "
                   "JSON_EXTRACT(s, '$.a'), JSON_UNQUOTE(JSON_EXTRACT(s, '$.b')), "
                   "t.s->'$.a' FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "reopen table json extraction values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_extract_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(20))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '{\"a\":1}', '{\"a\":1}')", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (2, '{\"a\":1}', 'bad')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_EXTRACT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT('{\"a\":1}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_EXTRACT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT('{}', '$', '$.b')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_EXTRACT() multiple path arguments are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT('bad', '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT('{\"a\":1}', 'bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT(1, '$')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_UNQUOTE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_UNQUOTE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_UNQUOTE('\"a\"', '\"b\"')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_UNQUOTE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_UNQUOTE(1)",
        (struct expected_sql_error){
            .code = mysql_error_json_unquote_incorrect_type,
            .sqlstate = "HY000",
            .message_part = "Incorrect type for argument",
        }
    );
    failures += execute_ok(database, "SET @@sql_mode = 'NO_BACKSLASH_ESCAPES'", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (3, '{\"a\":1}', '\"\\u00ZZ\"')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_UNQUOTE('\"\\u00ZZ\"')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT(s, '$.a') FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_UNQUOTE(s) FROM t WHERE id = 3",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_EXTRACT(missing, '$.a') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
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

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
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

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
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

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return mylite_test_expect_text(actual, expected, context);
}
