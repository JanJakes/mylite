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

static int test_no_source_dual_and_do_json_valid(void);
static int test_table_backed_json_valid_and_reopen(void);
static int test_json_valid_diagnostics(void);
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

    failures += test_no_source_dual_and_do_json_valid();
    failures += test_table_backed_json_valid_and_reopen();
    failures += test_json_valid_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_json_valid(void) {
    static const char *const columns_no_source[] = {
        "JSON_VALID('{\"a\":1}')",
        "JSON_VALID('hello')",
        "JSON_VALID('\"hello\"')",
        "JSON_VALID(NULL)",
        "JSON_VALID('1.2')",
        "JSON_VALID('1e2')",
        "JSON_VALID('01')",
        "JSON_VALID(1)",
        "JSON_VALID(TRUE)",
        "@@warning_count",
    };
    static const char *const values_no_source[] =
        {"1", "0", "1", NULL, "1", "1", "0", "0", "0", "0"};
    static const char *const columns_dual[] = {"JSON_VALID('{\"a\":1}')", "ok"};
    static const char *const values_dual[] = {"1", "0"};
    static const char *const columns_binary[] = {
        "JSON_VALID(CAST('{\"a\":1}' AS BINARY))",
        "JSON_VALID(CONVERT('{\"a\":1}' USING BINARY))",
    };
    static const char *const values_binary[] = {"0", "0"};
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
            .sql = "SELECT JSON_VALID('{\"a\":1}'), JSON_VALID('hello'), "
                   "JSON_VALID('\"hello\"'), JSON_VALID(NULL), JSON_VALID('1.2'), "
                   "JSON_VALID('1e2'), JSON_VALID('01'), JSON_VALID(1), "
                   "JSON_VALID(TRUE), @@warning_count",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source json_valid",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_VALID('{\"a\":1}'), JSON_VALID('bad') AS ok FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual json_valid",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_VALID(CAST('{\"a\":1}' AS BINARY)), "
                   "JSON_VALID(CONVERT('{\"a\":1}' USING BINARY))",
            .columns = columns_binary,
            .column_count = sizeof(columns_binary) / sizeof(columns_binary[0]),
            .values = values_binary,
            .row_count = 1U,
            .context = "binary json_valid",
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
            .context = "row count after json_valid select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_VALID('{\"a\":1}'), JSON_VALID('bad'), JSON_VALID(NULL)",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "json_valid do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "json_valid do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "json_valid do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "json_valid do warnings");
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
            .context = "row count after json_valid do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_json_valid_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_VALID(j)",
        "JSON_VALID(s)",
        "JSON_VALID(i)",
        "JSON_VALID(b)",
    };
    static const char *const values_table[] = {
        "1", "1", "1",  "0", "0",  "2", NULL, "0", "0",  "0",
        "3", "1", NULL, "0", NULL, "4", "1",  "0", NULL, "0",
    };
    static const char *const columns_id[] = {"id"};
    static const char *const values_valid_string_rows[] = {"1"};
    static const char *const values_invalid_string_rows[] = {"2", "4"};
    static const char *const values_null_string_rows[] = {"3"};
    static const char *const columns_remaining[] = {"id", "i", "JSON_VALID(j)", "JSON_VALID(s)"};
    static const char *const values_remaining[] = {
        "1",
        "9",
        "1",
        "1",
        "3",
        "1",
        "1",
        NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, j JSON, s VARCHAR(64), i INT, b VARBINARY(64))",
        NULL
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1}', '{\"b\":2}', 7, X'7B2261223A317D'), "
        "(2, NULL, 'bad', 0, X'626164'), "
        "(3, 'true', NULL, 1, NULL), "
        "(4, '123', '[1,]', NULL, X'5B312C325D')",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_VALID(j), JSON_VALID(s), JSON_VALID(i), JSON_VALID(b) "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .context = "table json_valid values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_VALID(s) ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_valid_string_rows,
            .row_count = 1U,
            .context = "json_valid truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_VALID(s) = 0 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_invalid_string_rows,
            .row_count = 2U,
            .context = "json_valid comparison predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_VALID(s) IS NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_null_string_rows,
            .row_count = 1U,
            .context = "json_valid null predicate",
        }
    );

    failures += expect_dml_ok(
        database,
        "UPDATE t SET i = 9 WHERE JSON_VALID(s)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "DELETE FROM t WHERE JSON_VALID(s) = 0",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, JSON_VALID(j), JSON_VALID(s) FROM t ORDER BY id",
            .columns = columns_remaining,
            .column_count = sizeof(columns_remaining) / sizeof(columns_remaining[0]),
            .values = values_remaining,
            .row_count = 2U,
            .context = "json_valid dml predicates",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json_valid");
    if (failures == 0) {
        failures += execute_ok(database, "USE app", NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, JSON_VALID(j), JSON_VALID(s) FROM t ORDER BY id",
            .columns = columns_remaining,
            .column_count = sizeof(columns_remaining) / sizeof(columns_remaining[0]),
            .values = values_remaining,
            .row_count = 2U,
            .context = "reopen json_valid dml predicates",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_valid_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '{\"a\":1}')", NULL);
    failures += execute_error(
        database,
        "SELECT JSON_VALID()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_VALID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALID('{}', '{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_VALID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALID(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_VALID(v + 1) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "JSON_VALID() supports only string, integer, boolean, NULL, limited binary cast, "
                "and descriptor column arguments",
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
        "/tmp/mylite-json-valid-function-%s-%d.mylite",
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
