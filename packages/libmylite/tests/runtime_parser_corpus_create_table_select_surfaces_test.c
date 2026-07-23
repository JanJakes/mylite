#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    sql_buffer_size = 256,
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

struct unsupported_statement {
    const char *sql;
    const char *target_name;
};

static int test_create_table_select_unsupported_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_target_absent(mylite_db *database, const char *target_name);
static int expect_query_values(mylite_db *database, struct expected_query query);

int main(void) {
    return test_create_table_select_unsupported_diagnostics() == 0 ? 0 : 1;
}

static int test_create_table_select_unsupported_diagnostics(void) {
    static const struct unsupported_statement unsupported_statements[] = {
        {.sql = "CREATE TABLE explicit_cols (id INT, KEY (id)) SELECT id FROM source1",
         .target_name = "explicit_cols"},
        {.sql = "CREATE TABLE table_options (id INT) ENGINE=InnoDB SELECT id FROM source1",
         .target_name = "table_options"},
        {.sql =
             "CREATE TABLE compound_source AS SELECT id FROM source1 UNION SELECT id FROM source2",
         .target_name = "compound_source"},
        {.sql = "CREATE TABLE parenthesized_source AS (SELECT id FROM source1) UNION (SELECT id "
                "FROM source2)",
         .target_name = "parenthesized_source"},
        {.sql = "CREATE TABLE cte_source WITH cte AS (SELECT id FROM source1) SELECT id FROM cte",
         .target_name = "cte_source"},
        {.sql = "CREATE TABLE table_source AS TABLE source1", .target_name = "table_source"},
        {.sql = "CREATE TABLE values_source AS VALUES ROW(1), ROW(2)",
         .target_name = "values_source"},
        {.sql = "CREATE TABLE with_table_source WITH cte AS (SELECT id FROM source1) TABLE cte",
         .target_name = "with_table_source"},
        {.sql =
             "CREATE TABLE with_values_source WITH cte AS (SELECT id FROM source1) VALUES ROW(1)",
         .target_name = "with_values_source"},
        {.sql = "CREATE TABLE partition_source (id INT) PARTITION BY HASH (id) AS SELECT id FROM "
                "source1",
         .target_name = "partition_source"},
    };
    static const char *const supported_rows[] = {"1"};
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
    failures += execute_ok(database, "CREATE TABLE source1 (id INT)");
    failures += execute_ok(database, "CREATE TABLE source2 (id INT)");
    failures += execute_ok(database, "INSERT INTO source1 VALUES (1)");
    failures += execute_ok(database, "INSERT INTO source2 VALUES (2)");

    for (size_t index = 0U;
         index < sizeof(unsupported_statements) / sizeof(unsupported_statements[0]);
         ++index) {
        failures += execute_error(
            database,
            unsupported_statements[index].sql,
            (struct expected_sql_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "not supported",
            }
        );
        failures += expect_target_absent(database, unsupported_statements[index].target_name);
    }

    failures += execute_ok(database, "CREATE TABLE supported_ctas AS SELECT id FROM source1");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "TABLE supported_ctas",
            .values = supported_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "supported CTAS still executes",
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

static int expect_target_absent(mylite_db *database, const char *target_name) {
    char sql[sql_buffer_size];
    int written = snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='%s'",
        target_name
    );
    static const char *const absent_rows[] = {"0"};

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "target name too long: %s\n", target_name);
        return 1;
    }
    return expect_query_values(
        database,
        (struct expected_query){
            .sql = sql,
            .values = absent_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = target_name,
        }
    );
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
