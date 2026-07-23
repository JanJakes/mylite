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

static int test_literal_json_insert_values(void);
static int test_dual_do_and_status(void);
static int test_table_backed_json_insert_values(void);
static int test_json_insert_diagnostics(void);
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

    failures += test_literal_json_insert_values();
    failures += test_dual_do_and_status();
    failures += test_table_backed_json_insert_values();
    failures += test_json_insert_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_literal_json_insert_values(void) {
    static const char *const columns_object[] = {
        "existing_member",
        "new_member",
        "nested",
        "missing_parent",
        "duplicate_path",
        "root_noop",
    };
    static const char *const values_object[] = {
        "{\"a\": 1}",
        "{\"a\": 1, \"b\": 2}",
        "{\"a\": {\"b\": 1, \"c\": 2}}",
        "{\"a\": 1}",
        "{\"a\": 1}",
        "{\"a\": 1}",
    };
    static const char *const columns_array[] = {
        "array_first",
        "array_middle",
        "array_append",
        "array_far_append",
        "array_leading_zero",
        "scalar_index_zero",
        "scalar_autowrap",
        "string_index_zero",
        "string_autowrap",
    };
    static const char *const values_array[] = {
        "[1, 2]",
        "[1, 2]",
        "[1, 2, 9]",
        "[1, 2, 9]",
        "[1, 2]",
        "1",
        "[1, 2]",
        "\"x\"",
        "[\"x\", 2]",
    };
    static const char *const columns_values[] = {
        "json_existing_noop",
        "json_array_value",
        "string_value",
        "string_value_type",
        "json_array_value_type",
        "json_null_value_noop",
        "json_null_value_insert",
    };
    static const char *const values_values[] = {
        "{\"a\": 1}",
        "{\"a\": [1, 2]}",
        "{\"a\": \"[1,2]\"}",
        "STRING",
        "ARRAY",
        "{\"a\": 1}",
        "{\"b\": null}",
    };
    static const char *const columns_nulls[] = {
        "null_document",
        "null_path",
        "null_path_then_invalid",
        "root_null_value",
        "new_member_then_null_path",
    };
    static const char *const values_nulls[] = {NULL, NULL, NULL, "{}", NULL};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_INSERT('{\"a\":1}', '$.a', 2) AS existing_member, "
                   "JSON_INSERT('{\"a\":1}', '$.b', 2) AS new_member, "
                   "JSON_INSERT('{\"a\":{\"b\":1}}', '$.a.c', 2) AS nested, "
                   "JSON_INSERT('{\"a\":1}', '$.b.c', 2) AS missing_parent, "
                   "JSON_INSERT('{}', '$.a', 1, '$.a', 2) AS duplicate_path, "
                   "JSON_INSERT('{\"a\":1}', '$', 2) AS root_noop",
            .columns = columns_object,
            .column_count = sizeof(columns_object) / sizeof(columns_object[0]),
            .values = values_object,
            .row_count = 1U,
            .context = "literal object json_insert values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_INSERT('[1,2]', '$[0]', 9) AS array_first, "
                   "JSON_INSERT('[1,2]', '$[1]', 9) AS array_middle, "
                   "JSON_INSERT('[1,2]', '$[2]', 9) AS array_append, "
                   "JSON_INSERT('[1,2]', '$[99]', 9) AS array_far_append, "
                   "JSON_INSERT('[1,2]', '$[01]', 9) AS array_leading_zero, "
                   "JSON_INSERT('1', '$[0]', 2) AS scalar_index_zero, "
                   "JSON_INSERT('1', '$[1]', 2) AS scalar_autowrap, "
                   "JSON_INSERT('\"x\"', '$[0]', 2) AS string_index_zero, "
                   "JSON_INSERT('\"x\"', '$[2]', 2) AS string_autowrap",
            .columns = columns_array,
            .column_count = sizeof(columns_array) / sizeof(columns_array[0]),
            .values = values_array,
            .row_count = 1U,
            .context = "literal array json_insert values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_INSERT('{\"a\":1}', '$.a', "
                   "JSON_EXTRACT('{\"x\":2}', '$.x')) AS json_existing_noop, "
                   "JSON_INSERT('{}', '$.a', JSON_ARRAY(1,2)) AS json_array_value, "
                   "JSON_INSERT('{}', '$.a', '[1,2]') AS string_value, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_INSERT('{}', '$.a', '[1,2]'), '$.a')) "
                   "AS string_value_type, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_INSERT('{}', '$.a', JSON_ARRAY(1,2)), "
                   "'$.a')) AS json_array_value_type, "
                   "JSON_INSERT('{\"a\":1}', '$.a', NULL) AS json_null_value_noop, "
                   "JSON_INSERT('{}', '$.b', NULL) AS json_null_value_insert",
            .columns = columns_values,
            .column_count = sizeof(columns_values) / sizeof(columns_values[0]),
            .values = values_values,
            .row_count = 1U,
            .context = "json_insert JSON and string value arguments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_INSERT(NULL, '$.a', 1) AS null_document, "
                   "JSON_INSERT('{\"a\":1}', NULL, 2) AS null_path, "
                   "JSON_INSERT('{}', NULL, 'a') AS null_path_then_invalid, "
                   "JSON_INSERT('{}', '$', NULL) AS root_null_value, "
                   "JSON_INSERT('{}', '$.a', 1, NULL, 2) AS new_member_then_null_path",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 1U,
            .context = "json_insert null handling",
        }
    );

    failures += execute_ok(database, "SELECT JSON_INSERT('{}', '$.a', 1) AS inserted", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 1U, "json_insert metadata");
        failures += mylite_test_expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_JSON,
            "json_insert metadata type"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_nullable(result, 0U),
            1,
            "json_insert nullable"
        );
        failures += expect_result_value(result, 0U, 0U, "{\"a\": 1}", "json_insert metadata value");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_dual_do_and_status(void) {
    static const char *const columns_dual[] = {"inserted", "JSON_INSERT('{}','$.a',1)"};
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
            .sql = "SELECT JSON_INSERT('{}', '$.a', 1) AS inserted, "
                   "JSON_INSERT('{}','$.a',1) FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "json_insert from dual",
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
            .context = "row count after json_insert select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_INSERT('{}', '$.a', 1), JSON_INSERT('{\"a\":1}', '$.a', 2)",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "json_insert do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "json_insert do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "json_insert do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "json_insert do warnings"
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
            .context = "row count after json_insert do",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_json_insert_values(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_INSERT(j, '$.a', 9)",
        "JSON_INSERT(j, '$.i', i)",
        "JSON_INSERT(j, '$.flag', flag)",
        "JSON_INSERT(j, '$.copy', j)",
        "JSON_INSERT(j, '$.label', label)",
        "JSON_INSERT(j, '$.doc', doc_text)",
    };
    static const char *const values_table[] = {
        "1",
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\"}",
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\", \"i\": 7}",
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\", \"flag\": 1}",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\", \"copy\": {\"a\": 1, \"b\": [2, 3], \"c\": "
        "\"x\"}}",
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\", \"label\": \"name\"}",
        // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\", \"doc\": "
        "\"{\\\"a\\\":1,\\\"b\\\":[2,3],\\\"c\\\":\\\"x\\\"}\"}",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "JSON_INSERT(j, '$.i', i)"};
    static const char *const values_limited[] = {
        "1",
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\", \"i\": 7}",
    };
    static const char *const columns_null_path[] = {
        "id",
        "JSON_INSERT(j, NULL, i + 1)",
        "JSON_INSERT(j, '$.i', i, NULL, i + 1)",
    };
    static const char *const values_null_path[] = {
        "1",
        NULL,
        NULL,
        "2",
        NULL,
        NULL,
    };
    static const char *const values_reopen[] = {
        "1",
        "{\"a\": 1, \"b\": [2, 3], \"c\": \"x\", \"i\": 7}",
        "2",
        NULL,
    };
    static const char *const columns_nested[] = {
        "id",
        "inserted_i",
        "inserted_label",
        "inserted_array_type",
    };
    static const char *const values_nested[] = {
        "1",
        "7",
        "name",
        "ARRAY",
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
        "CREATE TABLE t(id INT, j JSON, i INT, flag TINYINT, label VARCHAR(10), "
        "doc_text LONGTEXT, b VARBINARY(10))",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', 7, 1, 'name', "
        "'{\"a\":1,\"b\":[2,3],\"c\":\"x\"}', x'6162'), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_INSERT(j, '$.a', 9), JSON_INSERT(j, '$.i', i), "
                   "JSON_INSERT(j, '$.flag', flag), JSON_INSERT(j, '$.copy', j), "
                   "JSON_INSERT(j, '$.label', label), JSON_INSERT(j, '$.doc', doc_text) "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table json_insert projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_INSERT(j, '$.i', i) "
                   "FROM t WHERE id >= 1 ORDER BY id LIMIT 1",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 1U,
            .context = "table json_insert where order limit projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_INSERT(j, NULL, i + 1), "
                   "JSON_INSERT(j, '$.i', i, NULL, i + 1) FROM t ORDER BY id",
            .columns = columns_null_path,
            .column_count = sizeof(columns_null_path) / sizeof(columns_null_path[0]),
            .values = values_null_path,
            .row_count = 2U,
            .context = "table json_insert null path skips value planning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_EXTRACT(JSON_INSERT(j, '$.i', i), '$.i') AS inserted_i, "
                   "JSON_UNQUOTE(JSON_EXTRACT(JSON_INSERT(j, '$.label', label), '$.label')) "
                   "AS inserted_label, "
                   "JSON_TYPE(JSON_EXTRACT(JSON_INSERT(j, '$.arr', JSON_ARRAY(i)), '$.arr')) "
                   "AS inserted_array_type FROM t ORDER BY id",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 2U,
            .context = "row-scalar json_extract consumes json_insert result",
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
            .context = "json_insert does not mutate source rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json_insert");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_INSERT(j, '$.i', i) FROM t ORDER BY id",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_reopen,
            .row_count = 2U,
            .context = "reopen table json_insert values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_insert_diagnostics(void) {
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
        "SELECT JSON_INSERT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT('{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT('{}', '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT('{bad}', '$.a', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT('{bad}', NULL, 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT(doc_text, NULL, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT('{}', 'a', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT('{}', 'a', 1, NULL, 2)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT(j, 'a', 1, NULL, 2) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT(1, '$.a', 2)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT('{}', '$.*', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_INSERT() path or document shape is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT(j, n, 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_INSERT() supports only string and NULL path literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT(missing, '$.a', 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_INSERT(b, '$.a', 1) FROM t",
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
