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
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_native_function_argument_count = 1582,
    mysql_error_invalid_json_text = 3141,
    mysql_error_invalid_json_path = 3143,
    mysql_error_invalid_json_charset = 3144,
    mysql_error_invalid_json_data = 3146,
    mysql_error_json_path_not_allowed = 3153,
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

static int test_literal_json_remove_values(void);
static int test_dual_do_and_status(void);
static int test_table_backed_json_remove_values(void);
static int test_json_remove_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_literal_json_remove_values();
    failures += test_dual_do_and_status();
    failures += test_table_backed_json_remove_values();
    failures += test_json_remove_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_literal_json_remove_values(void) {
    static const char *const columns_object[] = {
        "remove_member",
        "nested",
        "missing_member",
        "missing_parent",
        "duplicate_path",
        "two_paths",
    };
    static const char *const values_object[] = {
        "{\"b\": 2}",
        "{\"a\": {\"c\": 2}}",
        "{\"a\": 1}",
        "{\"a\": 1}",
        "{\"b\": 2}",
        "{\"b\": 2}",
    };
    static const char *const columns_array[] = {
        "array_first",
        "array_middle",
        "array_missing",
        "array_leading_zero",
        "left_to_right",
        "scalar_array_noop",
        "string_array_noop",
        "scalar_member_noop",
    };
    static const char *const values_array[] = {
        "[2, 3]",
        "[1, 3]",
        "[1, 2, 3]",
        "[1, 3]",
        "[2]",
        "1",
        "\"x\"",
        "1",
    };
    static const char *const columns_wrapped[] = {
        "root_object_wrapped",
        "nested_object_wrapped",
        "wrapped_missing",
    };
    static const char *const values_wrapped[] = {
        "{}",
        "{\"a\": {}}",
        "{\"a\": 1}",
    };
    static const char *const columns_nulls[] = {
        "null_document",
        "null_path",
        "null_path_then_invalid",
        "root_path_before_null",
    };
    static const char *const values_nulls[] = {NULL, NULL, NULL, NULL};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REMOVE('{\"a\":1,\"b\":2}', '$.a') AS remove_member, "
                   "JSON_REMOVE('{\"a\":{\"b\":1,\"c\":2}}', '$.a.b') AS nested, "
                   "JSON_REMOVE('{\"a\":1}', '$.b') AS missing_member, "
                   "JSON_REMOVE('{\"a\":1}', '$.b.c') AS missing_parent, "
                   "JSON_REMOVE('{\"a\":1,\"b\":2}', '$.a', '$.a') AS duplicate_path, "
                   "JSON_REMOVE('{\"a\":1,\"b\":2,\"c\":3}', '$.a', '$.c') AS two_paths",
            .columns = columns_object,
            .column_count = sizeof(columns_object) / sizeof(columns_object[0]),
            .values = values_object,
            .row_count = 1U,
            .context = "literal object json_remove values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REMOVE('[1,2,3]', '$[0]') AS array_first, "
                   "JSON_REMOVE('[1,2,3]', '$[1]') AS array_middle, "
                   "JSON_REMOVE('[1,2,3]', '$[99]') AS array_missing, "
                   "JSON_REMOVE('[1,2,3]', '$[01]') AS array_leading_zero, "
                   "JSON_REMOVE('[1,2,3]', '$[0]', '$[1]') AS left_to_right, "
                   "JSON_REMOVE('1', '$[0]') AS scalar_array_noop, "
                   "JSON_REMOVE('\"x\"', '$[0]') AS string_array_noop, "
                   "JSON_REMOVE('1', '$.a') AS scalar_member_noop",
            .columns = columns_array,
            .column_count = sizeof(columns_array) / sizeof(columns_array[0]),
            .values = values_array,
            .row_count = 1U,
            .context = "literal array json_remove values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REMOVE('{\"a\":1}', '$[0].a') AS root_object_wrapped, "
                   "JSON_REMOVE('{\"a\":{\"b\":1}}', '$.a[0].b') AS nested_object_wrapped, "
                   "JSON_REMOVE('{\"a\":1}', '$[1].a') AS wrapped_missing",
            .columns = columns_wrapped,
            .column_count = sizeof(columns_wrapped) / sizeof(columns_wrapped[0]),
            .values = values_wrapped,
            .row_count = 1U,
            .context = "literal wrapped array leg json_remove values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REMOVE(NULL, '$.a') AS null_document, "
                   "JSON_REMOVE('{\"a\":1}', NULL) AS null_path, "
                   "JSON_REMOVE('{}', NULL, 'a') AS null_path_then_invalid, "
                   "JSON_REMOVE('{}', '$', NULL) AS root_path_before_null",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 1U,
            .context = "json_remove null handling",
        }
    );

    failures += execute_ok(database, "SELECT JSON_REMOVE('{\"a\":1}', '$.a') AS removed", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 1U, "json_remove metadata");
        failures += mylite_test_expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_JSON,
            "json_remove metadata type"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_nullable(result, 0U),
            1,
            "json_remove nullable"
        );
        failures += expect_result_value(result, 0U, 0U, "{}", "json_remove metadata value");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_dual_do_and_status(void) {
    static const char *const columns_dual[] = {"removed", "JSON_REMOVE('{\"a\":1}','$.a')"};
    static const char *const values_dual[] = {"{}", "{}"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REMOVE('{\"a\":1}', '$.a') AS removed, "
                   "JSON_REMOVE('{\"a\":1}','$.a') FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "json_remove from dual",
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
            .context = "row count after json_remove select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_REMOVE('{\"a\":1}', '$.a'), JSON_REMOVE('{\"a\":1}', '$.b')",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "json_remove do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "json_remove do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "json_remove do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "json_remove do warnings"
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
            .context = "row count after json_remove do",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_json_remove_values(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_REMOVE(j, '$.a')",
        "JSON_REMOVE(j, '$.b[0]')",
        "JSON_REMOVE(doc_text, '$.c')",
        "JSON_REMOVE(j, NULL)",
        "JSON_REMOVE(j, '$', NULL)",
    };
    static const char *const values_table[] = {
        "1",
        "{\"b\": [2, 3], \"c\": \"x\"}",
        "{\"a\": 1, \"b\": [3], \"c\": \"x\"}",
        "{\"a\": 1, \"b\": [2, 3]}",
        NULL,
        NULL,
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "JSON_REMOVE(j, '$.a')"};
    static const char *const values_limited[] = {
        "1",
        "{\"b\": [2, 3], \"c\": \"x\"}",
    };
    static const char *const values_reopen[] = {
        "1",
        "{\"b\": [2, 3], \"c\": \"x\"}",
        "2",
        NULL,
    };
    static const char *const columns_nested[] = {
        "id",
        "b0",
        "unquoted_c",
        "removed_type",
    };
    static const char *const values_nested[] = {
        "1",
        "2",
        "x",
        NULL,
        "2",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_source[] = {"id", "j"};
    static const char *const values_source[] = {
        "1",
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\"}",
        "2",
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_open(path, &database);
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_dml_ok(
        database,
        "CREATE TABLE t(id INT, j JSON, doc_text LONGTEXT, b VARBINARY(10))",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', "
        "'{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', x'6162'), "
        "(2, NULL, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_REMOVE(j, '$.a'), JSON_REMOVE(j, '$.b[0]'), "
                   "JSON_REMOVE(doc_text, '$.c'), JSON_REMOVE(j, NULL), "
                   "JSON_REMOVE(j, '$', NULL) FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table json_remove projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_REMOVE(j, '$.a') "
                   "FROM t WHERE id >= 1 ORDER BY id LIMIT 1",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 1U,
            .context = "table json_remove where order limit projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_EXTRACT(JSON_REMOVE(j, '$.a'), '$.b[0]') AS b0, "
                   "JSON_UNQUOTE(JSON_EXTRACT(JSON_REMOVE(j, '$.a'), '$.c')) AS unquoted_c, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_REMOVE(j, '$.a'), '$.a')) AS removed_type "
                   "FROM t ORDER BY id",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 2U,
            .context = "row-scalar json_extract consumes json_remove result",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, j FROM t ORDER BY id",
            .columns = columns_source,
            .column_count = sizeof(columns_source) / sizeof(columns_source[0]),
            .values = values_source,
            .row_count = 2U,
            .context = "json_remove does not mutate source rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json_remove");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_REMOVE(j, '$.a') FROM t ORDER BY id",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen table json_remove values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_remove_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_open(path, &database);
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t(j JSON, n INT, b VARBINARY(10), doc_text LONGTEXT)",
        NULL
    );
    failures +=
        execute_ok(database, "INSERT INTO t VALUES ('{\"a\":1}', 1, x'6162', '{bad}')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE('{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE('{bad}', '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE('{bad}', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE(doc_text, NULL) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE('{}', 'a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE('{}', 'a', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE(j, 'a', NULL) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE('{}', '$')",
        (struct expected_sql_error){
            .code = mysql_error_json_path_not_allowed,
            .sqlstate = "42000",
            .message_part = "path expression '$' is not allowed",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE(j, '$') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_json_path_not_allowed,
            .sqlstate = "42000",
            .message_part = "path expression '$' is not allowed",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE(1, '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE('{}', '$.*')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_REMOVE() path or document shape is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE(j, n) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_REMOVE() supports only string and NULL path literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE(missing, '$.a') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REMOVE(b, '$.a') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
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
        fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
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

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_dml_ok(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (failures == 0) {
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            sql
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            sql
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures == 0) {
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
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; failures == 0 && row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_value(result, row, column, expected.values[index], expected.context);
        }
    }
    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
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
