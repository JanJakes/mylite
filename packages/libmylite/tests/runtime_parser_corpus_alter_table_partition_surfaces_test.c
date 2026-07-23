#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_alter_table_partition_unsupported_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);

int main(void) {
    return test_alter_table_partition_unsupported_diagnostics() == 0 ? 0 : 1;
}

static int test_alter_table_partition_unsupported_diagnostics(void) {
    static const char *const unsupported_statements[] = {
        "ALTER TABLE t PARTITION BY HASH (id) PARTITIONS 4",
        "ALTER TABLE t REMOVE PARTITIONING",
        "ALTER TABLE t ADD PARTITION PARTITIONS 2",
        "ALTER TABLE t ADD PARTITION",
        "ALTER TABLE t DROP PARTITION p0",
        "ALTER TABLE t REORGANIZE PARTITION p0 INTO (PARTITION p0 VALUES LESS THAN (10))",
        "ALTER TABLE t REORGANIZE PARTITION",
        "ALTER TABLE t REBUILD PARTITION p0",
        "ALTER TABLE t COALESCE PARTITION 1",
        "ALTER TABLE t EXCHANGE PARTITION p0 WITH TABLE staging WITHOUT VALIDATION",
        "ALTER TABLE t ANALYZE PARTITION p0",
        "ALTER TABLE t CHECK PARTITION p0",
        "ALTER TABLE t OPTIMIZE PARTITION p0",
        "ALTER TABLE t REPAIR PARTITION p0",
        "ALTER TABLE t ALGORITHM = INPLACE, LOCK = SHARED, ADD PARTITION PARTITIONS 1",
        "ALTER TABLE t ADD COLUMN c INT, DROP PARTITION p0",
    };
    static const char *const count_rows[] = {"1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transient database"
    );
    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(database, "CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(16))");
    failures += execute_ok(database, "CREATE TABLE staging (id INT PRIMARY KEY, name VARCHAR(16))");
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'alpha')");

    for (size_t index = 0U;
         index < sizeof(unsupported_statements) / sizeof(unsupported_statements[0]);
         ++index) {
        failures += execute_error(
            database,
            unsupported_statements[index],
            (struct expected_sql_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "not supported",
            }
        );
    }

    failures += execute_error(
        database,
        "ALTER TABLE t TRUNCATE PARTITION p0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "not supported",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "partition truncate placeholder preserves rows",
        }
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_ERROR, sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    if (result != NULL) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "failed result columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "failed result rows");
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, query.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", query.sql, mylite_errmsg(database));
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    if (failures == 0 && query.values != NULL) {
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += mylite_test_expect_text(
                    mylite_result_value_text(result, row, column),
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
        }
    }
    mylite_result_free(result);
    return failures;
}
