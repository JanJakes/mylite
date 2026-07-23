#include "mylite_test_support.h"

#include <mylite/mylite.h>

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

static int test_no_source_dual_and_do_field(void);
static int test_table_backed_field(void);
static int test_field_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_no_source_dual_and_do_field();
    failures += test_table_backed_field();
    failures += test_field_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_field(void) {
    static const char *const columns_no_source[] = {
        "FIELD('b', 'a', 'b')",
        "FIELD('x', 'a', 'b')",
        "FIELD(NULL, 'a')",
        "FIELD('abc', 'ABC')",
        "FIELD(-1, 0, -1)",
        "FIELD(TRUE, FALSE, TRUE)",
        "@@warning_count",
    };
    static const char *const values_no_source[] = {"2", "0", "0", "1", "2", "2", "0"};
    static const char *const columns_dual[] = {"FIELD ('b', 'a', 'b')", "field_alias"};
    static const char *const values_dual[] = {"2", "2"};
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
            .sql = "SELECT FIELD('b', 'a', 'b'), FIELD('x', 'a', 'b'), FIELD(NULL, 'a'), "
                   "FIELD('abc', 'ABC'), FIELD(-1, 0, -1), FIELD(TRUE, FALSE, TRUE), "
                   "@@warning_count",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source field",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FIELD ('b', 'a', 'b'), FIELD('x', 'y', 'x') AS field_alias FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual field",
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
            .context = "row count after field select",
        }
    );

    failures += execute_ok(database, "DO FIELD('x', 'a', 'x'), FIELD(NULL, NULL)", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "field do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "field do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "field do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "field do warnings");
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
            .context = "row count after field do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_field(void) {
    static const char *const columns_table[] =
        {"id", "option_pos", "char_pos", "text_pos", "n_pos"};
    static const char *const values_table[] = {
        "1", "1", "1", "1", "2", "2", "2", "2", "2", "1",
        "3", "0", "0", "0", "0", "4", "3", "3", "1", "3",
    };
    static const char *const columns_limited[] = {"id", "pos"};
    static const char *const values_limited[] = {"4", "3", "3", "0"};
    static const char *const columns_where[] = {"FIELD(option_name, 'User 0000019')"};
    static const char *const values_where[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE options("
        "id INT, option_name VARCHAR(32), code CHAR(1), notes TEXT, n INT"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO options VALUES "
        "(1, 'User 0000018', 'A', 'alpha', 1), "
        "(2, 'User 0000019', 'B', 'beta', 2), "
        "(3, NULL, NULL, NULL, NULL), "
        "(4, 'User 0000020', 'C', 'ALPHA', -1)",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "FIELD(option_name, 'User 0000018', 'User 0000019', 'User 0000020') "
                   "AS option_pos, FIELD(code, 'a', 'b', 'c') AS char_pos, "
                   "FIELD(notes, 'alpha', 'beta') AS text_pos, "
                   "FIELD(n, 2, 1, -1) AS n_pos FROM options ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .context = "table field projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FIELD(option_name, 'User 0000018', 'User 0000019', "
                   "'User 0000020') AS pos FROM options WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table field where order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FIELD(option_name, 'User 0000019') FROM options WHERE id = 2",
            .columns = columns_where,
            .column_count = sizeof(columns_where) / sizeof(columns_where[0]),
            .values = values_where,
            .row_count = 1U,
            .context = "table field single candidate",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_field_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(20), n INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'a', 1)", NULL);
    failures += execute_error(
        database,
        "SELECT FIELD()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'FIELD'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD('x')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'FIELD'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD(v, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD(v, n) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIELD() does not support mixed string and numeric arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD('x', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIELD() does not support mixed string and numeric arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD('x', CONCAT('x')) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIELD() supports only string, integer, boolean, and NULL arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD(v + 1, 'a') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIELD() supports only string, integer, boolean, and NULL arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD(9223372036854775808, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT FIELD('\xC3\xA9', 'e')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FIELD() string literals support only ASCII values",
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
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

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
