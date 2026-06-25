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
    mysql_error_invalid_json_text = 3141,
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

static int test_literal_json_merge_values(void);
static int test_warning_and_statement_status(void);
static int test_table_backed_json_merge_values(void);
static int test_json_merge_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_literal_json_merge_values();
    failures += test_warning_and_statement_status();
    failures += test_table_backed_json_merge_values();
    failures += test_json_merge_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_literal_json_merge_values(void) {
    static const char *const columns[] = {
        "preserve_arrays",
        "preserve_duplicate_keys",
        "preserve_mixed",
        "preserve_nested_objects",
        "patch_array_replace",
        "patch_remove_key",
        "patch_nested_objects",
        "patch_json_null",
        "patch_null_document",
        "preserve_null_document",
    };
    static const char *const values[] = {
        "[1, 2, true, false]",
        "{\"a\": [1, 3, 5], \"b\": 2, \"c\": 4, \"d\": 6}",
        "[{\"a\": 1}, 2]",
        "{\"a\": {\"x\": 1, \"y\": 2}}",
        "[true, false]",
        "{\"a\": 1}",
        "{\"a\": {\"x\": 1, \"y\": 2}}",
        "null",
        NULL,
        NULL,
    };
    static const char *const columns_constructors[] = {
        "constructor_preserve",
        "constructor_patch",
    };
    static const char *const values_constructors[] = {
        "{\"a\": [1, 2]}",
        "{\"a\": 1, \"b\": 2}",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_MERGE_PRESERVE('[1,2]', '[true,false]') AS preserve_arrays, "
                   "JSON_MERGE_PRESERVE('{\"a\":1,\"b\":2}', '{\"a\":3,\"c\":4}', "
                   "'{\"a\":5,\"d\":6}') AS preserve_duplicate_keys, "
                   "JSON_MERGE_PRESERVE('{\"a\":1}', '2') AS preserve_mixed, "
                   "JSON_MERGE_PRESERVE('{\"a\":{\"x\":1}}', '{\"a\":{\"y\":2}}') "
                   "AS preserve_nested_objects, "
                   "JSON_MERGE_PATCH('[1,2]', '[true,false]') AS patch_array_replace, "
                   "JSON_MERGE_PATCH('{\"a\":1,\"b\":2}', '{\"b\":null}') AS patch_remove_key, "
                   "JSON_MERGE_PATCH('{\"a\":{\"x\":1}}', '{\"a\":{\"y\":2}}') "
                   "AS patch_nested_objects, "
                   "JSON_MERGE_PATCH('{\"a\":1}', 'null') AS patch_json_null, "
                   "JSON_MERGE_PATCH(NULL, '{\"a\":1}') AS patch_null_document, "
                   "JSON_MERGE_PRESERVE(NULL, '{bad}') AS preserve_null_document",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "literal JSON merge values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_MERGE_PRESERVE(JSON_OBJECT('a', 1), JSON_OBJECT('a', 2)) "
                   "AS constructor_preserve, "
                   "JSON_MERGE_PATCH(JSON_OBJECT('a', 1), JSON_OBJECT('b', 2)) "
                   "AS constructor_patch",
            .columns = columns_constructors,
            .column_count = sizeof(columns_constructors) / sizeof(columns_constructors[0]),
            .values = values_constructors,
            .row_count = 1U,
            .context = "JSON merge constructor document arguments",
        }
    );

    failures += execute_ok(
        database,
        "SELECT JSON_MERGE_PATCH('{\"a\":1}', '{\"b\":2}') AS patched",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 1U, "json_merge metadata");
        failures += expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_JSON,
            "json_merge metadata type"
        );
        failures += expect_int(mylite_result_column_nullable(result, 0U), 1, "json_merge nullable");
        failures += expect_result_value(result, 0U, 0U, "{\"a\": 1, \"b\": 2}", "json_merge value");
    }
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_warning_and_statement_status(void) {
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_values[] = {
        "Warning",
        "1287",
        ("'JSON_MERGE' is deprecated and will be removed in a future release. Please use "
         "JSON_MERGE_PRESERVE/JSON_MERGE_PATCH instead"),
    };
    static const char *const status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "1"};
    static const char *const values_after_dual[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "1"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += execute_ok(database, "SELECT JSON_MERGE('1', 'true') AS merged", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_warning_count(result), 1U, "json_merge warning");
        failures += expect_result_value(result, 0U, 0U, "[1, true]", "json_merge synonym value");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 1U,
            .context = "JSON_MERGE deprecation warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "status after JSON_MERGE select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_MERGE_PATCH('{\"a\":1}', '{\"b\":2}') AS patched FROM DUAL",
            .columns = (const char *const[]){"patched"},
            .column_count = 1U,
            .values = (const char *const[]){"{\"a\": 1, \"b\": 2}"},
            .row_count = 1U,
            .context = "JSON_MERGE_PATCH from DUAL",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = values_after_dual,
            .row_count = 1U,
            .context = "status after JSON_MERGE_PATCH dual select",
        }
    );
    failures += execute_ok(database, "DO JSON_MERGE('1', 'true')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "JSON_MERGE do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "JSON_MERGE do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "JSON_MERGE do affected");
        failures += expect_size(mylite_result_warning_count(result), 1U, "JSON_MERGE do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "status after JSON_MERGE do",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_table_backed_json_merge_values(void) {
    static const char *const columns_table[] = {
        "id",
        "preserved",
        "patched",
        "patch_b",
    };
    static const char *const values_table[] = {
        "1",
        "{\"a\": 1, \"b\": [2, 3], \"c\": 4}",
        "{\"a\": 1, \"b\": 3, \"c\": 4}",
        "3",
        "2",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_short_circuit[] = {"id", "preserve_short", "merge_short"};
    static const char *const values_short_circuit[] = {
        "1",
        "{\"a\": 1, \"b\": 2, \"k\": 1}",
        "{\"a\": 1, \"b\": 2, \"k\": 1}",
        "2",
        NULL,
        NULL,
    };
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_values[] = {
        "Warning",
        "1287",
        ("'JSON_MERGE' is deprecated and will be removed in a future release. Please use "
         "JSON_MERGE_PRESERVE/JSON_MERGE_PATCH instead"),
    };
    static const char *const columns_updated[] = {"id", "j"};
    static const char *const values_updated[] = {
        "1",
        "{\"a\": 1, \"b\": 2, \"d\": 4}",
        "2",
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_open(path, &database);
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, j JSON, patch JSON, doc_text LONGTEXT, b VARBINARY(10), key_name VARCHAR(10))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"b\":2}', '{\"b\":3,\"c\":4}', '{\"d\":4}', x'6162', 'k'), "
        "(2, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_MERGE_PRESERVE(j, patch) AS preserved, "
                   "JSON_MERGE_PATCH(j, patch) AS patched, "
                   "JSON_EXTRACT(JSON_MERGE_PATCH(j, patch), '$.b') AS patch_b "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table JSON merge projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_MERGE_PRESERVE(j, JSON_OBJECT(key_name, 1)) "
                   "AS preserve_short, "
                   "JSON_MERGE(j, JSON_OBJECT(key_name, 1)) AS merge_short "
                   "FROM t ORDER BY id",
            .columns = columns_short_circuit,
            .column_count = sizeof(columns_short_circuit) / sizeof(columns_short_circuit[0]),
            .values = values_short_circuit,
            .row_count = 2U,
            .context = "table JSON merge first NULL short-circuit",
        }
    );
    failures +=
        execute_ok(database, "UPDATE t SET j = JSON_MERGE_PATCH(j, doc_text) WHERE id = 1", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, j FROM t ORDER BY id",
            .columns = columns_updated,
            .column_count = sizeof(columns_updated) / sizeof(columns_updated[0]),
            .values = values_updated,
            .row_count = 2U,
            .context = "JSON merge update assignment",
        }
    );
    failures +=
        execute_ok(database, "UPDATE t SET j = JSON_MERGE(j, doc_text) WHERE id = 1", &result);
    if (failures == 0) {
        failures +=
            expect_int64(mylite_result_affected_rows(result), 1, "JSON_MERGE update affected rows");
        failures +=
            expect_size(mylite_result_warning_count(result), 1U, "JSON_MERGE update warning count");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 1U,
            .context = "JSON_MERGE update deprecation warning",
        }
    );
    failures += execute_ok(database, "UPDATE t SET doc_text = '{bad}' WHERE id = 1", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_MERGE(j, doc_text) FROM t WHERE id = 1",
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

static int test_json_merge_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_open_memory(&database);
    failures += execute_error(
        database,
        "SELECT JSON_MERGE_PATCH()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_MERGE_PATCH('{\"a\":1}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_MERGE_PATCH('{bad}', '{\"a\":1}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_MERGE_PATCH(NULL, '{bad}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_MERGE_PRESERVE('{bad}', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_MERGE_PATCH(1, '{\"a\":1}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_MERGE_PRESERVE(CAST('{\"a\":1}' AS BINARY), '{\"b\":2}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );

    mylite_close(database);
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += expect_text(
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_json_merge_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
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

    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
