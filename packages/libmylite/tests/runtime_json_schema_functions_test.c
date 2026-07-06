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
    mysql_error_invalid_json_data = 3146,
    mysql_error_invalid_json_type = 3853,
    mysql_error_not_supported_yet = 1235,
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

static int test_json_schema_valid_values(void);
static int test_json_schema_validation_reports(void);
static int test_json_schema_row_backed_values(void);
static int test_json_schema_nulls_user_variables_and_do(void);
static int test_json_schema_diagnostics(void);
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

    failures += test_json_schema_valid_values();
    failures += test_json_schema_validation_reports();
    failures += test_json_schema_row_backed_values();
    failures += test_json_schema_nulls_user_variables_and_do();
    failures += test_json_schema_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_json_schema_valid_values(void) {
    static const char *const columns[] = {
        "ok_object",
        "bad_object",
        "type_array",
        "empty_schema",
        "required_ok",
        "required_missing",
        "property_type",
        "minimum_ok",
        "maximum_bad",
        "annotation_ok",
    };
    static const char *const values[] = {"1", "0", "1", "1", "1", "0", "0", "1", "0", "1"};
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open values database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "JSON_SCHEMA_VALID('{\"type\":\"object\"}', '{\"a\":1}') AS ok_object, "
                   "JSON_SCHEMA_VALID('{\"type\":\"object\"}', '[1]') AS bad_object, "
                   "JSON_SCHEMA_VALID('{\"type\":[\"object\",\"null\"]}', 'null') AS "
                   "type_array, "
                   "JSON_SCHEMA_VALID('{}', '123') AS empty_schema, "
                   "JSON_SCHEMA_VALID('{\"type\":\"object\",\"required\":[\"id\"],"
                   "\"properties\":{\"id\":{\"type\":\"integer\"},\"name\":{\"type\":"
                   "\"string\"}}}', '{\"id\":7,\"name\":\"Ada\"}') AS required_ok, "
                   "JSON_SCHEMA_VALID('{\"type\":\"object\",\"required\":[\"id\"]}', "
                   "'{\"name\":\"Ada\"}') AS required_missing, "
                   "JSON_SCHEMA_VALID('{\"properties\":{\"id\":{\"type\":\"integer\"}}}', "
                   "'{\"id\":\"7\"}') AS property_type, "
                   "JSON_SCHEMA_VALID('{\"minimum\":2}', '3') AS minimum_ok, "
                   "JSON_SCHEMA_VALID('{\"maximum\":2}', '3') AS maximum_bad, "
                   "JSON_SCHEMA_VALID('{\"id\":\"x\",\"$schema\":\"draft\",\"description\":"
                   "\"d\",\"type\":\"string\"}', '\"x\"') AS annotation_ok",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "json schema valid values",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_json_schema_validation_reports(void) {
    static const char *const columns[] = {"valid_report", "type_report", "required_report"};
    static const char *const values[] = {
        "{\"valid\": true}",
        "{\"valid\": false, \"reason\": \"The JSON document location '#' failed requirement "
        "'type' at JSON Schema location '#'\", \"schema-location\": \"#\", "
        "\"document-location\": \"#\", \"schema-failed-keyword\": \"type\"}",
        "{\"valid\": false, \"reason\": \"The JSON document location '#' failed requirement "
        "'required' at JSON Schema location '#'\", \"schema-location\": \"#\", "
        "\"document-location\": \"#\", \"schema-failed-keyword\": \"required\"}",
    };
    static const char *const nested_columns[] = {"maximum_report"};
    static const char *const nested_values[] = {
        "{\"valid\": false, \"reason\": \"The JSON document location '#/score' failed "
        "requirement 'maximum' at JSON Schema location '#/properties/score'\", "
        "\"schema-location\": \"#/properties/score\", \"document-location\": \"#/score\", "
        "\"schema-failed-keyword\": \"maximum\"}",
    };
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open report database");

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "JSON_SCHEMA_VALIDATION_REPORT('{\"type\":\"object\"}', '{\"a\":1}') AS "
                   "valid_report, "
                   "JSON_SCHEMA_VALIDATION_REPORT('{\"type\":\"object\"}', '[1]') AS "
                   "type_report, "
                   "JSON_SCHEMA_VALIDATION_REPORT('{\"type\":\"object\",\"required\":[\"id\"]}', "
                   "'{}') AS required_report",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "json schema validation reports",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_SCHEMA_VALIDATION_REPORT('{\"properties\":{\"score\":"
                   "{\"maximum\":5}}}', '{\"score\":7}') AS maximum_report",
            .columns = nested_columns,
            .column_count = sizeof(nested_columns) / sizeof(nested_columns[0]),
            .values = nested_values,
            .row_count = 1U,
            .context = "json schema nested report",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_json_schema_row_backed_values(void) {
    static const char *const columns[] = {"id", "ok", "report"};
    static const char invalid_report[] =
        "{\"valid\": false, \"reason\": \"The JSON document location '#/score' failed "
        "requirement 'maximum' at JSON Schema location '#/properties/score'\", "
        "\"schema-location\": \"#/properties/score\", \"document-location\": \"#/score\", "
        "\"schema-failed-keyword\": \"maximum\"}";
    static const char *const values[] = {
        "1",
        "1",
        "{\"valid\": true}",
        "2",
        "0",
        invalid_report,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rows database");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE js_rows (id INT PRIMARY KEY, doc JSON, schema_text TEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO js_rows VALUES "
        "(1, '{\"id\":1,\"score\":7}', '{\"type\":\"object\",\"required\":[\"id\"]}'), "
        "(2, '{\"score\":11}', '{\"type\":\"object\",\"required\":[\"id\"]}')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_SCHEMA_VALID(schema_text, doc) AS ok, "
                   "JSON_SCHEMA_VALIDATION_REPORT('{\"properties\":{\"score\":{\"maximum\":"
                   "10}}}', doc) AS report FROM js_rows ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 2U,
            .context = "json schema row-backed values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_schema_nulls_user_variables_and_do(void) {
    static const char *const columns[] = {"ok", "bad", "null_schema", "null_doc"};
    static const char *const values[] = {"1", "0", NULL, NULL};
    static const char *const status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const status_values[] = {"0", "0"};
    mylite_result *result = NULL;
    mylite_db *database = NULL;
    int failures = expect_int(mylite_open_memory(&database), MYLITE_OK, "open variables database");

    failures += execute_ok(
        database,
        "SET @schema = '{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}}}', "
        "@doc = '{\"id\":7}', @bad_doc = '{\"id\":\"7\"}'",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_SCHEMA_VALID(@schema, @doc) AS ok, "
                   "JSON_SCHEMA_VALID(@schema, @bad_doc) AS bad, "
                   "JSON_SCHEMA_VALID(NULL, '{bad}') AS null_schema, "
                   "JSON_SCHEMA_VALID('{}', NULL) AS null_doc",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "json schema user variables and nulls",
        }
    );
    failures += execute_ok(
        database,
        "DO JSON_SCHEMA_VALID(@schema, @doc), JSON_SCHEMA_VALIDATION_REPORT(@schema, @bad_doc)",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "json schema do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "json schema do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "json schema do rows");
        failures += expect_size(mylite_result_warning_count(result), 0U, "json schema do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = status_values,
            .row_count = 1U,
            .context = "json schema do status",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_json_schema_diagnostics(void) {
    mylite_db *database = NULL;
    int failures =
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics database");

    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALIDATION_REPORT('{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID('{bad}', '{}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text in argument 1",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID('{}', '{bad}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text in argument 2",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID('[]', '{}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_type,
            .sqlstate = "22032",
            .message_part = "an object is required",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID(1, '{}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "argument 1",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID('{}', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "argument 2",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID('{\"$ref\":\"x\"}', '{}')",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "references in JSON Schema",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_SCHEMA_VALID('{\"pattern\":\"x\"}', '\"x\"')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON schema validation supports",
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "runtime-json-schema-functions-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? -1 : 0;
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
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
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
