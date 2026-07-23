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
    mysql_error_parse = 1064,
    mysql_error_native_function_argument_count = 1582,
    mysql_error_invalid_json_text = 3141,
    mysql_error_invalid_json_path = 3143,
    mysql_error_invalid_json_charset = 3144,
    mysql_error_invalid_json_data = 3146,
    mysql_error_json_path_not_array_cell = 3165,
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

static int test_literal_json_array_mutation_values(void);
static int test_dual_do_and_status(void);
static int test_table_backed_json_array_mutation_values(void);
static int test_json_array_mutation_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_literal_json_array_mutation_values();
    failures += test_dual_do_and_status();
    failures += test_table_backed_json_array_mutation_values();
    failures += test_json_array_mutation_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_literal_json_array_mutation_values(void) {
    static const char *const columns_append[] = {
        "nested_array_append",
        "scalar_autowrap",
        "nested_scalar_autowrap",
        "member_array_append",
        "member_scalar_autowrap",
        "root_autowrap",
        "array_missing_index",
        "missing_member",
        "object_missing_member",
        "member_index_autowrap",
        "object_array_leg_autowrap",
        "nested_scalar_array_leg_autowrap",
    };
    static const char *const values_append[] = {
        "[\"a\", [\"b\", \"c\", 1], \"d\"]",
        "[[\"a\", 2], [\"b\", \"c\"], \"d\"]",
        "[\"a\", [[\"b\", 3], \"c\"], \"d\"]",
        "{\"a\": 1, \"b\": [2, 3, \"x\"], \"c\": 4}",
        "{\"a\": 1, \"b\": [2, 3], \"c\": [4, \"y\"]}",
        "[{\"a\": 1}, \"z\"]",
        "[1, 2]",
        "[1, 2]",
        "{\"a\": 1}",
        "{\"a\": [1, 9]}",
        "[{\"a\": 1}, 9]",
        "[1, 9]",
    };
    static const char *const columns_insert[] = {
        "array_middle",
        "array_append",
        "nested_array_start",
        "nested_array_middle",
        "left_to_right",
        "scalar_parent_noop",
        "missing_parent_noop",
        "root_object_array_cell_parent_noop",
        "nested_scalar_parent_noop",
    };
    static const char *const values_insert[] = {
        "[\"a\", \"x\", {\"b\": [1, 2]}, [3, 4]]",
        "[\"a\", {\"b\": [1, 2]}, [3, 4], \"x\"]",
        "[\"a\", {\"b\": [\"x\", 1, 2]}, [3, 4]]",
        "[\"a\", {\"b\": [1, 2]}, [3, \"y\", 4]]",
        "[\"x\", \"a\", {\"b\": [1, 2]}, [3, 4]]",
        "{\"a\": 1}",
        "{\"a\": 1}",
        "{\"a\": 1}",
        "[1]",
    };
    static const char *const columns_nulls[] = {
        "append_null_document",
        "append_null_path",
        "append_null_value",
        "append_later_null_path",
        "insert_null_document",
        "insert_null_path",
        "insert_null_value",
        "insert_later_null_path",
    };
    static const char *const values_nulls[] = {
        NULL,
        NULL,
        "{\"a\": [1, null]}",
        NULL,
        NULL,
        NULL,
        "[null, 1]",
        NULL,
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1]', 1) "
                   "AS nested_array_append, "
                   "JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '$[0]', 2) "
                   "AS scalar_autowrap, "
                   "JSON_ARRAY_APPEND('[\"a\", [\"b\", \"c\"], \"d\"]', '$[1][0]', 3) "
                   "AS nested_scalar_autowrap, "
                   "JSON_ARRAY_APPEND('{\"a\":1,\"b\":[2,3],\"c\":4}', '$.b', 'x') "
                   "AS member_array_append, "
                   "JSON_ARRAY_APPEND('{\"a\":1,\"b\":[2,3],\"c\":4}', '$.c', 'y') "
                   "AS member_scalar_autowrap, "
                   "JSON_ARRAY_APPEND('{\"a\":1}', '$', 'z') AS root_autowrap, "
                   "JSON_ARRAY_APPEND('[1,2]', '$[5]', 9) AS array_missing_index, "
                   "JSON_ARRAY_APPEND('[1,2]', '$.missing', 9) AS missing_member, "
                   "JSON_ARRAY_APPEND('{\"a\":1}', '$.missing', 9) AS object_missing_member, "
                   "JSON_ARRAY_APPEND('{\"a\":1}', '$.a[0]', 9) AS member_index_autowrap, "
                   "JSON_ARRAY_APPEND('{\"a\":1}', '$[0]', 9) AS object_array_leg_autowrap, "
                   "JSON_ARRAY_APPEND('1', '$[0][0]', 9) AS nested_scalar_array_leg_autowrap",
            .columns = columns_append,
            .column_count = sizeof(columns_append) / sizeof(columns_append[0]),
            .values = values_append,
            .row_count = 1U,
            .context = "literal json_array_append values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '$[1]', 'x') "
                   "AS array_middle, "
                   "JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '$[100]', 'x') "
                   "AS array_append, "
                   "JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '$[1].b[0]', 'x') "
                   "AS nested_array_start, "
                   "JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '$[2][1]', 'y') "
                   "AS nested_array_middle, "
                   "JSON_ARRAY_INSERT('[\"a\", {\"b\":[1,2]}, [3,4]]', '$[0]', 'x', "
                   "'$[2][1]', 'y') AS left_to_right, "
                   "JSON_ARRAY_INSERT('{\"a\":1}', '$.a[0]', 9) AS scalar_parent_noop, "
                   "JSON_ARRAY_INSERT('{\"a\":1}', '$.missing[0]', 9) AS missing_parent_noop, "
                   "JSON_ARRAY_INSERT('{\"a\":1}', '$[0]', 9) AS "
                   "root_object_array_cell_parent_noop, "
                   "JSON_ARRAY_INSERT('[1]', '$[0][0]', 9) AS nested_scalar_parent_noop",
            .columns = columns_insert,
            .column_count = sizeof(columns_insert) / sizeof(columns_insert[0]),
            .values = values_insert,
            .row_count = 1U,
            .context = "literal json_array_insert values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_ARRAY_APPEND(NULL, '$', 1) AS append_null_document, "
                   "JSON_ARRAY_APPEND('{\"a\":1}', NULL, 1) AS append_null_path, "
                   "JSON_ARRAY_APPEND('{\"a\":1}', '$.a', NULL) AS append_null_value, "
                   "JSON_ARRAY_APPEND('{\"a\":1}', '$.a', 1, NULL, 2, 'bad', 3) "
                   "AS append_later_null_path, "
                   "JSON_ARRAY_INSERT(NULL, '$[0]', 1) AS insert_null_document, "
                   "JSON_ARRAY_INSERT('[1]', NULL, 1) AS insert_null_path, "
                   "JSON_ARRAY_INSERT('[1]', '$[0]', NULL) AS insert_null_value, "
                   "JSON_ARRAY_INSERT('[1]', '$[0]', 1, NULL, 2, 'bad', 3) "
                   "AS insert_later_null_path",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 1U,
            .context = "literal JSON array mutation NULL values",
        }
    );

    failures +=
        execute_ok(database, "SELECT JSON_ARRAY_APPEND('[1]', '$', 2) AS appended", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            1U,
            "json_array_append metadata"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_JSON,
            "json_array_append metadata type"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_nullable(result, 0U),
            1,
            "json_array_append nullable"
        );
        failures +=
            expect_result_value(result, 0U, 0U, "[1, 2]", "json_array_append metadata value");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_dual_do_and_status(void) {
    static const char *const columns_dual[] = {
        "appended",
        "JSON_ARRAY_INSERT('[1]','$[0]',2)",
    };
    static const char *const values_dual[] = {"[1, 2]", "[2, 1]"};
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
            .sql = "SELECT JSON_ARRAY_APPEND('[1]', '$', 2) AS appended, "
                   "JSON_ARRAY_INSERT('[1]','$[0]',2) FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "JSON array mutation from dual",
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
            .context = "row count after JSON array mutation select",
        }
    );
    failures += execute_ok(
        database,
        "DO JSON_ARRAY_APPEND('[1]', '$', 2), JSON_ARRAY_INSERT('[1]', '$[0]', 2)",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "JSON array mutation do columns"
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            0U,
            "JSON array mutation do rows"
        );
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "JSON array mutation do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "JSON array mutation do warnings"
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
            .context = "row count after JSON array mutation do",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_json_array_mutation_values(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_ARRAY_APPEND(j, '$.a', i)",
        "JSON_ARRAY_INSERT(j, '$.b[1]', i)",
        "JSON_ARRAY_APPEND(j, '$.b', label)",
        "JSON_ARRAY_INSERT(j, '$.b[1]', j)",
        "JSON_ARRAY_INSERT(j, '$.b[1]', doc_text)",
    };
    static const char *const values_table[] = {
        "1",
        "{\"a\": [1, 7], \"b\": [2, 3], \"c\": \"x\"}",
        "{\"a\": 1, \"b\": [2, 7, 3], \"c\": \"x\"}",
        "{\"a\": 1, \"b\": [2, 3, \"name\"], \"c\": \"x\"}",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "{\"a\": 1, \"b\": [2, {\"a\": 1, \"b\": [2, 3], \"c\": \"x\"}, 3], \"c\": \"x\"}",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "{\"a\": 1, \"b\": [2, \"{\\\"a\\\":1,\\\"b\\\":[2,3],\\\"c\\\":\\\"x\\\"}\", 3], "
        "\"c\": \"x\"}",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_nested[] = {
        "id",
        "appended_i",
        "appended_array_type",
        "inserted_i",
    };
    static const char *const values_nested[] = {
        "1",
        "7",
        "ARRAY",
        "7",
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
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, j JSON, i INT, label VARCHAR(10), doc_text LONGTEXT, "
        "b VARBINARY(10))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', 7, 'name', "
        "'{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', x'6162'), "
        "(2, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_ARRAY_APPEND(j, '$.a', i), "
                   "JSON_ARRAY_INSERT(j, '$.b[1]', i), "
                   "JSON_ARRAY_APPEND(j, '$.b', label), "
                   "JSON_ARRAY_INSERT(j, '$.b[1]', j), "
                   "JSON_ARRAY_INSERT(j, '$.b[1]', doc_text) FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table JSON array mutation projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_EXTRACT(JSON_ARRAY_APPEND(j, '$.a', i), '$.a[1]') "
                   "AS appended_i, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_ARRAY_APPEND(j, '$.b', JSON_ARRAY(i)), "
                   "'$.b[2]')) AS appended_array_type, "
                   "JSON_EXTRACT(JSON_ARRAY_INSERT(j, '$.b[1]', i), '$.b[1]') "
                   "AS inserted_i FROM t ORDER BY id",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 2U,
            .context = "row-scalar JSON introspection consumes array mutation result",
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
            .context = "JSON array mutation does not mutate source rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_array_mutation_diagnostics(void) {
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
        execute_ok(database, "INSERT INTO t VALUES ('{\"a\":[1]}', 1, x'6162', '{bad}')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND('{}', '$')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND('{}', '$', 1, '$')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND('{bad}', '$', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND('{}', 'bad', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND('{}', '$.*', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_ARRAY_APPEND() path or document shape is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND(1, '$', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND(j, n, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_ARRAY_APPEND() supports only string and NULL path literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_APPEND(b, '$', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT('[1]', '$[0]')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT('bad', '$[0]', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT('[1]', 'bad', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT('[1]', '$.*', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_ARRAY_INSERT() path or document shape is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT('{}', '$', 1)",
        (struct expected_sql_error){
            .code = mysql_error_json_path_not_array_cell,
            .sqlstate = "42000",
            .message_part = "A path expression is not a path to a cell in an array",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT('{}', '$.a', 1)",
        (struct expected_sql_error){
            .code = mysql_error_json_path_not_array_cell,
            .sqlstate = "42000",
            .message_part = "A path expression is not a path to a cell in an array",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT('[1]', '$', 1, NULL, 2)",
        (struct expected_sql_error){
            .code = mysql_error_json_path_not_array_cell,
            .sqlstate = "42000",
            .message_part = "A path expression is not a path to a cell in an array",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT(1, '$[0]', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_ARRAY_INSERT(j, n, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_ARRAY_INSERT() supports only string and NULL path literals",
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
