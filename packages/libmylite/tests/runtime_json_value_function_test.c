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
    size_t warning_count;
    const char *context;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static const char json_value_missing_object_member_warning[] =
    "Invalid JSON text in argument 1 to function json_value: \"Missing a name for object "
    "member.\" at position 1.";

static int test_no_source_dual_do_and_warning_json_value(void);
static int test_table_backed_json_value_and_reopen(void);
static int test_json_value_diagnostics(void);
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

    failures += test_no_source_dual_do_and_warning_json_value();
    failures += test_table_backed_json_value_and_reopen();
    failures += test_json_value_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_do_and_warning_json_value(void) {
    static const char *const columns_values[] = {
        "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.a')",
        "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.b')",
        "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.c')",
        "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.d')",
        "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.e[1]')",
        "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,\"e\":[10,20],\"o\":{\"k\":\"v\"}"
        "}', '$.o')",
        "JSON_VALUE('{\"a\":1}', '$.missing')",
        "JSON_VALUE(NULL, '$.a')",
    };
    static const char *const values_values[] = {
        "1",
        "x",
        NULL,
        "true",
        "20",
        "{\"k\": \"v\"}",
        NULL,
        NULL,
    };
    static const char *const columns_path[] = {
        "JSON_VALUE('{\"a\":1}', '$')",
        "JSON_VALUE('{\"a-b\":2}', '$.\"a-b\"')",
        "JSON_VALUE('[10,20,[30,40]]', '$[2][1]')",
        "JSON_VALUE('1', '$')",
    };
    static const char *const values_path[] = {"{\"a\": 1}", "2", "40", "1"};
    static const char *const columns_labels[] = {"JSON_VALUE('{\"a\":1}', '$.a')", "value"};
    static const char *const values_labels[] = {"1", "x"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_warning[] = {"-1", "1"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const columns_warning[] = {"JSON_VALUE('{bad}', '$.a')"};
    static const char *const values_warning[] = {NULL};
    static const char *const columns_show_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_show_warnings[] = {
        "Warning",
        "3141",
        json_value_missing_object_member_warning,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.a'), "
                   "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.b'), "
                   "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.c'), "
                   "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.d'), "
                   "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.e[1]'), "
                   "JSON_VALUE('{\"a\":1,\"b\":\"x\",\"c\":null,\"d\":true,"
                   "\"e\":[10,20],\"o\":{\"k\":\"v\"}}', '$.o'), "
                   "JSON_VALUE('{\"a\":1}', '$.missing'), JSON_VALUE(NULL, '$.a')",
            .columns = columns_values,
            .column_count = sizeof(columns_values) / sizeof(columns_values[0]),
            .values = values_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "literal json_value values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_VALUE('{\"a\":1}', '$'), "
                   "JSON_VALUE('{\"a-b\":2}', '$.\"a-b\"'), "
                   "JSON_VALUE('[10,20,[30,40]]', '$[2][1]'), JSON_VALUE('1', '$')",
            .columns = columns_path,
            .column_count = sizeof(columns_path) / sizeof(columns_path[0]),
            .values = values_path,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "simple json_value path forms",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_VALUE('{\"a\":1}', '$.a'), "
                   "JSON_VALUE('{\"value\":\"x\"}', '$.value') AS value FROM DUAL",
            .columns = columns_labels,
            .column_count = sizeof(columns_labels) / sizeof(columns_labels[0]),
            .values = values_labels,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "json_value labels",
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
            .warning_count = 0U,
            .context = "row count after json_value select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_VALUE('{bad}', '$.a')",
            .columns = columns_warning,
            .column_count = sizeof(columns_warning) / sizeof(columns_warning[0]),
            .values = values_warning,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid json_value document warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_show_warnings,
            .column_count = sizeof(columns_show_warnings) / sizeof(columns_show_warnings[0]),
            .values = values_show_warnings,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "invalid json_value warning details",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_warning,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "warning count after invalid json_value document",
        }
    );

    failures +=
        execute_ok(database, "DO JSON_VALUE('{\"a\":1}', '$.a'), JSON_VALUE(NULL, '$.a')", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "json_value do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "json_value do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "json_value do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "json_value do warnings"
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
            .warning_count = 0U,
            .context = "row count after json_value do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_json_value_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_VALUE(j, '$.a')",
        "JSON_VALUE(j, '$.b')",
        "JSON_VALUE(j, '$.c')",
        "JSON_VALUE(j, '$.o')",
        "JSON_VALUE(s, '$.a')",
        "JSON_VALUE(s, '$.b')",
        "JSON_VALUE(s, '$.missing')",
        "JSON_VALUE(s, '$.o')",
    };
    static const char *const values_table[] = {
        "1",  "1",
        "x",  NULL,
        NULL, "10",
        "y",  NULL,
        NULL, "2",
        NULL, "null",
        NULL, "{\"k\": \"v\"}",
        "20", NULL,
        NULL, "{\"k\": \"w\"}",
        "3",  NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL,
    };
    static const char *const columns_invalid_table[] = {"id", "JSON_VALUE(s, '$.a')"};
    static const char *const values_invalid_table[] = {"1", NULL, "2", "1", "3", NULL};
    static const char *const columns_show_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_show_warnings[] = {
        "Warning",
        "3141",
        json_value_missing_object_member_warning,
        "Warning",
        "3141",
        json_value_missing_object_member_warning,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(128))", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"b\":\"x\"}', '{\"a\":10,\"b\":\"y\"}'), "
        "(2, '{\"a\":null,\"b\":\"null\",\"o\":{\"k\":\"v\"}}', "
        "'{\"a\":20,\"b\":null,\"o\":{\"k\":\"w\"}}'), "
        "(3, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_VALUE(j, '$.a'), JSON_VALUE(j, '$.b'), "
                   "JSON_VALUE(j, '$.c'), JSON_VALUE(j, '$.o'), "
                   "JSON_VALUE(s, '$.a'), JSON_VALUE(s, '$.b'), "
                   "JSON_VALUE(s, '$.missing'), JSON_VALUE(s, '$.o') FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "table json_value values",
        }
    );
    failures += execute_ok(database, "CREATE TABLE invalid_rows(id INT, s VARCHAR(20))", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO invalid_rows VALUES (1,'{bad}'),(2,'{\"a\":1}'),(3,'{bad}')",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_VALUE(s, '$.a') FROM invalid_rows ORDER BY id",
            .columns = columns_invalid_table,
            .column_count = sizeof(columns_invalid_table) / sizeof(columns_invalid_table[0]),
            .values = values_invalid_table,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "table invalid json_value document warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_show_warnings,
            .column_count = sizeof(columns_show_warnings) / sizeof(columns_show_warnings[0]),
            .values = values_show_warnings,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table invalid json_value warning details",
        }
    );

    mylite_close(database);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json_value");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_VALUE(j, '$.a'), JSON_VALUE(j, '$.b'), "
                   "JSON_VALUE(j, '$.c'), JSON_VALUE(j, '$.o'), "
                   "JSON_VALUE(s, '$.a'), JSON_VALUE(s, '$.b'), "
                   "JSON_VALUE(s, '$.missing'), JSON_VALUE(s, '$.o') FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "reopen table json_value values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_value_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(20))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '{\"a\":1}', '{\"a\":1}')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_VALUE()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE('{\"a\":1}')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE('{\"a\":1}', '$[')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_path,
            .sqlstate = "42000",
            .message_part = "Invalid JSON path expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE(1, '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE(CAST('{\"a\":1}' AS BINARY), '$.a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE('{\"a\":1}', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON_VALUE() supports only string path literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE('{\"a\":\"x\"}', '$.a' RETURNING CHAR)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE(missing, '$.a') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALUE(t.s, '$.a') FROM t AS x",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 't.s' in 'field list'",
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
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
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
