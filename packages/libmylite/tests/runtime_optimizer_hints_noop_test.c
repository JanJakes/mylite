#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_optimizer_hints_are_noop_comments(void);
static int test_optimizer_hints_preserve_dml_paths(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_optimizer_hints_are_noop_comments();
    failures += test_optimizer_hints_preserve_dml_paths();

    return failures == 0 ? 0 : 1;
}

static int test_optimizer_hints_are_noop_comments(void) {
    static const char *const select_columns[] = {"one"};
    static const char *const select_values[] = {"1"};
    static const char *const diagnostics_columns[] =
        {"ROW_COUNT()", "@@warning_count", "@@error_count"};
    static const char *const select_diagnostics[] = {"-1", "0", "0"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open select memory");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT /*+ MAX_EXECUTION_TIME(1000) */ 1 AS one",
            .columns = select_columns,
            .column_count = 1U,
            .values = select_values,
            .row_count = 1U,
            .context = "select optimizer hint no-op",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .columns = diagnostics_columns,
            .column_count = 3U,
            .values = select_diagnostics,
            .row_count = 1U,
            .context = "select hint diagnostics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1 /*+ NO_SUCH_HINT() */ AS one",
            .columns = select_columns,
            .column_count = 1U,
            .values = select_values,
            .row_count = 1U,
            .context = "trailing hint-shaped comment no-op",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, @@error_count",
            .columns = diagnostics_columns,
            .column_count = 3U,
            .values = select_diagnostics,
            .row_count = 1U,
            .context = "trailing hint comment diagnostics",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_optimizer_hints_preserve_dml_paths(void) {
    static const char *const columns[] = {"id", "v", "s"};
    static const char *const hinted_select_columns[] = {"id", "v"};
    static const char *const hinted_select_row[] = {"1", "10"};
    static const char *const remaining_row[] = {"2", "25", "b"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "dml") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open dml file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t (id INT PRIMARY KEY, v INT NOT NULL, s VARCHAR(20))",
        NULL
    );
    failures += expect_statement_ok(
        database,
        "INSERT /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (1, 10, 'a')",
        1
    );
    failures += expect_statement_ok(
        database,
        "REPLACE /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (2, 20, 'b')",
        1
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT /*+ MAX_EXECUTION_TIME(1000) */ id, v FROM t WHERE id = 1",
            .columns = hinted_select_columns,
            .column_count = 2U,
            .values = hinted_select_row,
            .row_count = 1U,
            .context = "table select hint no-op rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "UPDATE /*+ SET_VAR(sort_buffer_size=262144) */ t SET v = 25 WHERE id = 2",
        1
    );
    failures += expect_statement_ok(
        database,
        "DELETE /*+ SET_VAR(sort_buffer_size=262144) */ FROM t WHERE id = 1",
        1
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, s FROM t ORDER BY id",
            .columns = columns,
            .column_count = 3U,
            .values = remaining_row,
            .row_count = 1U,
            .context = "dml hint no-op rows",
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

static int expect_statement_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
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
