#include "mylite_test_support.h"

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
    test_path_suffix_capacity = 16,
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

static int test_multi_aggregate_values(void);
static int test_multi_aggregate_diagnostics(void);
static int seed_database(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query query);
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

    failures += test_multi_aggregate_values();
    failures += test_multi_aggregate_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_multi_aggregate_values(void) {
    static const char *const mixed_columns[] = {
        "total",
        "nonnull_n",
        "null_count",
        "distinct_n",
        "distinct_name",
        "distinct_raw",
        "sum_n",
        "avg_n",
        "min_n",
        "max_n",
        "and_n",
        "or_n",
        "xor_n",
        "stdp_n",
        "stds_n",
        "varp_n",
        "vars_n",
        "labels",
    };
    static const char *const mixed_values[] = {
        "4",
        "3",
        "0",
        "3",
        "2",
        "2",
        "60",
        "20.0000",
        "10",
        "30",
        "0",
        "30",
        "0",
        "8.16496580927726",
        "10",
        "66.66666666666667",
        "100",
        "a|b|c",
    };
    static const char *const filtered_columns[] = {
        "total",
        "nonnull_n",
        "sum_n",
        "avg_n",
        "min_n",
        "max_n",
        "stdp_n",
        "vars_n",
        "labels",
    };
    static const char *const filtered_values[] = {
        "2",
        "2",
        "50",
        "25.0000",
        "20",
        "30",
        "5",
        "50",
        "c",
    };
    static const char *const empty_values[] = {
        "0",
        "0",
        "0",
        NULL,
        NULL,
        NULL,
        NULL,
        "18446744073709551615",
        "0",
        "0",
        NULL,
        NULL,
        NULL,
    };
    static const char *const tableless_columns[] = {
        "COUNT(*)",
        "COUNT(1)",
        "COUNT(NULL)",
        "MIN(5)",
        "MAX(9)",
        "STDDEV_POP(1)",
        "VAR_SAMP(1)",
    };
    static const char *const tableless_values[] = {
        "1",
        "1",
        "0",
        "5",
        "9",
        "0",
        NULL,
    };
    static const char *const count_columns[] = {
        "COUNT(*)",
        "COUNT(n)",
        "COUNT(DISTINCT n)",
        "COUNT(DISTINCT name)",
        "COUNT(DISTINCT raw)",
    };
    static const char *const count_values[] = {"4", "3", "3", "2", "2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) AS total, COUNT(n) AS nonnull_n, "
                   "COUNT(NULL) AS null_count, COUNT(DISTINCT n) AS distinct_n, "
                   "COUNT(DISTINCT name) AS distinct_name, "
                   "COUNT(DISTINCT raw) AS distinct_raw, "
                   "SUM(n) AS sum_n, AVG(n) AS avg_n, MIN(n) AS min_n, MAX(n) AS max_n, "
                   "BIT_AND(n) AS and_n, BIT_OR(n) AS or_n, BIT_XOR(n) AS xor_n, "
                   "STDDEV_POP(n) AS stdp_n, STDDEV_SAMP(n) AS stds_n, "
                   "VAR_POP(n) AS varp_n, VAR_SAMP(n) AS vars_n, "
                   "GROUP_CONCAT(label ORDER BY id SEPARATOR '|') AS labels FROM t",
            .columns = mixed_columns,
            .column_count = sizeof(mixed_columns) / sizeof(mixed_columns[0]),
            .values = mixed_values,
            .row_count = 1U,
            .context = "mixed aggregate row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*), COUNT(n), COUNT(DISTINCT n), COUNT(DISTINCT name), "
                   "COUNT(DISTINCT raw) FROM t",
            .columns = count_columns,
            .column_count = sizeof(count_columns) / sizeof(count_columns[0]),
            .values = count_values,
            .row_count = 1U,
            .context = "count-only aggregate row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) AS total, COUNT(n) AS nonnull_n, SUM(n) AS sum_n, "
                   "AVG(n) AS avg_n, MIN(n) AS min_n, MAX(n) AS max_n, "
                   "STDDEV_POP(n) AS stdp_n, VAR_SAMP(n) AS vars_n, "
                   "GROUP_CONCAT(label ORDER BY id SEPARATOR ',') AS labels "
                   "FROM t WHERE n >= 20",
            .columns = filtered_columns,
            .column_count = sizeof(filtered_columns) / sizeof(filtered_columns[0]),
            .values = filtered_values,
            .row_count = 1U,
            .context = "filtered aggregate row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) AS total, COUNT(n) AS nonnull_n, "
                   "COUNT(NULL) AS null_count, SUM(n) AS sum_n, AVG(n) AS avg_n, "
                   "MIN(n) AS min_n, MAX(n) AS max_n, BIT_AND(n) AS and_n, "
                   "BIT_OR(n) AS or_n, BIT_XOR(n) AS xor_n, STDDEV_POP(n) AS stdp_n, "
                   "VAR_POP(n) AS varp_n, GROUP_CONCAT(label ORDER BY id) AS labels "
                   "FROM t WHERE id > 99",
            .columns =
                (const char *const[]){
                    "total",
                    "nonnull_n",
                    "null_count",
                    "sum_n",
                    "avg_n",
                    "min_n",
                    "max_n",
                    "and_n",
                    "or_n",
                    "xor_n",
                    "stdp_n",
                    "varp_n",
                    "labels",
                },
            .column_count = sizeof(empty_values) / sizeof(empty_values[0]),
            .values = empty_values,
            .row_count = 1U,
            .context = "empty aggregate row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*), COUNT(1), COUNT(NULL), MIN(5), MAX(9), "
                   "STDDEV_POP(1), VAR_SAMP(1)",
            .columns = tableless_columns,
            .column_count = sizeof(tableless_columns) / sizeof(tableless_columns[0]),
            .values = tableless_values,
            .row_count = 1U,
            .context = "tableless aggregate row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*), SUM(n) FROM t LIMIT 0",
            .columns = (const char *const[]){"COUNT(*)", "SUM(n)"},
            .column_count = 2U,
            .values = (const char *const[]){NULL, NULL},
            .row_count = 0U,
            .context = "limit zero aggregate row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*), SUM(n) FROM t LIMIT 1 OFFSET 1",
            .columns = (const char *const[]){"COUNT(*)", "SUM(n)"},
            .column_count = 2U,
            .values = (const char *const[]){NULL, NULL},
            .row_count = 0U,
            .context = "limit offset aggregate row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_multi_aggregate_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_database(database);
    failures += execute_error(
        database,
        "SELECT SUM(n), n FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate SELECT supports only aggregate select items",
        }
    );
    failures += execute_error(
        database,
        "SELECT COUNT(*), SUM(n) FROM (SELECT n FROM t) AS d",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate SELECT supports only descriptor-backed table reads",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_database(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "CREATE DATABASE app", &result);

    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT NOT NULL, n INT NULL, m INT NOT NULL, "
        "label VARCHAR(20) NULL, name VARCHAR(20) NULL, raw VARBINARY(4) NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, NULL, 5, 'a', NULL, NULL), "
        "(2, 10, 7, 'b', 'alpha', X'41'), "
        "(3, 20, 9, 'c', 'Alpha', X'4100'), "
        "(4, 30, 11, NULL, 'beta', X'41')",
        &result
    );
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        *out_result = NULL;
        return 1;
    }

    *out_result = result;
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "execute '%s': expected error, got OK\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t column = 0U; column < query.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            query.columns[column],
            query.sql
        );
    }
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                query.values[(row * query.column_count) + column],
                query.context
            );
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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
                "%s row %zu col %zu: expected NULL, got '%s'\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL) {
        fprintf(
            stderr,
            "%s row %zu col %zu: expected '%s', got NULL\n",
            context,
            row,
            column,
            expected
        );
        return 1;
    }
    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}
