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

static int test_no_source_dual_and_do_json_introspection(void);
static int test_table_backed_json_introspection_and_reopen(void);
static int test_json_introspection_diagnostics(void);
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

    failures += test_no_source_dual_and_do_json_introspection();
    failures += test_table_backed_json_introspection_and_reopen();
    failures += test_json_introspection_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_json_introspection(void) {
    static const char *const columns_type[] = {
        "null_sql",
        "object_type",
        "array_type",
        "true_type",
        "false_type",
        "json_null_type",
        "string_type",
        "integer_type",
        "negative_type",
    };
    static const char *const values_type[] = {
        NULL,
        "OBJECT",
        "ARRAY",
        "BOOLEAN",
        "BOOLEAN",
        "NULL",
        "STRING",
        "INTEGER",
        "INTEGER",
    };
    static const char *const columns_length[] = {
        "null_sql",
        "int_len",
        "null_len",
        "string_len",
        "array_len",
        "object_len",
        "path_len",
        "missing_len",
        "null_path_len",
        "null_doc_bad_path_len",
    };
    static const char *const values_length[] = {
        NULL,
        "1",
        "1",
        "1",
        "3",
        "2",
        "1",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_depth[] = {
        "null_sql",
        "object_depth",
        "array_depth",
        "scalar_depth",
        "nested_depth",
    };
    static const char *const values_depth[] = {NULL, "1", "2", "1", "4"};
    static const char *const columns_storage[] = {
        "null_size",
        "empty_object_size",
        "empty_array_size",
        "object_size",
        "array_size",
        "string_size",
        "free_null",
        "free_object",
        "free_mutated",
    };
    static const char *const values_storage[] = {
        NULL,
        "5",
        "5",
        "13",
        "14",
        "5",
        NULL,
        "0",
        "0",
    };
    static const char *const columns_pretty[] = {
        "scalar_pretty",
        "object_pretty",
        "array_pretty",
        "empty_object_pretty",
        "null_pretty",
    };
    static const char expected_pretty_array[] = "[\n"
                                                "  1,\n"
                                                "  {\n"
                                                "    \"a\": [\n"
                                                "      true,\n"
                                                "      false,\n"
                                                "      null,\n"
                                                "      \"x\"\n"
                                                "    ]\n"
                                                "  }\n"
                                                "]";
    static const char *const values_pretty[] = {
        "123",
        "{\n  \"a\": 2,\n  \"b\": 1\n}",
        expected_pretty_array,
        "{}",
        NULL,
    };
    static const char *const columns_keys[] = {
        "root_keys",
        "nested_keys",
        "empty_keys",
        "array_keys",
        "missing_keys",
        "scalar_path_keys",
        "null_doc_keys",
        "null_path_keys",
        "null_doc_bad_path_keys",
        "ordered_keys",
        "duplicate_keys",
    };
    static const char *const values_keys[] = {
        "[\"a\", \"b\"]",
        "[\"c\"]",
        "[]",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "[\"a\", \"m\", \"z\"]",
        "[\"a\"]",
    };
    static const char *const columns_nested[] = {
        "nested_type",
        "nested_length",
        "nested_depth",
        "nested_pretty",
    };
    static const char *const values_nested[] = {"ARRAY", "2", "2", "[\n  1,\n  true\n]"};
    static const char *const columns_dual[] = {"jt", "jl", "jd", "jss", "jsf", "jp", "jk"};
    static const char *const values_dual[] = {
        "OBJECT",
        "2",
        "3",
        "13",
        "0",
        "{\n  \"a\": 1\n}",
        "[\"a\"]",
    };
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
            .sql = "SELECT JSON_TYPE(NULL) AS null_sql, JSON_TYPE('{}') AS object_type, "
                   "JSON_TYPE('[]') AS array_type, JSON_TYPE('true') AS true_type, "
                   "JSON_TYPE('false') AS false_type, JSON_TYPE('null') AS json_null_type, "
                   "JSON_TYPE('\"x\"') AS string_type, JSON_TYPE('1') AS integer_type, "
                   "JSON_TYPE('-1') AS negative_type",
            .columns = columns_type,
            .column_count = sizeof(columns_type) / sizeof(columns_type[0]),
            .values = values_type,
            .row_count = 1U,
            .context = "literal json_type values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_LENGTH(NULL) AS null_sql, JSON_LENGTH('1') AS int_len, "
                   "JSON_LENGTH('null') AS null_len, JSON_LENGTH('\"x\"') AS string_len, "
                   "JSON_LENGTH('[1,2,{\"a\":3}]') AS array_len, "
                   "JSON_LENGTH('{\"a\":1,\"b\":{\"c\":30}}') AS object_len, "
                   "JSON_LENGTH('{\"a\":1,\"b\":{\"c\":30}}', '$.b') AS path_len, "
                   "JSON_LENGTH('{\"a\":1}', '$.missing') AS missing_len, "
                   "JSON_LENGTH('{\"a\":1}', NULL) AS null_path_len, "
                   "JSON_LENGTH(NULL, 'bad') AS null_doc_bad_path_len",
            .columns = columns_length,
            .column_count = sizeof(columns_length) / sizeof(columns_length[0]),
            .values = values_length,
            .row_count = 1U,
            .context = "literal json_length values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_DEPTH(NULL) AS null_sql, JSON_DEPTH('{}') AS object_depth, "
                   "JSON_DEPTH('[[],{}]') AS array_depth, "
                   "JSON_DEPTH('true') AS scalar_depth, "
                   "JSON_DEPTH('{\"a\":{\"b\":[1]}}') AS nested_depth",
            .columns = columns_depth,
            .column_count = sizeof(columns_depth) / sizeof(columns_depth[0]),
            .values = values_depth,
            .row_count = 1U,
            .context = "literal json_depth values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_STORAGE_SIZE(NULL) AS null_size, "
                   "JSON_STORAGE_SIZE('{}') AS empty_object_size, "
                   "JSON_STORAGE_SIZE('[]') AS empty_array_size, "
                   "JSON_STORAGE_SIZE('{\"a\":1}') AS object_size, "
                   "JSON_STORAGE_SIZE('[1,2,3]') AS array_size, "
                   "JSON_STORAGE_SIZE('\"abc\"') AS string_size, "
                   "JSON_STORAGE_FREE(NULL) AS free_null, "
                   "JSON_STORAGE_FREE('{}') AS free_object, "
                   "JSON_STORAGE_FREE(JSON_SET('{\"a\":1}', '$.a', 2)) AS free_mutated",
            .columns = columns_storage,
            .column_count = sizeof(columns_storage) / sizeof(columns_storage[0]),
            .values = values_storage,
            .row_count = 1U,
            .context = "literal json storage values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_PRETTY('123') AS scalar_pretty, "
                   "JSON_PRETTY('{\"b\":1,\"a\":2}') AS object_pretty, "
                   "JSON_PRETTY('[1,{\"a\":[true,false,null,\"x\"]}]') AS array_pretty, "
                   "JSON_PRETTY('{}') AS empty_object_pretty, "
                   "JSON_PRETTY(NULL) AS null_pretty",
            .columns = columns_pretty,
            .column_count = sizeof(columns_pretty) / sizeof(columns_pretty[0]),
            .values = values_pretty,
            .row_count = 1U,
            .context = "literal json_pretty values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_KEYS('{\"a\":1,\"b\":{\"c\":30}}') AS root_keys, "
                   "JSON_KEYS('{\"a\":1,\"b\":{\"c\":30}}', '$.b') AS nested_keys, "
                   "JSON_KEYS('{}') AS empty_keys, JSON_KEYS('[]') AS array_keys, "
                   "JSON_KEYS('{\"a\":1}', '$.missing') AS missing_keys, "
                   "JSON_KEYS('{\"a\":1}', '$.a') AS scalar_path_keys, "
                   "JSON_KEYS(NULL) AS null_doc_keys, "
                   "JSON_KEYS('{\"a\":1}', NULL) AS null_path_keys, "
                   "JSON_KEYS(NULL, 'bad') AS null_doc_bad_path_keys, "
                   "JSON_KEYS('{\"z\":1,\"a\":2,\"m\":3}') AS ordered_keys, "
                   "JSON_KEYS('{\"a\":1,\"a\":2}') AS duplicate_keys",
            .columns = columns_keys,
            .column_count = sizeof(columns_keys) / sizeof(columns_keys[0]),
            .values = values_keys,
            .row_count = 1U,
            .context = "literal json_keys values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_TYPE(JSON_EXTRACT('{\"a\":[1,true]}', '$.a')) AS nested_type, "
                   "JSON_LENGTH(JSON_EXTRACT('{\"a\":[1,true]}', '$.a')) AS nested_length, "
                   "JSON_DEPTH(JSON_EXTRACT('{\"a\":[1,true]}', '$.a')) AS nested_depth, "
                   "JSON_PRETTY(JSON_EXTRACT('{\"a\":[1,true]}', '$.a')) AS nested_pretty",
            .columns = columns_nested,
            .column_count = sizeof(columns_nested) / sizeof(columns_nested[0]),
            .values = values_nested,
            .row_count = 1U,
            .context = "nested json introspection values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_TYPE('{\"a\":1}') AS jt, "
                   "JSON_LENGTH('{\"a\":1,\"b\":2}') AS jl, "
                   "JSON_DEPTH('[1,[2]]') AS jd, "
                   "JSON_STORAGE_SIZE('{\"a\":1}') AS jss, "
                   "JSON_STORAGE_FREE('{\"a\":1}') AS jsf, "
                   "JSON_PRETTY('{\"a\":1}') AS jp, "
                   "JSON_KEYS('{\"a\":1}') AS jk FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "json introspection from dual",
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
            .context = "row count after json introspection select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_TYPE('{\"a\":1}'), JSON_LENGTH('[1,2]'), JSON_DEPTH('[1,[2]]'), "
        "JSON_STORAGE_SIZE('{\"a\":1}'), JSON_STORAGE_FREE('{\"a\":1}'), "
        "JSON_PRETTY('{\"a\":1}'), JSON_KEYS('{\"a\":1}')",
        &result
    );
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 0U, "json introspection do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "json introspection do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "json introspection do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "json introspection do warnings");
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
            .context = "row count after json introspection do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_json_introspection_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_TYPE(j)",
        "JSON_LENGTH(j)",
        "JSON_KEYS(j)",
        "JSON_TYPE(JSON_EXTRACT(j, '$.a'))",
        "JSON_LENGTH(JSON_EXTRACT(j, '$.a'))",
        "JSON_DEPTH(j)",
        "JSON_STORAGE_SIZE(j)",
        "JSON_STORAGE_FREE(j)",
        "JSON_PRETTY(JSON_EXTRACT(j, '$.b'))",
        "JSON_KEYS(JSON_EXTRACT(j, '$.b'))",
        "JSON_STORAGE_SIZE(JSON_EXTRACT(j, '$.a'))",
        "JSON_STORAGE_FREE(JSON_EXTRACT(j, '$.a'))",
        "JSON_DEPTH(v)",
        "JSON_LENGTH(v, '$.x')",
        "JSON_STORAGE_SIZE(v)",
        "JSON_STORAGE_FREE(v)",
        "JSON_KEYS(v)",
        "JSON_LENGTH(j, NULL)",
        "JSON_KEYS(j, NULL)",
    };
    static const char *const values_table[] = {
        "1",       "OBJECT",
        "2",       "[\"a\", \"b\"]",
        "ARRAY",   "2",
        "3",       "43",
        "0",       "{\n  \"c\": 3\n}",
        "[\"c\"]", "11",
        "0",       "3",
        "2",       "23",
        "0",       "[\"x\"]",
        NULL,      NULL,
        "2",       NULL,
        NULL,      NULL,
        NULL,      NULL,
        NULL,      NULL,
        NULL,      NULL,
        NULL,      NULL,
        NULL,      NULL,
        NULL,      NULL,
        NULL,      NULL,
        NULL,      NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON, v VARCHAR(100))", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":[1,2],\"b\":{\"c\":3}}', '{\"x\":[10,20]}'), "
        "(2, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_TYPE(j), JSON_LENGTH(j), "
                   "JSON_KEYS(j), "
                   "JSON_TYPE(JSON_EXTRACT(j, '$.a')), "
                   "JSON_LENGTH(JSON_EXTRACT(j, '$.a')), "
                   "JSON_DEPTH(j), JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j), "
                   "JSON_PRETTY(JSON_EXTRACT(j, '$.b')), "
                   "JSON_KEYS(JSON_EXTRACT(j, '$.b')), "
                   "JSON_STORAGE_SIZE(JSON_EXTRACT(j, '$.a')), "
                   "JSON_STORAGE_FREE(JSON_EXTRACT(j, '$.a')), "
                   "JSON_DEPTH(v), JSON_LENGTH(v, '$.x'), "
                   "JSON_STORAGE_SIZE(v), JSON_STORAGE_FREE(v), JSON_KEYS(v), "
                   "JSON_LENGTH(j, NULL), JSON_KEYS(j, NULL) "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table json introspection values",
        }
    );

    mylite_close(database);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json introspection");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_TYPE(j), JSON_LENGTH(j), "
                   "JSON_KEYS(j), "
                   "JSON_TYPE(JSON_EXTRACT(j, '$.a')), "
                   "JSON_LENGTH(JSON_EXTRACT(j, '$.a')), "
                   "JSON_DEPTH(j), JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j), "
                   "JSON_PRETTY(JSON_EXTRACT(j, '$.b')), "
                   "JSON_KEYS(JSON_EXTRACT(j, '$.b')), "
                   "JSON_STORAGE_SIZE(JSON_EXTRACT(j, '$.a')), "
                   "JSON_STORAGE_FREE(JSON_EXTRACT(j, '$.a')), "
                   "JSON_DEPTH(v), JSON_LENGTH(v, '$.x'), "
                   "JSON_STORAGE_SIZE(v), JSON_STORAGE_FREE(v), JSON_KEYS(v), "
                   "JSON_LENGTH(j, NULL), JSON_KEYS(j, NULL) "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "reopen table json introspection values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_introspection_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(20))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '{\"a\":1}', '{\"a\":1}')", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (2, '{\"a\":1}', 'bad')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_TYPE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'JSON_TYPE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_TYPE('{}', '$')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'JSON_TYPE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH('{}', '$', '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_LENGTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_DEPTH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_DEPTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_DEPTH('{}', '$')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_DEPTH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_SIZE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_STORAGE_SIZE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_SIZE('{}', '$')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_STORAGE_SIZE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_FREE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_STORAGE_FREE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_FREE('{}', '$')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_STORAGE_FREE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_PRETTY()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_PRETTY'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_PRETTY('{}', '$')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_PRETTY'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'JSON_KEYS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS('{}', '$', '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'JSON_KEYS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_TYPE(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_DEPTH(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_SIZE(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_FREE(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_PRETTY(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_TYPE('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_DEPTH('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_SIZE('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_FREE('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_PRETTY('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH('bad', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS('bad', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_TYPE(CAST('{\"a\":1}' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH(CAST('{\"a\":1}' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS(CAST('{\"a\":1}' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_DEPTH(CAST('{\"a\":1}' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_SIZE(CAST('{\"a\":1}' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_FREE(CAST('{\"a\":1}' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_PRETTY(CAST('{\"a\":1}' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH('{\"a\":1}', 'bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS('{\"a\":1}', 'bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH('{\"a\":1}', '$.*')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_LENGTH() path expression is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS('{\"a\":1}', '$.*')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_KEYS() path expression is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_TYPE(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_DEPTH(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_SIZE(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_FREE(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_PRETTY(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH(s) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS(s) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_DEPTH(s) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_SIZE(s) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_STORAGE_FREE(s) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_PRETTY(s) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_LENGTH(s, NULL) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_KEYS(s, NULL) FROM t WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
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
        "/tmp/mylite-json-introspection-functions-%s-%d.mylite",
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
