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

static int test_no_source_dual_and_do_json_overlaps_member(void);
static int test_table_backed_json_overlaps_member_and_reopen(void);
static int test_json_overlaps_member_diagnostics(void);
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

    failures += test_no_source_dual_and_do_json_overlaps_member();
    failures += test_table_backed_json_overlaps_member_and_reopen();
    failures += test_json_overlaps_member_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_json_overlaps_member(void) {
    static const char *const columns_overlaps[] = {
        "same_scalar",
        "different_scalar",
        "array_hit",
        "array_scalar_hit",
        "object_hit",
        "object_miss",
        "nested_miss",
        "null_left",
        "null_right",
    };
    static const char *const values_overlaps[] = {"1", "0", "1", "1", "1", "0", "0", NULL, NULL};
    static const char *const columns_member[] = {
        "int_hit",
        "string_hit",
        "string_number_miss",
        "array_hit",
        "object_hit",
        "null_left",
        "null_right",
    };
    static const char *const values_member[] = {"1", "1", "0", "1", "1", NULL, NULL};
    static const char *const columns_dual[] = {"overlap_hit", "member_hit"};
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
            .sql = "SELECT JSON_OVERLAPS('1','1') AS same_scalar, "
                   "JSON_OVERLAPS('1','2') AS different_scalar, "
                   "JSON_OVERLAPS('[1,2]','[2,3]') AS array_hit, "
                   "JSON_OVERLAPS('[1]', '1') AS array_scalar_hit, "
                   "JSON_OVERLAPS('{\"a\":1}', '{\"a\":1,\"b\":2}') AS object_hit, "
                   "JSON_OVERLAPS('{\"a\":1}', '{\"a\":2}') AS object_miss, "
                   "JSON_OVERLAPS('{\"a\":{\"b\":2}}', '{\"a\":{\"b\":2,\"c\":3}}') "
                   "AS nested_miss, "
                   "JSON_OVERLAPS(NULL, 'bad') AS null_left, "
                   "JSON_OVERLAPS('1', NULL) AS null_right",
            .columns = columns_overlaps,
            .column_count = sizeof(columns_overlaps) / sizeof(columns_overlaps[0]),
            .values = values_overlaps,
            .row_count = 1U,
            .context = "literal json_overlaps values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 17 MEMBER OF('[23, \"abc\", 17]') AS int_hit, "
                   "'ab' MEMBER OF('[23, \"abc\", 17, \"ab\"]') AS string_hit, "
                   "17 MEMBER OF('[\"17\"]') AS string_number_miss, "
                   "JSON_ARRAY(1) MEMBER OF('[[1]]') AS array_hit, "
                   "JSON_OBJECT('a',1) MEMBER OF('[{\"a\":1}]') AS object_hit, "
                   "NULL MEMBER OF('bad') AS null_left, "
                   "1 MEMBER OF(NULL) AS null_right",
            .columns = columns_member,
            .column_count = sizeof(columns_member) / sizeof(columns_member[0]),
            .values = values_member,
            .row_count = 1U,
            .context = "literal member of values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT JSON_OVERLAPS('{\"a\":1}', '{\"a\":1}') AS overlap_hit, "
                   "'blue' MEMBER OF('[\"blue\", \"red\"]') AS member_hit FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "json overlaps member from dual",
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
            .context = "row count after json overlaps member select",
        }
    );

    failures += execute_ok(
        database,
        "DO JSON_OVERLAPS('{\"a\":1}', '{\"a\":1}'), 1 MEMBER OF('[1,2]')",
        &result
    );
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 0U, "json overlaps member do columns");
        failures +=
            expect_size(mylite_result_row_count(result), 0U, "json overlaps member do rows");
        failures += expect_int64(
            mylite_result_affected_rows(result),
            0,
            "json overlaps member do affected"
        );
        failures += expect_size(
            mylite_result_warning_count(result),
            0U,
            "json overlaps member do warnings"
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
            .context = "row count after json overlaps member do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_json_overlaps_member_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "JSON_OVERLAPS(j, '{\"a\":1}')",
        "JSON_OVERLAPS(JSON_EXTRACT(j, '$.tags'), '[\"blue\"]')",
        "s MEMBER OF('[\"blue\",\"red\"]')",
        "id MEMBER OF('[1,2]')",
    };
    static const char *const values_table[] = {
        "1", "1",  "1",  "1",  "1", "2", "0", "0",  "0", "1",
        "3", NULL, NULL, NULL, "0", "4", "0", NULL, "1", "0",
    };
    static const char *const columns_id[] = {"id"};
    static const char *const values_blue_rows[] = {"1"};
    static const char *const values_member_rows[] = {"1", "4"};
    static const char *const values_missing_miss_rows[] = {"1", "2"};
    static const char *const values_null_rows[] = {"3"};
    static const char *const columns_remaining[] = {
        "id",
        "marker",
        "s MEMBER OF('[\"blue\",\"red\"]')",
    };
    static const char *const values_remaining[] = {"1", "9", "1", "3", "0", NULL, "4", "9", "1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures +=
        execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(128), marker INT)", NULL);
    failures += expect_dml_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '{\"a\":1,\"tags\":[\"blue\",\"red\"],\"o\":{\"k\":2}}', 'blue', 0), "
        "(2, '{\"a\":2,\"tags\":[\"green\"],\"o\":{}}', 'green', 0), "
        "(3, NULL, NULL, 0), "
        "(4, '{\"missing\":1}', 'blue', 0)",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 0U}
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, JSON_OVERLAPS(j, '{\"a\":1}'), "
                   "JSON_OVERLAPS(JSON_EXTRACT(j, '$.tags'), '[\"blue\"]'), "
                   "s MEMBER OF('[\"blue\",\"red\"]'), id MEMBER OF('[1,2]') "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .context = "table json overlaps member projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_OVERLAPS(JSON_EXTRACT(j, '$.tags'), "
                   "'[\"blue\"]') ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_blue_rows,
            .row_count = 1U,
            .context = "json_overlaps truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE s MEMBER OF('[\"blue\",\"red\"]') ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_member_rows,
            .row_count = 2U,
            .context = "member of truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_OVERLAPS(j, '{\"missing\":1}') = 0 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_missing_miss_rows,
            .row_count = 2U,
            .context = "json_overlaps zero comparison predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE JSON_OVERLAPS(j, '{\"a\":1}') IS NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_null_rows,
            .row_count = 1U,
            .context = "json_overlaps null predicate",
        }
    );

    failures += expect_dml_ok(
        database,
        "UPDATE t SET marker = 9 WHERE s MEMBER OF('[\"blue\",\"red\"]')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "DELETE FROM t WHERE JSON_OVERLAPS(j, '{\"a\":2}')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, marker, s MEMBER OF('[\"blue\",\"red\"]') FROM t ORDER BY id",
            .columns = columns_remaining,
            .column_count = sizeof(columns_remaining) / sizeof(columns_remaining[0]),
            .values = values_remaining,
            .row_count = 3U,
            .context = "json overlaps member dml predicates",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen json overlaps member");
    if (failures == 0) {
        failures += execute_ok(database, "USE app", NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, marker, s MEMBER OF('[\"blue\",\"red\"]') FROM t ORDER BY id",
            .columns = columns_remaining,
            .column_count = sizeof(columns_remaining) / sizeof(columns_remaining[0]),
            .values = values_remaining,
            .row_count = 3U,
            .context = "reopen json overlaps member dml predicates",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_json_overlaps_member_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, j JSON, s VARCHAR(16))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '{\"a\":1}', 'blue')", NULL);
    failures += execute_ok(database, "CREATE TABLE invalid_doc(s VARCHAR(16))", NULL);
    failures += execute_ok(database, "INSERT INTO invalid_doc VALUES ('bad')", NULL);

    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_OVERLAPS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS('{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_OVERLAPS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS('{}','{}','{}')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'JSON_OVERLAPS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS('bad','{}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS('{}','bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS('bad', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS('bad', 1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS(1,'1')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS('{}',1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS(CAST('{\"a\":1}' AS BINARY),'{}')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS(s, NULL) FROM invalid_doc",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 MEMBER OF(1)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_data,
            .sqlstate = "22032",
            .message_part = "Invalid data type for JSON data",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 MEMBER OF('bad')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_text,
            .sqlstate = "22032",
            .message_part = "Invalid JSON text",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 MEMBER OF(CAST('[1]' AS BINARY))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_json_charset,
            .sqlstate = "22032",
            .message_part = "Cannot create a JSON value from a string with CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 MEMBER OF()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT JSON_OVERLAPS(missing, '1') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT (id + 1) MEMBER OF('[1]') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "JSON constructors support only string, integer, boolean, NULL, "
                            "and descriptor column arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1 MEMBER OF(s) FROM invalid_doc",
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
        mylite_result_free(result);
        return failures;
    }
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t index = (row * expected.column_count) + column;

            failures +=
                expect_result_value(result, row, column, expected.values[index], expected.context);
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
    int failures = make_test_path(path, path_size, name);

    if (failures == 0) {
        remove_related_files(path);
        failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open database");
    }
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
        "/tmp/mylite-json-overlaps-member-%s-%d.mylite",
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + path_suffix_capacity];
    int written = 0;

    if (path == NULL || suffix == NULL) {
        return;
    }
    written = snprintf(related, sizeof(related), "%s%s", path, suffix);
    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
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
                "%s: row %zu column %zu expected NULL, got '%s'\n",
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }
    return 0;
}
