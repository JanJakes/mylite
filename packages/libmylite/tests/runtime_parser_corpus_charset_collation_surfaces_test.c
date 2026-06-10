#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

struct expected_cell {
    size_t row;
    size_t column;
    const char *expected;
    const char *context;
};

static int test_charset_collation_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error_contains(mylite_db *database, const char *sql, const char *expected);
static int expect_query_cells(
    mylite_db *database,
    const char *sql,
    size_t expected_rows,
    size_t expected_columns,
    const struct expected_cell *cells,
    size_t cell_count
);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);

int main(void) {
    return test_charset_collation_surfaces() == 0 ? 0 : 1;
}

static int test_charset_collation_surfaces(void) {
    static const struct expected_cell value_cells[] = {
        {.row = 0U, .column = 0U, .expected = "1", .context = "first id"},
        {.row = 0U, .column = 1U, .expected = "abc", .context = "introduced ascii string"},
        {.row = 1U, .column = 0U, .expected = "2", .context = "second id"},
        {.row = 1U, .column = 1U, .expected = "def", .context = "introduced update string"},
        {.row = 2U, .column = 0U, .expected = "3", .context = "third id"},
        {.row = 2U, .column = 1U, .expected = "mix", .context = "ordinary collated string"},
    };
    static const struct expected_cell predicate_equal_cells[] = {
        {.row = 0U, .column = 0U, .expected = "1", .context = "introduced equality"},
    };
    static const struct expected_cell predicate_like_cells[] = {
        {.row = 0U, .column = 0U, .expected = "2", .context = "introduced like"},
    };
    static const struct expected_cell predicate_between_cells[] = {
        {.row = 0U, .column = 0U, .expected = "1", .context = "introduced between"},
    };
    static const struct expected_cell predicate_in_cells[] = {
        {.row = 0U, .column = 0U, .expected = "2", .context = "introduced in"},
    };
    static const struct expected_cell ascii_column_cells[] = {
        {.row = 0U, .column = 0U, .expected = "b", .context = "binary ascii column name"},
        {.row = 0U, .column = 1U, .expected = "ascii", .context = "binary ascii charset"},
        {.row = 0U, .column = 2U, .expected = "ascii_bin", .context = "binary ascii collation"},
        {.row = 1U, .column = 0U, .expected = "v", .context = "ascii column name"},
        {.row = 1U, .column = 1U, .expected = "ascii", .context = "ascii charset"},
        {.row = 1U, .column = 2U, .expected = "ascii_general_ci", .context = "ascii collation"},
    };
    static const struct expected_cell enum_column_cells[] = {
        {.row = 0U, .column = 0U, .expected = "c", .context = "enum column"},
        {.row = 0U, .column = 1U, .expected = "enum('a','b')", .context = "enum labels"},
    };
    static const struct expected_cell set_column_cells[] = {
        {.row = 0U, .column = 0U, .expected = "c", .context = "set column"},
        {.row = 0U, .column = 1U, .expected = "set('x','a','b')", .context = "set members"},
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transient database");
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t (id INT PRIMARY KEY, s VARCHAR(16))");
    failures += execute_ok(database, "INSERT INTO t VALUES (1, _ascii'abc')");
    failures += execute_ok(database, "INSERT INTO t VALUES (2, _utf8mb4'xyz')");
    failures += execute_ok(database, "UPDATE t SET s = _ascii'def' WHERE id = 2");
    failures += execute_ok(database, "INSERT INTO t VALUES (3, 'mix' COLLATE utf8mb4_bin)");
    failures += expect_query_cells(
        database,
        "SELECT id, s FROM t ORDER BY id",
        3U,
        2U,
        value_cells,
        sizeof(value_cells) / sizeof(value_cells[0])
    );
    failures += expect_query_cells(
        database,
        "SELECT id FROM t WHERE s = _ascii'abc' ORDER BY id",
        1U,
        1U,
        predicate_equal_cells,
        sizeof(predicate_equal_cells) / sizeof(predicate_equal_cells[0])
    );
    failures += expect_query_cells(
        database,
        "SELECT id FROM t WHERE s LIKE _ascii'd%' ORDER BY id",
        1U,
        1U,
        predicate_like_cells,
        sizeof(predicate_like_cells) / sizeof(predicate_like_cells[0])
    );
    failures += expect_query_cells(
        database,
        "SELECT id FROM t WHERE s BETWEEN _ascii'abb' AND _ascii'abd' ORDER BY id",
        1U,
        1U,
        predicate_between_cells,
        sizeof(predicate_between_cells) / sizeof(predicate_between_cells[0])
    );
    failures += expect_query_cells(
        database,
        "SELECT id FROM t WHERE s IN (_ascii'def', _ascii'missing') ORDER BY id",
        1U,
        1U,
        predicate_in_cells,
        sizeof(predicate_in_cells) / sizeof(predicate_in_cells[0])
    );

    failures += execute_ok(
        database,
        "CREATE TABLE ascii_attr (v VARCHAR(8) ASCII, b VARCHAR(8) BINARY ASCII)"
    );
    failures += expect_query_cells(
        database,
        "SELECT COLUMN_NAME, CHARACTER_SET_NAME, COLLATION_NAME "
        "FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ascii_attr' "
        "ORDER BY COLUMN_NAME",
        2U,
        3U,
        ascii_column_cells,
        sizeof(ascii_column_cells) / sizeof(ascii_column_cells[0])
    );

    failures += execute_ok(database, "CREATE TABLE enum_hex (c ENUM(0x61, b'01100010'))");
    failures += execute_ok(database, "CREATE TABLE set_hex (c SET('x', 0x61, b'01100010'))");
    failures += execute_error_contains(
        database,
        "CREATE TABLE enum_nul (c ENUM(0x00))",
        "ENUM labels do not support NUL bytes"
    );
    failures += expect_query_cells(
        database,
        "SELECT COLUMN_NAME, COLUMN_TYPE "
        "FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'enum_hex' "
        "ORDER BY COLUMN_NAME",
        1U,
        2U,
        enum_column_cells,
        sizeof(enum_column_cells) / sizeof(enum_column_cells[0])
    );
    failures += expect_query_cells(
        database,
        "SELECT COLUMN_NAME, COLUMN_TYPE "
        "FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'set_hex' "
        "ORDER BY COLUMN_NAME",
        1U,
        2U,
        set_column_cells,
        sizeof(set_column_cells) / sizeof(set_column_cells[0])
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error_contains(mylite_db *database, const char *sql, const char *expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    const char *message = mylite_errmsg(database);

    if (rc == MYLITE_OK || message == NULL || strstr(message, expected) == NULL) {
        fprintf(
            stderr,
            "%s: expected error containing [%s], got %d: %s\n",
            sql,
            expected,
            rc,
            message == NULL ? "(null)" : message
        );
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int expect_query_cells(
    mylite_db *database,
    const char *sql,
    size_t expected_rows,
    size_t expected_columns,
    const struct expected_cell *cells,
    size_t cell_count
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_row_count(result), expected_rows, sql);
    failures += expect_size(mylite_result_column_count(result), expected_columns, sql);
    for (size_t index = 0U; index < cell_count; ++index) {
        failures += expect_text(
            mylite_result_value_text(result, cells[index].row, cells[index].column),
            cells[index].expected,
            cells[index].context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}
