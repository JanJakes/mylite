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

static int test_no_source_dual_and_do_elt(void);
static int test_table_backed_elt(void);
static int test_elt_diagnostics(void);
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

    failures += test_no_source_dual_and_do_elt();
    failures += test_table_backed_elt();
    failures += test_elt_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_elt(void) {
    static const char *const columns_no_source[] = {
        "elt_one",
        "elt_three",
        "elt_zero",
        "elt_negative",
        "elt_beyond",
        "elt_null_index",
        "elt_true_index",
        "elt_false_index",
        "elt_int",
        "elt_true",
        "elt_null",
        "warnings",
    };
    static const char *const values_no_source[] = {
        "Aa",
        "Cc",
        NULL,
        NULL,
        NULL,
        NULL,
        "no",
        NULL,
        "10",
        "1",
        NULL,
        "0",
    };
    static const char *const columns_dual[] = {"elt_alias", "plus_index"};
    static const char *const values_dual[] = {"second", "x"};
    static const char *const columns_expression[] = {
        "index_expr",
        "value_expr",
        "unicode_value",
    };
    static const char *const values_expression[] = {"a", "1", "\xC3\xA9"};
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
            .sql = "SELECT ELT(1,'Aa','Bb','Cc') AS elt_one, "
                   "ELT(3,'Aa','Bb','Cc') AS elt_three, ELT(0,'Aa') AS elt_zero, "
                   "ELT(-1,'Aa') AS elt_negative, ELT(4,'Aa','Bb') AS elt_beyond, "
                   "ELT(NULL,'Aa') AS elt_null_index, ELT(TRUE,'no','yes') AS elt_true_index, "
                   "ELT(FALSE,'zero') AS elt_false_index, ELT(1, 10, TRUE, NULL) AS elt_int, "
                   "ELT(2, 10, TRUE, NULL) AS elt_true, "
                   "ELT(3, 10, TRUE, NULL) AS elt_null, @@warning_count AS warnings",
            .columns = columns_no_source,
            .column_count = sizeof(columns_no_source) / sizeof(columns_no_source[0]),
            .values = values_no_source,
            .row_count = 1U,
            .context = "no-source elt",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ELT (2,'first','second') AS elt_alias, "
                   "ELT(+1,'x') AS plus_index FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual elt",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ELT(1 + 0, 'a') AS index_expr, "
                   "ELT(1, 1 + 0) AS value_expr, "
                   "ELT(1, '\xC3\xA9') AS unicode_value",
            .columns = columns_expression,
            .column_count = sizeof(columns_expression) / sizeof(columns_expression[0]),
            .values = values_expression,
            .row_count = 1U,
            .context = "expression elt",
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
            .context = "row count after elt select",
        }
    );

    failures += execute_ok(database, "DO ELT(2,'a','b'), ELT(NULL,'a')", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "elt do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "elt do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "elt do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "elt do warnings");
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
            .context = "row count after elt do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_elt(void) {
    static const char *const columns_table[] = {
        "id",
        "selected",
        "expr_index",
        "len_index",
        "nested_value",
    };
    static const char *const values_table[] = {
        "1", "alpha", "alpha", "medium", "prefix-alpha",
        "2", "beta!", "last",  "short",  "prefix-fallback",
        "3", NULL,    NULL,    NULL,     NULL,
        "4", NULL,    NULL,    "long",   NULL,
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE elt_values ("
        "id INT, value_text VARCHAR(20), number_value INT, created_on DATE)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO elt_values VALUES "
        "(1, 'alpha', 10, '2024-01-02'), "
        "(2, 'beta', 20, '2024-03-04'), "
        "(3, NULL, NULL, NULL), "
        "(4, 'beyond', 40, '2024-05-06')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "ELT(id, value_text, CONCAT(value_text, '!'), number_value) AS selected, "
                   "ELT(id + 1, 'zero', value_text, 'last') AS expr_index, "
                   "ELT(LENGTH(value_text) - 3, 'short', 'medium', 'long') AS len_index, "
                   "CONCAT('prefix-', ELT(id, value_text, 'fallback')) AS nested_value "
                   "FROM elt_values ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .context = "table-backed elt",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_elt_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE elt_bad (id INT, approximate_value DOUBLE, binary_value VARBINARY(10))",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT ELT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ELT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ELT(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'ELT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT ELT(9223372036854775808, 'a')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ELT() index literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT ELT(1, 9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT ELT(approximate_value, 'a') FROM elt_bad",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ELT() index supports only signed integer, boolean, NULL, integer descriptor, "
                "and supported integer expression arguments",
        }
    );
    failures += execute_error(
        database,
        "SELECT ELT(id, binary_value) FROM elt_bad",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ELT() does not support binary columns",
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
