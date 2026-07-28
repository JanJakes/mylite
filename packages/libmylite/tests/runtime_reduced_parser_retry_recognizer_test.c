#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_primary_parser_runtime_behavior(void);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);

int main(void) {
    return test_primary_parser_runtime_behavior() == 0 ? 0 : 1;
}

static int test_primary_parser_runtime_behavior(void) {
    static const char *const tableless_limit_value[] = {"7"};
    static const char *const type_identifier_values[] = {"1", "11"};
    static const char *const index_values[] = {
        "prefix_type",
        "BTREE",
        "suffix_type",
        "BTREE",
    };
    mylite_db *database = NULL;
    int failures =
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open retry runtime");

    if (database == NULL) {
        return failures;
    }
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE type(id INT, type INT)");
    failures += execute_statement_ok(database, "INSERT INTO type VALUES (2,22), (1,11)");
    failures += execute_statement_ok(database, "CREATE INDEX prefix_type TYPE BTREE ON type(type)");
    failures += execute_statement_ok(database, "CREATE INDEX suffix_type ON type(id) TYPE BTREE");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 7 LIMIT 1",
            .values = tableless_limit_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "tableless limit runtime",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, type FROM type ORDER BY type LIMIT 1",
            .values = type_identifier_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "TYPE identifiers runtime",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT INDEX_NAME, INDEX_TYPE FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'type' ORDER BY INDEX_NAME",
            .values = index_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "legacy TYPE index runtime",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int status = mylite_execute(database, sql, strlen(sql), &result);

    if (status != MYLITE_OK) {
        fprintf(stderr, "expected OK for [%s], got %d %s\n", sql, status, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int status = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (status != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK for [%s], got %d %s\n",
            expected.context,
            expected.sql,
            status,
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
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
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += mylite_test_expect_text(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}
