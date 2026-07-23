#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
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

static int test_row_scalar_numeric_comparison_update_contexts(void);
static int test_row_scalar_numeric_comparison_update_does_not_widen_joined_update(void);
static int open_numeric_comparison_context_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_row_scalar_numeric_comparison_update_contexts();
    failures += test_row_scalar_numeric_comparison_update_does_not_widen_joined_update();

    return failures == 0 ? 0 : 1;
}

static int test_row_scalar_numeric_comparison_update_contexts(void) {
    static const char *const columns[] = {
        "id",
        "out_greatest",
        "out_least",
        "out_interval",
        "out_isnull",
        "out_crc32",
        "out_format",
        "out_trunc",
        "out_mod",
    };
    static const char *const values[] = {
        "1", "7",  "alpha", "2",  "0", "3504355690", "12.3", "12.3", "3",
        "2", "5",  "m",     "1",  "0", "1055472505", "56.8", "56.7", "3",
        "3", NULL, NULL,    "-1", "1", NULL,         NULL,   NULL,   NULL,
    };
    static const char *const status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const status_values[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        open_numeric_comparison_context_database(&database, "assignment", path, sizeof(path));
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_greatest = GREATEST(i, 5)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_least = LEAST(s, 'm')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_interval = INTERVAL(i, 1, 5, 10)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_isnull = ISNULL(s)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_crc32 = CRC32(s)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_format = FORMAT(d, 1)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_trunc = TRUNCATE(d, 1)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_dml_ok(
        database,
        "UPDATE t SET out_mod = MOD(i, 4)",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, out_greatest, out_least, out_interval, out_isnull, out_crc32, "
                   "out_format, out_trunc, out_mod FROM t ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 3U,
            .context = "row-scalar numeric/comparison update assignments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = status_values,
            .row_count = 1U,
            .context = "row-scalar numeric/comparison status after select",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_row_scalar_numeric_comparison_update_does_not_widen_joined_update(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_numeric_comparison_context_database(&database, "joined", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE joined_source(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO joined_source VALUES (1)", NULL);
    failures += execute_error(
        database,
        "UPDATE t JOIN joined_source ON t.id = joined_source.id "
        "SET t.out_greatest = GREATEST(t.i, 5)",
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

static int open_numeric_comparison_context_database(
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
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, "open database");
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    if (failures == 0) {
        failures += expect_dml_ok(
            *out_database,
            "CREATE TABLE t("
            "id INT, i INT, d DECIMAL(10,2), s VARCHAR(32), "
            "out_greatest INT, out_least VARCHAR(64), out_interval INT, out_isnull INT, "
            "out_crc32 BIGINT, out_format VARCHAR(64), out_trunc VARCHAR(64), out_mod INT)",
            (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
        );
    }
    if (failures == 0) {
        failures += expect_dml_ok(
            *out_database,
            "INSERT INTO t(id, i, d, s) VALUES "
            "(1, 7, 12.34, 'alpha'), "
            "(2, 3, 56.78, 'Zulu'), "
            "(3, NULL, NULL, NULL)",
            (struct expected_dml_result){.affected_rows = 3, .warning_count = 0U}
        );
    }
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char full_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(full_path, sizeof(full_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(full_path)) {
        (void)remove(full_path);
    }
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
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
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "dml column count");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "dml row count");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            "dml affected rows"
        );
        failures += mylite_test_expect_size(
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
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
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
    return mylite_test_expect_text(actual, expected, context);
}
