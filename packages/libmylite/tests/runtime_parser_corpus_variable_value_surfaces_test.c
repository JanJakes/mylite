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

static int test_variable_value_surfaces(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql);
static int expect_query_cells(
    mylite_db *database,
    const char *sql,
    size_t expected_rows,
    size_t expected_columns,
    const struct expected_cell *cells,
    size_t cell_count
);

int main(void) {
    return test_variable_value_surfaces() == 0 ? 0 : 1;
}

static int test_variable_value_surfaces(void) {
    static const struct expected_cell arithmetic_cells[] = {
        {.row = 0U, .column = 0U, .expected = "9", .context = "SET variable multiply"},
        {.row = 0U, .column = 1U, .expected = "1021", .context = "SET variable add"},
        {.row = 0U, .column = 2U, .expected = "1017", .context = "SET variable modulo"},
    };
    static const struct expected_cell dml_cells[] = {
        {.row = 0U, .column = 0U, .expected = "1", .context = "first inserted id"},
        {.row = 0U, .column = 1U, .expected = "7", .context = "first inserted integer"},
        {.row = 0U, .column = 2U, .expected = "abc", .context = "first inserted text"},
        {.row = 1U, .column = 0U, .expected = "2", .context = "second inserted id"},
        {.row = 1U, .column = 1U, .expected = "9", .context = "second inserted integer"},
        {.row = 1U, .column = 2U, .expected = "abc-x", .context = "second inserted text"},
        {.row = 2U, .column = 0U, .expected = "3", .context = "system variable id"},
        {.row = 2U, .column = 1U, .expected = "0", .context = "system warning count"},
        {.row = 2U, .column = 2U, .expected = "SYSTEM", .context = "system time zone"},
    };
    static const struct expected_cell updated_cells[] = {
        {.row = 0U, .column = 0U, .expected = "11", .context = "UPDATE user variable value"},
        {.row = 0U, .column = 1U, .expected = "abc", .context = "UPDATE preserves text"},
    };
    static const struct expected_cell count_cells[] = {
        {.row = 0U, .column = 0U, .expected = "3", .context = "failed INSERT did not mutate"},
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open temporary database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t (id INT PRIMARY KEY, i INT, s VARCHAR(32))");

    failures += execute_ok(database, "SET @step = 3, @now = 1000");
    failures += execute_ok(database, "SET @step3 = @step * 3");
    failures += execute_ok(database, "SET @unix_time = @now + 7 * @step");
    failures += execute_ok(database, "SET @mod = @unix_time - @unix_time % @step3");
    failures += expect_query_cells(
        database,
        "SELECT @step3, @unix_time, @mod",
        1U,
        3U,
        arithmetic_cells,
        sizeof(arithmetic_cells) / sizeof(arithmetic_cells[0])
    );

    failures += execute_ok(database, "SET TIMESTAMP = @@TIMESTAMP + 1");
    failures += execute_ok(database, "SET @id = 1, @i = 7, @s = 'abc'");
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(@id, @i, @s), "
        "(@id + 1, @step * 3, CONCAT(@s, '-x'))"
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (3, @@warning_count, @@time_zone)");
    failures += expect_query_cells(
        database,
        "SELECT id, i, s FROM t ORDER BY id",
        3U,
        3U,
        dml_cells,
        sizeof(dml_cells) / sizeof(dml_cells[0])
    );

    failures += execute_ok(database, "SET @next = 11");
    failures += execute_ok(database, "UPDATE t SET i = @next WHERE id = 1");
    failures += expect_query_cells(
        database,
        "SELECT i, s FROM t WHERE id = 1",
        1U,
        2U,
        updated_cells,
        sizeof(updated_cells) / sizeof(updated_cells[0])
    );

    failures += execute_error(database, "INSERT INTO t VALUES (@id + id, 100, 'bad')");
    failures += expect_query_cells(
        database,
        "SELECT COUNT(*) FROM t",
        1U,
        1U,
        count_cells,
        sizeof(count_cells) / sizeof(count_cells[0])
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

static int execute_error(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
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
    failures += mylite_test_expect_size(mylite_result_row_count(result), expected_rows, sql);
    failures += mylite_test_expect_size(mylite_result_column_count(result), expected_columns, sql);
    for (size_t index = 0U; index < cell_count; ++index) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, cells[index].row, cells[index].column),
            cells[index].expected,
            cells[index].context
        );
    }
    mylite_result_free(result);
    return failures;
}
