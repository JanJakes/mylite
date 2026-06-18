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

static int test_json_row_scalar_order_contexts(void);
static int test_json_row_scalar_update_contexts(void);
static int test_json_row_scalar_update_does_not_widen_joined_update(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_json_context_database(
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

    failures += test_json_row_scalar_order_contexts();
    failures += test_json_row_scalar_update_contexts();
    failures += test_json_row_scalar_update_does_not_widen_joined_update();

    return failures == 0 ? 0 : 1;
}

static int test_json_row_scalar_order_contexts(void) {
    static const char *const columns[] = {"id", "v"};
    static const char *const values_extract[] = {"3", NULL, "2", "1", "1", "2"};
    static const char *const values_unquote[] = {"3", NULL, "1", "x", "2", "y"};
    static const char *const values_type[] = {"3", NULL, "1", "OBJECT", "2", "OBJECT"};
    static const char *const values_length[] = {"3", NULL, "1", "3", "2", "3"};
    static const char *const values_quote[] = {"3", NULL, "1", "\"plain\"", "2", "\"quoted\""};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_json_context_database(&database, "order", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_EXTRACT(j, '$.a') AS v FROM t "
                   "ORDER BY JSON_EXTRACT(j, '$.a'), id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_extract,
            .row_count = 3U,
            .context = "json_extract order key",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')) AS v FROM t "
                   "ORDER BY JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')), id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_unquote,
            .row_count = 3U,
            .context = "json_unquote order key",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_TYPE(j) AS v FROM t ORDER BY JSON_TYPE(j), id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_type,
            .row_count = 3U,
            .context = "json_type order key",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_LENGTH(j) AS v FROM t ORDER BY JSON_LENGTH(j), id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_length,
            .row_count = 3U,
            .context = "json_length order key",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_QUOTE(s) AS v FROM t ORDER BY JSON_QUOTE(s), id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values_quote,
            .row_count = 3U,
            .context = "json_quote order key",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_row_scalar_update_contexts(void) {
    static const char *const columns_text[] = {"id", "out_text", "out_int", "out_json"};
    static const char *const values_text[] = {
        "1",
        "x",
        "3",
        NULL,
        "2",
        "OBJECT",
        "1",
        "[\"a\", \"b\", \"obj\"]",
        "3",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_json[] = {"id", "out_json"};
    static const char *const values_json_first[] = {
        "1",
        "{\"a\": 2, \"b\": \"x\", \"n\": 7, \"obj\": {\"k\": 1}}",
        "2",
        "[3, \"quoted\", {\"a\": 1, \"b\": \"y\", \"obj\": {\"m\": 2}}]",
        "3",
        "{\"s\": null, \"id\": 3}",
    };
    static const char *const values_json_second[] = {
        "1",
        "{\"a\": 2, \"b\": \"x\", \"obj\": {\"k\": 1}, \"inserted\": 7}",
        "2",
        "{\"a\": 3, \"b\": \"y\", \"obj\": {\"m\": 2}}",
        "3",
        NULL,
    };
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_status[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_json_context_database(&database, "update", path, sizeof(path));
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_text = JSON_UNQUOTE(JSON_EXTRACT(j, '$.b')) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_text = JSON_QUOTE(s) WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_int = JSON_LENGTH(j) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_json = JSON_KEYS(j) WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_text = JSON_TYPE(j) WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_int = JSON_EXTRACT(j, '$.a') WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, out_text, out_int, out_json FROM t ORDER BY id",
            .columns = columns_text,
            .column_count = sizeof(columns_text) / sizeof(columns_text[0]),
            .values = values_text,
            .row_count = 3U,
            .context = "json text and introspection update assignments",
        }
    );

    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_json = JSON_SET(j, '$.n', n) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_json = JSON_ARRAY(n, s, j) WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_json = JSON_OBJECT('id', id, 's', s) WHERE id = 3",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, out_json FROM t ORDER BY id",
            .columns = columns_json,
            .column_count = sizeof(columns_json) / sizeof(columns_json[0]),
            .values = values_json_first,
            .row_count = 3U,
            .context = "json construction update assignments",
        }
    );

    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_json = JSON_INSERT(j, '$.inserted', n) WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_json = JSON_REPLACE(j, '$.a', n) WHERE id = 2",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_json = JSON_REMOVE(j, '$.b') WHERE id = 3",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, out_json FROM t ORDER BY id",
            .columns = columns_json,
            .column_count = sizeof(columns_json) / sizeof(columns_json[0]),
            .values = values_json_second,
            .row_count = 3U,
            .context = "json mutation update assignments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_status,
            .row_count = 1U,
            .context = "json row-scalar status after select",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_row_scalar_update_does_not_widen_joined_update(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_json_context_database(&database, "joined", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE joined_source(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO joined_source VALUES (1)", NULL);
    failures += execute_error(
        database,
        "UPDATE t JOIN joined_source ON t.id = joined_source.id "
        "SET t.out_text = JSON_QUOTE(t.s)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "joined UPDATE supports only constant assignment values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *local = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &local);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got %d: %s\n",
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(local);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = local;
    } else {
        mylite_result_free(local);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
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

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "dml column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "dml row count");
        failures += expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            "dml affected rows"
        );
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            "dml warnings"
        );
    }
    mylite_result_free(result);
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
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
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

static int open_json_context_database(
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
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open database");
    failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    failures += execute_ok(*out_database, "USE app", NULL);
    failures += expect_dml_ok(
        *out_database,
        "CREATE TABLE t("
        "id INT, j JSON, s VARCHAR(64), n INT, b BOOLEAN, "
        "out_text VARCHAR(255), out_json JSON, out_int INT)",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        *out_database,
        "INSERT INTO t(id, j, s, n, b, out_text, out_json, out_int) VALUES "
        "(1, '{\"a\":2,\"b\":\"x\",\"obj\":{\"k\":1}}', 'plain', 7, TRUE, NULL, NULL, "
        "NULL), "
        "(2, '{\"a\":1,\"b\":\"y\",\"obj\":{\"m\":2}}', 'quoted', 3, FALSE, NULL, NULL, "
        "NULL), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-runtime-json-row-scalar-contexts-%s-%d.mylite",
        name,
        current_process_id()
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(buffer)) {
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s row %zu column %zu: expected NULL, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
