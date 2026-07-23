#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    seed_bit_count = 6,
    metadata_bit_and_column = 0,
    metadata_bit_or_column = 1,
    metadata_bit_xor_column = 2,
    metadata_column_count = 3,
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

static int test_bitwise_aggregate_window_results(void);
static int test_bitwise_aggregate_window_metadata(void);
static int test_bitwise_aggregate_window_diagnostics(void);
static int seed_bits(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_bitwise_aggregate_window_results();
    failures += test_bitwise_aggregate_window_metadata();
    failures += test_bitwise_aggregate_window_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_bitwise_aggregate_window_results(void) {
    static const char *const no_source_columns[] = {"ba", "bo", "bx"};
    static const char *const no_source_values[] = {"1", "0", "3"};
    static const char *const partition_columns[] = {"id", "group_id", "ba", "bo", "bx"};
    static const char *const partition_values[] = {
        "6",  NULL, "18446744073709551615",
        "0",  "0",  "1",
        "10", "3",  "7",
        "4",  "2",  "10",
        "3",  "7",  "4",
        "3",  "10", "3",
        "7",  "4",  "4",
        "20", "8",  "14",
        "6",  "5",  "20",
        "8",  "14", "6",
    };
    static const char *const moving_columns[] = {"id", "ba", "bo", "bx"};
    static const char *const moving_values[] = {
        "1", "7",  "7",  "7",  "2", "3", "7",  "4", "3", "3",  "3",  "3",
        "4", "12", "12", "12", "5", "8", "14", "6", "6", "10", "10", "10",
    };
    static const char *const empty_frame_columns[] = {"id", "ba", "bo", "bx"};
    static const char *const empty_frame_values[] = {
        "1", "18446744073709551615", "0", "0", "2", "7",  "7",  "7",  "3", "3",  "3",  "3",
        "4", "18446744073709551615", "0", "0", "5", "12", "12", "12", "6", "10", "10", "10",
    };
    static const char *const named_columns[] = {"id", "bx"};
    static const char *const named_values[] = {
        "1",
        "7",
        "2",
        "4",
        "3",
        "4",
        "4",
        "8",
        "5",
        "2",
        "6",
        "2",
    };
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transient database"
    );

    if (failures == 0) {
        failures += seed_bits(database);
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT BIT_AND(1) OVER () AS ba, BIT_OR(NULL) OVER () AS bo, "
                   "BIT_XOR(3) OVER () AS bx",
            .columns = no_source_columns,
            .column_count = 3U,
            .values = no_source_values,
            .row_count = 1U,
            .context = "source-free bitwise aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, group_id, BIT_AND(value) OVER (PARTITION BY group_id) AS ba, "
                   "BIT_OR(value) OVER (PARTITION BY group_id) AS bo, "
                   "BIT_XOR(value) OVER (PARTITION BY group_id) AS bx "
                   "FROM bits ORDER BY group_id, id",
            .columns = partition_columns,
            .column_count = sizeof(partition_columns) / sizeof(partition_columns[0]),
            .values = partition_values,
            .row_count = seed_bit_count,
            .context = "partitioned bitwise aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "BIT_AND(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND CURRENT ROW) AS ba, "
                   "BIT_OR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND CURRENT ROW) AS bo, "
                   "BIT_XOR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND CURRENT ROW) AS bx "
                   "FROM bits ORDER BY id",
            .columns = moving_columns,
            .column_count = sizeof(moving_columns) / sizeof(moving_columns[0]),
            .values = moving_values,
            .row_count = seed_bit_count,
            .context = "moving bitwise aggregate window frame",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "BIT_AND(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND 1 PRECEDING) AS ba, "
                   "BIT_OR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND 1 PRECEDING) AS bo, "
                   "BIT_XOR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING "
                   "AND 1 PRECEDING) AS bx "
                   "FROM bits ORDER BY id",
            .columns = empty_frame_columns,
            .column_count = sizeof(empty_frame_columns) / sizeof(empty_frame_columns[0]),
            .values = empty_frame_values,
            .row_count = seed_bit_count,
            .context = "empty frame bitwise aggregate windows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, BIT_XOR(value) OVER w AS bx FROM bits "
                   "WINDOW w AS (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING "
                   "AND CURRENT ROW) ORDER BY id",
            .columns = named_columns,
            .column_count = sizeof(named_columns) / sizeof(named_columns[0]),
            .values = named_values,
            .row_count = seed_bit_count,
            .context = "named bitwise aggregate window",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_bitwise_aggregate_window_metadata(void) {
    const char *const metadata_sql = "SELECT BIT_AND(value) OVER () AS ba, "
                                     "BIT_OR(value) OVER () AS bo, "
                                     "BIT_XOR(value) OVER () AS bx "
                                     "FROM bits ORDER BY id LIMIT 1";
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transient database"
    );

    if (failures == 0) {
        failures += seed_bits(database);
    }
    if (failures == 0) {
        rc = mylite_execute(database, metadata_sql, strlen(metadata_sql), &result);
        failures +=
            mylite_test_expect_int(rc, MYLITE_OK, "bitwise aggregate window metadata query");
        if (rc != MYLITE_OK) {
            fprintf(stderr, "metadata query: %s\n", mylite_errmsg(database));
        }
    }
    if (failures != 0) {
        mylite_result_free(result);
        mylite_close(database);
        return failures;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        metadata_column_count,
        "metadata column count"
    );
    failures += mylite_test_expect_int(
        mylite_result_column_type(result, metadata_bit_and_column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "bit and metadata type"
    );
    failures += mylite_test_expect_int(
        mylite_result_column_nullable(result, metadata_bit_and_column),
        0,
        "bit and nullable"
    );
    failures += mylite_test_expect_int(
        (int)(mylite_result_column_flags(result, metadata_bit_and_column) &
              MYLITE_RESULT_COLUMN_FLAG_UNSIGNED),
        MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
        "bit and unsigned"
    );
    failures += mylite_test_expect_int(
        mylite_result_column_type(result, metadata_bit_or_column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "bit or metadata type"
    );
    failures += mylite_test_expect_int(
        mylite_result_column_nullable(result, metadata_bit_or_column),
        0,
        "bit or nullable"
    );
    failures += mylite_test_expect_int(
        (int)(mylite_result_column_flags(result, metadata_bit_or_column) &
              MYLITE_RESULT_COLUMN_FLAG_UNSIGNED),
        MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
        "bit or unsigned"
    );
    failures += mylite_test_expect_int(
        mylite_result_column_type(result, metadata_bit_xor_column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        "bit xor metadata type"
    );
    failures += mylite_test_expect_int(
        mylite_result_column_nullable(result, metadata_bit_xor_column),
        0,
        "bit xor nullable"
    );
    failures += mylite_test_expect_int(
        (int)(mylite_result_column_flags(result, metadata_bit_xor_column) &
              MYLITE_RESULT_COLUMN_FLAG_UNSIGNED),
        MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
        "bit xor unsigned"
    );

    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

static int test_bitwise_aggregate_window_diagnostics(void) {
    mylite_db *database = NULL;
    int failures = mylite_test_expect_int(
        mylite_test_open_temporary(&database),
        MYLITE_OK,
        "open transient database"
    );

    if (failures == 0) {
        failures += seed_bits(database);
    }

    failures += execute_error(
        database,
        "SELECT BIT_AND(DISTINCT value) OVER () FROM bits",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIT_AND(label) OVER () FROM bits",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "aggregate-window arguments support only signed 64-bit integer "
                            "expressions",
        }
    );
    failures += execute_error(
        database,
        "SELECT BIT_OR(1) OVER (ORDER BY id)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "BIT_OR() without a table source supports only OVER ()",
        }
    );

    mylite_close(database);
    return failures;
}

static int seed_bits(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE app");
    failures += execute_ok(database, "USE app");
    failures += execute_ok(
        database,
        "CREATE TABLE bits(id INT, group_id INT, value BIGINT, label VARCHAR(10))"
    );
    failures += execute_ok(
        database,
        "INSERT INTO bits VALUES "
        "(1,10,7,'a'),"
        "(2,10,3,'b'),"
        "(3,10,NULL,'c'),"
        "(4,20,12,'d'),"
        "(5,20,10,'e'),"
        "(6,NULL,NULL,'f')"
    );
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
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    failures += mylite_test_expect_int(rc, MYLITE_OK, expected.context);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", expected.sql, mylite_errmsg(database));
        mylite_result_free(result);
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
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
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
    return mylite_test_expect_text(
        mylite_result_value_text(result, row, column),
        expected,
        context
    );
}
