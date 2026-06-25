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

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_no_source_dual_and_do_json_contains(void);
static int test_table_backed_json_contains_and_reopen(void);
static int test_independent_json_contains_handles(void);
static int test_json_contains_diagnostics(void);
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

    failures += test_no_source_dual_and_do_json_contains();
    failures += test_table_backed_json_contains_and_reopen();
    failures += test_independent_json_contains_handles();
    failures += test_json_contains_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_json_contains(void) {
    static const char *const columns_contains[] = {
        "c1",
        "c2",
        "c3",
        "c4",
        "c5",
        "c6",
        "c7",
        "c8",
    };
    static const char *const values_contains[] = {"1", "0", "1", "0", "1", "1", "0", "0"};
    static const char *const columns_path[] = {
        "a",
        "array_member",
        "array_index",
        "missing",
        "null_target",
        "null_candidate",
        "null_path",
    };
    static const char *const values_path[] = {"1", "1", "1", NULL, NULL, NULL, NULL};
    static const char *const columns_contains_path[] = {
        "one_hit",
        "all_hit",
        "all_miss",
        "one_miss",
        "upper_one",
        "upper_all",
        "null_doc",
        "null_mode",
        "null_path",
        "all_null",
    };
    static const char *const values_contains_path[] =
        {"1", "1", "0", "0", "1", "1", NULL, NULL, NULL, NULL};
    static const char *const columns_dual[] = {"has_a", "path_a"};
    static const char *const values_dual[] = {"1", "1"};
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
            .sql = "SELECT JSON_CONTAINS('1','1') AS c1, "
                   "JSON_CONTAINS('1','2') AS c2, "
                   "JSON_CONTAINS('true','true') AS c3, "
                   "JSON_CONTAINS('true','false') AS c4, "
                   "JSON_CONTAINS('null','null') AS c5, "
                   "JSON_CONTAINS('\"x\"','\"x\"') AS c6, "
                   "JSON_CONTAINS('\"x\"','\"y\"') AS c7, "
                   "JSON_CONTAINS('{\"a\":1}','{\"b\":1}') AS c8",
            .columns = columns_contains,
            .column_count = sizeof(columns_contains) / sizeof(columns_contains[0]),
            .values = values_contains,
            .row_count = 1U,
            .context = "literal json_contains scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_CONTAINS('{\"a\":1,\"b\":[2,3]}','1','$.a') AS a, "
                   "JSON_CONTAINS('{\"a\":1,\"b\":[2,3]}','2','$.b') AS array_member, "
                   "JSON_CONTAINS('{\"a\":1,\"b\":[2,3]}','3','$.b[1]') AS array_index, "
                   "JSON_CONTAINS('{\"a\":1}','1','$.missing') AS missing, "
                   "JSON_CONTAINS(NULL,'1') AS null_target, "
                   "JSON_CONTAINS('{\"a\":1}',NULL) AS null_candidate, "
                   "JSON_CONTAINS('{\"a\":1}','1',NULL) AS null_path",
            .columns = columns_path,
            .column_count = sizeof(columns_path) / sizeof(columns_path[0]),
            .values = values_path,
            .row_count = 1U,
            .context = "json_contains path and null values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_CONTAINS_PATH('{\"a\":1,\"b\":{\"c\":2}}','one','$.a','$.x') "
                   "AS one_hit, "
                   "JSON_CONTAINS_PATH('{\"a\":1,\"b\":{\"c\":2}}','all','$.a','$.b.c') "
                   "AS all_hit, "
                   "JSON_CONTAINS_PATH('{\"a\":1,\"b\":{\"c\":2}}','all','$.a','$.x') "
                   "AS all_miss, "
                   "JSON_CONTAINS_PATH('{\"a\":1}','one','$.x') AS one_miss, "
                   "JSON_CONTAINS_PATH('{\"a\":1}','ONE','$.a') AS upper_one, "
                   "JSON_CONTAINS_PATH('{\"a\":1}','ALL','$.a') AS upper_all, "
                   "JSON_CONTAINS_PATH(NULL,'one','bad') AS null_doc, "
                   "JSON_CONTAINS_PATH('{\"a\":1}',NULL,'bad') AS null_mode, "
                   "JSON_CONTAINS_PATH('{\"a\":1}','one',NULL) AS null_path, "
                   "JSON_CONTAINS_PATH(NULL,NULL,NULL) AS all_null",
            .columns = columns_contains_path,
            .column_count = sizeof(columns_contains_path) / sizeof(columns_contains_path[0]),
            .values = values_contains_path,
            .row_count = 1U,
            .context = "literal json_contains_path values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_CONTAINS('{\"a\":1}', '1', '$.a') AS has_a, "
                   "JSON_CONTAINS_PATH('{\"a\":1}', 'one', '$.a') AS path_a FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "json contains from dual",
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
            .context = "row count after json contains select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_CONTAINS('{\"a\":1}', '1', '$.a'), "
        "JSON_CONTAINS_PATH('{\"a\":1}', 'one', '$.a')",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "json contains do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "json contains do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "json contains do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "json contains do warnings");
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
            .context = "row count after json contains do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_json_contains_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_CONTAINS(j, '1', '$.a')",
        "JSON_CONTAINS(j, '\"blue\"', '$.tags')",
        "JSON_CONTAINS(s, '{\"a\":1}')",
        "JSON_CONTAINS_PATH(j, 'one', '$.o.k')",
    };
    static const char *const values_table[] = {
        "1", "1",  "1",  "1",  "1",  "2", "0",  "0",  "0", "0",
        "3", NULL, NULL, NULL, NULL, "4", NULL, NULL, "0", "0",
    };
    static const char *const columns_id[] = {"id"};
    static const char *const values_blue_rows[] = {"1"};
    static const char *const values_path_rows[] = {"1"};
    static const char *const values_a_rows[] = {"1"};
    static const char *const values_missing_path_rows[] = {"1", "2", "4"};
    static const char *const values_null_contains_rows[] = {"3", "4"};
    static const char *const columns_remaining[] =
        {"id", "marker", "JSON_CONTAINS(j, '\"blue\"', '$.tags')"};
    static const char *const values_remaining[] = {"1", "9", "1", "3", "0", NULL, "4", "0", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures +=
        execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(128), marker INT)", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"tags\":[\"blue\",\"red\"],\"o\":{\"k\":2}}', '{\"a\":1}', 0), "
        "(2, '{\"a\":2,\"tags\":[\"green\"],\"o\":{}}', '{\"a\":2}', 0), "
        "(3, NULL, NULL, 0), "
        "(4, '{\"b\":1}', '[1,2]', 0)",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_CONTAINS(j, '1', '$.a'), "
                   "JSON_CONTAINS(j, '\"blue\"', '$.tags'), "
                   "JSON_CONTAINS(s, '{\"a\":1}'), "
                   "JSON_CONTAINS_PATH(j, 'one', '$.o.k') FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .context = "table json contains projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_CONTAINS(j, '\"blue\"', '$.tags') ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_blue_rows,
            .row_count = 1U,
            .context = "json_contains truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_CONTAINS_PATH(j, 'one', '$.o.k') ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_path_rows,
            .row_count = 1U,
            .context = "json_contains_path truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_CONTAINS(j, '1', '$.a') = 1 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_a_rows,
            .row_count = 1U,
            .context = "json_contains comparison predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_CONTAINS_PATH(j, 'one', '$.missing') = 0 "
                   "ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_missing_path_rows,
            .row_count = 3U,
            .context = "json_contains_path zero comparison predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_CONTAINS(j, '1', '$.a') IS NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_null_contains_rows,
            .row_count = 2U,
            .context = "json_contains null predicate",
        }
    );

    failures += expect_dml_ok(
        database,
        "UPDATE t SET marker = 9 WHERE JSON_CONTAINS(j, '\"blue\"', '$.tags')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "DELETE FROM t WHERE JSON_CONTAINS(s, '{\"a\":2}')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, marker, JSON_CONTAINS(j, '\"blue\"', '$.tags') FROM t ORDER BY id",
            .columns = columns_remaining,
            .column_count = sizeof(columns_remaining) / sizeof(columns_remaining[0]),
            .values = values_remaining,
            .row_count = 3U,
            .context = "json contains dml predicates",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json contains");
    if (failures == 0) {
        failures += execute_ok(database, "USE app", NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, marker, JSON_CONTAINS(j, '\"blue\"', '$.tags') FROM t ORDER BY id",
            .columns = columns_remaining,
            .column_count = sizeof(columns_remaining) / sizeof(columns_remaining[0]),
            .values = values_remaining,
            .row_count = 3U,
            .context = "reopen json contains dml predicates",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_json_contains_handles(void) {
    static const char *const columns[] = {"has_a", "has_b"};
    static const char *const values_left[] = {"1", "0"};
    static const char *const values_right[] = {NULL, "1"};
    char left_path[test_path_capacity] = {0};
    char right_path[test_path_capacity] = {0};
    mylite_db *left = NULL;
    mylite_db *right = NULL;
    int failures = 0;

    failures += open_app_database(&left, "independent-left", left_path, sizeof(left_path));
    failures += open_app_database(&right, "independent-right", right_path, sizeof(right_path));
    if (failures == 0) {
        failures += execute_ok(left, "CREATE TABLE t(id INT, j JSON)", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(right, "CREATE TABLE t(id INT, j JSON)", NULL);
    }
    if (failures == 0) {
        failures += expect_dml_ok(
            left,
            "INSERT INTO t VALUES (1, '{\"a\":1}')",
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
        );
    }
    if (failures == 0) {
        failures += expect_dml_ok(
            right,
            "INSERT INTO t VALUES (1, '{\"b\":1}')",
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
        );
    }
    if (failures == 0) {
        failures += expect_query(
            left,
            (struct expected_query){
                .sql = "SELECT JSON_CONTAINS(j, '1', '$.a') AS has_a, "
                       "JSON_CONTAINS_PATH(j, 'one', '$.b') AS has_b FROM t",
                .columns = columns,
                .column_count = sizeof(columns) / sizeof(columns[0]),
                .values = values_left,
                .row_count = 1U,
                .context = "left independent json contains handle",
            }
        );
    }
    if (failures == 0) {
        failures += expect_query(
            right,
            (struct expected_query){
                .sql = "SELECT JSON_CONTAINS(j, '1', '$.a') AS has_a, "
                       "JSON_CONTAINS_PATH(j, 'one', '$.b') AS has_b FROM t",
                .columns = columns,
                .column_count = sizeof(columns) / sizeof(columns[0]),
                .values = values_right,
                .row_count = 1U,
                .context = "right independent json contains handle",
            }
        );
    }

    if (left != NULL) {
        mylite_close(left);
    }
    if (right != NULL) {
        mylite_close(right);
    }
    if (left_path[0] != '\0') {
        remove_related_files(left_path);
    }
    if (right_path[0] != '\0') {
        remove_related_files(right_path);
    }
    return failures;
}

static int test_json_contains_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '{\"a\":1}')", NULL);
    failures += execute_ok(database, "CREATE TABLE invalid_doc(s VARCHAR(16))", NULL);
    failures += execute_ok(database, "INSERT INTO invalid_doc VALUES ('bad')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_CONTAINS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_CONTAINS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('{}','{}','$','$.a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_CONTAINS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_CONTAINS_PATH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH('{}','one')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_CONTAINS_PATH'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('bad','{}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('bad',NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('bad',1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS(s,NULL) FROM invalid_doc",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS(s,1) FROM invalid_doc",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('{}','bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS(1,'1')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('{}',1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('{}','{}','bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS('{\"a\":[1]}','1','$.a[*]')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_EXTRACT() path expression is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS(CAST('{\"a\":1}' AS BINARY),'{}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH('{\"a\":1}','some','$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_one_or_all,
            .sqlstate = "42000",
            .message_part = "The oneOrAll argument to json_contains_path",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH(j,'some','$.a') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_one_or_all,
            .sqlstate = "42000",
            .message_part = "The oneOrAll argument to json_contains_path",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH('bad','one','$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH('bad','some','bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH(s,'some','bad') FROM invalid_doc",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH('{\"a\":[1]}','one','$.a[*]')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_EXTRACT() path expression is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH(CAST('{\"a\":1}' AS BINARY),'one','$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH(1,'one','$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH('{\"a\":1}','one','bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS(missing, '1') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS_PATH(j, 'one', missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_EXTRACT() supports only string and NULL path literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_CONTAINS(j + 1, '1') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "JSON introspection supports only current JSON document producers and descriptor "
                "columns",
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
        "/tmp/mylite-json-contains-functions-%s-%d.mylite",
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
