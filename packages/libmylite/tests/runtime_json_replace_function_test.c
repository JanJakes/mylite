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

static int test_literal_json_replace_values(void);
static int test_dual_do_and_status(void);
static int test_table_backed_json_replace_values(void);
static int test_json_replace_diagnostics(void);
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

    failures += test_literal_json_replace_values();
    failures += test_dual_do_and_status();
    failures += test_table_backed_json_replace_values();
    failures += test_json_replace_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_literal_json_replace_values(void) {
    static const char *const columns_object[] = {
        "existing_member",
        "missing_member",
        "nested",
        "missing_parent",
        "duplicate_path",
        "root",
    };
    static const char *const values_object[] = {
        "{\"a\": 10, \"b\": [2, 3]}",
        "{\"a\": 1}",
        "{\"a\": {\"b\": 2}}",
        "{\"a\": 1}",
        "{\"a\": 3}",
        "2",
    };
    static const char *const columns_array[] = {
        "array_replace",
        "array_append_noop",
        "array_far_noop",
        "array_leading_zero",
        "scalar_replace",
        "scalar_noop",
    };
    static const char *const values_array[] = {
        "[9, 2]",
        "[1, 2]",
        "[1, 2]",
        "[1, 9]",
        "2",
        "1",
    };
    static const char *const columns_values[] = {
        "json_array_value",
        "string_value",
        "string_type",
        "json_type",
    };
    static const char *const values_values[] = {
        "{\"a\": [1, 2]}",
        "{\"a\": \"[1,2]\"}",
        "STRING",
        "ARRAY",
    };
    static const char *const columns_nulls[] = {
        "json_null_value",
        "null_document",
        "null_path",
    };
    static const char *const values_nulls[] = {
        "{\"a\": null}",
        NULL,
        NULL,
    };
    static const char *const values_metadata[] = {"{\"a\": 1}"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REPLACE('{\"a\":1,\"b\":[2,3]}', '$.a', 10) "
                   "AS existing_member, "
                   "JSON_REPLACE('{\"a\":1}', '$.b', 2) AS missing_member, "
                   "JSON_REPLACE('{\"a\":{\"b\":1}}', '$.a.b', 2) AS nested, "
                   "JSON_REPLACE('{\"a\":1}', '$.b.c', 2) AS missing_parent, "
                   "JSON_REPLACE('{\"a\":1}', '$.a', 2, '$.a', 3) AS duplicate_path, "
                   "JSON_REPLACE('{\"a\":1}', '$', 2) AS root",
            .columns = columns_object,
            .column_count = sizeof(columns_object) / sizeof(columns_object[0]),
            .values = values_object,
            .row_count = 1U,
            .context = "literal object json_replace values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REPLACE('[1,2]', '$[0]', 9) AS array_replace, "
                   "JSON_REPLACE('[1,2]', '$[2]', 9) AS array_append_noop, "
                   "JSON_REPLACE('[1,2]', '$[99]', 9) AS array_far_noop, "
                   "JSON_REPLACE('[1,2]', '$[01]', 9) AS array_leading_zero, "
                   "JSON_REPLACE('1', '$[0]', 2) AS scalar_replace, "
                   "JSON_REPLACE('1', '$[1]', 2) AS scalar_noop",
            .columns = columns_array,
            .column_count = sizeof(columns_array) / sizeof(columns_array[0]),
            .values = values_array,
            .row_count = 1U,
            .context = "literal array json_replace values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REPLACE('{\"a\":1}', '$.a', JSON_ARRAY(1,2)) "
                   "AS json_array_value, "
                   "JSON_REPLACE('{\"a\":1}', '$.a', '[1,2]') AS string_value, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_REPLACE('{\"a\":1}', '$.a', '[1,2]'), "
                   "'$.a')) AS string_type, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_REPLACE('{\"a\":1}', '$.a', "
                   "JSON_ARRAY(1,2)), '$.a')) AS json_type",
            .columns = columns_values,
            .column_count = sizeof(columns_values) / sizeof(columns_values[0]),
            .values = values_values,
            .row_count = 1U,
            .context = "json_replace JSON and string value arguments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_REPLACE('{\"a\":1}', '$.a', NULL) AS json_null_value, "
                   "JSON_REPLACE(NULL, '$.a', 1) AS null_document, "
                   "JSON_REPLACE('{\"a\":1}', NULL, 2) AS null_path",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 1U,
            .context = "json_replace null handling",
        }
    );

    failures +=
        execute_ok(database, "SELECT JSON_REPLACE('{\"a\":1}', '$.b', 2) AS changed", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            1U,
            "json_replace metadata columns"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_JSON,
            "json_replace metadata type"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_nullable(result, 0U),
            1,
            "json_replace metadata nullable"
        );
        failures +=
            expect_result_value(result, 0U, 0U, values_metadata[0], "json_replace metadata value");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_dual_do_and_status(void) {
    static const char *const columns_dual[] = {"changed", "JSON_REPLACE('{\"a\":0}','$.a',1)"};
    static const char *const values_dual[] = {"{\"a\": 1}", "{\"a\": 1}"};
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
            .sql = "SELECT JSON_REPLACE('{\"a\":0}', '$.a', 1) AS changed, "
                   "JSON_REPLACE('{\"a\":0}','$.a',1) FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "json_replace from dual",
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
            .context = "row count after json_replace select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_REPLACE('{\"a\":1}', '$.a', 2), "
        "JSON_REPLACE('{\"a\":1}', '$.a', NULL)",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "json_replace do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "json_replace do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "json_replace do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "json_replace do warnings"
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
            .context = "row count after json_replace do",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_json_replace_values(void) {
    static const char nested_json_value[] =
        "{\"b\": 0, \"i\": 0, \"j\": {\"b\": 0, \"i\": 0, \"j\": null, \"s\": 0, "
        "\"x\": \"\"}, \"s\": 0, \"x\": \"\"}";
    static const char *const columns_table[] = {
        "id",
        "JSON_REPLACE(j, '$.s', s)",
        "JSON_REPLACE(j, '$.i', i)",
        "JSON_REPLACE(j, '$.b', b)",
        "JSON_REPLACE(j, '$.j', j)",
        "JSON_REPLACE(j, '$.x', label)",
    };
    static const char *const values_table[] = {
        "1",
        "{\"b\": 0, \"i\": 0, \"j\": null, \"s\": \"[1,2]\", \"x\": \"\"}",
        "{\"b\": 0, \"i\": 7, \"j\": null, \"s\": 0, \"x\": \"\"}",
        "{\"b\": 1, \"i\": 0, \"j\": null, \"s\": 0, \"x\": \"\"}",
        nested_json_value,
        "{\"b\": 0, \"i\": 0, \"j\": null, \"s\": 0, \"x\": \"row\"}",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "JSON_REPLACE(j, '$.i', i)"};
    static const char *const values_limited[] = {
        "1",
        "{\"b\": 0, \"i\": 7, \"j\": null, \"s\": 0, \"x\": \"\"}",
    };
    static const char *const columns_null_path[] = {
        "id",
        "JSON_REPLACE(j, NULL, i + 1)",
        "JSON_REPLACE(j, '$.i', i, NULL, i + 1)",
    };
    static const char *const values_null_path[] = {
        "1",
        NULL,
        NULL,
        "2",
        NULL,
        NULL,
    };
    static const char *const columns_nested[] = {
        "id",
        "extracted_i",
        "unquoted_label",
        "json_value_type",
    };
    static const char *const values_nested[] = {
        "1",
        "7",
        "row",
        "ARRAY",
        "2",
        NULL,
        NULL,
        NULL,
    };
    static const char *const values_reopen[] = {
        "1",
        "{\"b\": 0, \"i\": 7, \"j\": null, \"s\": 0, \"x\": \"\"}",
        "2",
        NULL,
    };
    static const char *const columns_source[] = {"id", "j"};
    static const char *const values_source[] = {
        "1",
        "{\"b\": 0, \"i\": 0, \"j\": null, \"s\": 0, \"x\": \"\"}",
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
        "CREATE TABLE t(id INT, j JSON, s VARCHAR(100), i INT, b BOOLEAN, label VARCHAR(20))",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"s\":0,\"i\":0,\"b\":0,\"j\":null,\"x\":\"\"}', '[1,2]', 7, TRUE, 'row'), "
        "(2, NULL, NULL, NULL, FALSE, NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_REPLACE(j, '$.s', s), JSON_REPLACE(j, '$.i', i), "
                   "JSON_REPLACE(j, '$.b', b), JSON_REPLACE(j, '$.j', j), "
                   "JSON_REPLACE(j, '$.x', label) FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table json_replace projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_REPLACE(j, '$.i', i) "
                   "FROM t WHERE id >= 1 ORDER BY id LIMIT 1",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 1U,
            .context = "table json_replace where order limit projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_REPLACE(j, NULL, i + 1), "
                   "JSON_REPLACE(j, '$.i', i, NULL, i + 1) FROM t ORDER BY id",
            .columns = columns_null_path,
            .column_count = sizeof(columns_null_path) / sizeof(columns_null_path[0]),
            .values = values_null_path,
            .row_count = 2U,
            .context = "table json_replace null path skips value planning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "JSON_EXTRACT(JSON_REPLACE(j, '$.i', i), '$.i') AS extracted_i, "
                   "JSON_UNQUOTE(JSON_EXTRACT(JSON_REPLACE(j, '$.x', label), '$.x')) "
                   "AS unquoted_label, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_REPLACE(j, '$.j', JSON_ARRAY(i)), '$.j')) "
                   "AS json_value_type "
                   "FROM t ORDER BY id",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 2U,
            .context = "row-scalar json_extract consumes json_replace result",
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
            .context = "json_replace does not mutate source rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json_replace");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_REPLACE(j, '$.i', i) FROM t ORDER BY id",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen table json_replace values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_replace_diagnostics(void) {
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
    failures += execute_ok(database, "CREATE TABLE t(j JSON, n INT, b VARBINARY(10))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES ('{\"a\":1}', 1, x'6162')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE('{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE('{}', '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE('{bad}', '$.a', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE('{}', 'a', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE(j, 'a', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE(1, '$.a', 2)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE('{}', '$.*', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_REPLACE() path or document shape is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE(j, '$.a', n + 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "JSON constructors support only string, integer, boolean, NULL, and descriptor "
                "column arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE(missing, '$.a', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_REPLACE(b, '$.a', 1) FROM t",
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
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
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
    int failures = 0;

    failures += execute_ok(database, expected.sql, &result);
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
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                expected.columns[column],
                expected.context
            );
        }
        for (size_t row = 0U; row < expected.row_count; ++row) {
            for (size_t column = 0U; column < expected.column_count; ++column) {
                size_t index = (row * expected.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    expected.values[index],
                    expected.context
                );
            }
        }
    }
    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
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
