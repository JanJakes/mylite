#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_convert_tz.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

static int test_direct_scalar_boundaries(void);
static int test_row_backed_boundaries(void);
static int test_length_aware_conversion_api(void);
static int test_invalid_fraction_warning(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
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

    failures += test_direct_scalar_boundaries();
    failures += test_row_backed_boundaries();
    failures += test_length_aware_conversion_api();
    failures += test_invalid_fraction_warning();

    return failures == 0 ? 0 : 1;
}

static int test_direct_scalar_boundaries(void) {
    static const char *const lower_values[] = {
        "1970-01-01 00:00:00",
        "1970-01-01 00:00:00.999999",
        "1970-01-01 01:00:01",
        "1970-01-01 01:00:01.000001",
        "1969-12-31 10:01:00.999999",
        "1970-01-01 14:00:01.000000",
        "1970-01-01 14:00:00.999999",
        "1969-12-31 10:01:01.000000",
    };
    static const char *const upper_values[] = {
        "3001-01-19 00:59:59.999999",
        "3001-01-19 00:00:00.000000",
        "3001-01-19 13:59:59.999999",
        "3001-01-18 10:01:00.000000",
        "3001-01-18 10:00:59.999999",
        "3001-01-19 14:00:00.000000",
    };
    static const char *const calendar_values[] = {
        "2004-01-01 14:30:00.1",
        "2004-01-01 14:30:00.12",
        "2004-01-01 14:30:00.123456",
        "2000-02-28 22:30:00.123456",
        "2000-03-01 01:30:00.654321",
        "2004-02-29 22:30:00.000001",
        "0001-01-01 00:00:00",
        "9999-12-31 23:59:59",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open boundary database");
    if (failures != 0) {
        return failures;
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "CONVERT_TZ('1970-01-01 00:00:00','+00:00','+01:00'),"
                   "CONVERT_TZ('1970-01-01 00:00:00.999999','+00:00','+01:00'),"
                   "CONVERT_TZ('1970-01-01 00:00:01','+00:00','+01:00'),"
                   "CONVERT_TZ('1970-01-01 00:00:01.000001','+00:00','+01:00'),"
                   "CONVERT_TZ('1969-12-31 10:01:00.999999','-13:59','+14:00'),"
                   "CONVERT_TZ('1969-12-31 10:01:01.000000','-13:59','+14:00'),"
                   "CONVERT_TZ('1970-01-01 14:00:00.999999','+14:00','-13:59'),"
                   "CONVERT_TZ('1970-01-01 14:00:01.000000','+14:00','-13:59')",
            .values = lower_values,
            .column_count = sizeof(lower_values) / sizeof(lower_values[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "lower UTC boundary",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "CONVERT_TZ('3001-01-18 23:59:59.999999','+00:00','+01:00'),"
                   "CONVERT_TZ('3001-01-19 00:00:00.000000','+00:00','+01:00'),"
                   "CONVERT_TZ('3001-01-18 10:00:59.999999','-13:59','+14:00'),"
                   "CONVERT_TZ('3001-01-18 10:01:00.000000','-13:59','+14:00'),"
                   "CONVERT_TZ('3001-01-19 13:59:59.999999','+14:00','-13:59'),"
                   "CONVERT_TZ('3001-01-19 14:00:00.000000','+14:00','-13:59')",
            .values = upper_values,
            .column_count = sizeof(upper_values) / sizeof(upper_values[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "upper UTC boundary",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "CONVERT_TZ('2004-01-01 12:00:00.1','+00:00','+02:30'),"
                   "CONVERT_TZ('2004-01-01 12:00:00.12','+00:00','+02:30'),"
                   "CONVERT_TZ('2004-01-01 12:00:00.123456','+00:00','+02:30'),"
                   "CONVERT_TZ('2000-02-29 00:30:00.123456','+01:00','-01:00'),"
                   "CONVERT_TZ('2000-02-29 23:30:00.654321','-01:00','+01:00'),"
                   "CONVERT_TZ('2004-03-01 00:30:00.000001','+01:00','-01:00'),"
                   "CONVERT_TZ('0001-01-01 00:00:00','+00:00','+01:00'),"
                   "CONVERT_TZ('9999-12-31 23:59:59','+00:00','+01:00')",
            .values = calendar_values,
            .column_count = sizeof(calendar_values) / sizeof(calendar_values[0]),
            .row_count = 1U,
            .warning_count = 0U,
            .context = "fractions leap days and distant years",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_row_backed_boundaries(void) {
    static const char *const values[] = {
        "1",
        "1970-01-01 00:00:00.999999",
        "2",
        "1970-01-01 14:00:01.000000",
        "3",
        "3001-01-19 13:59:59.999999",
        "4",
        "3001-01-18 10:01:00.000000",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open row boundary db");
    if (failures != 0) {
        return failures;
    }
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE boundaries("
        "id INT, dt VARCHAR(26), from_tz VARCHAR(6), to_tz VARCHAR(6))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO boundaries VALUES "
        "(1,'1970-01-01 00:00:00.999999','+00:00','+01:00'),"
        "(2,'1969-12-31 10:01:01.000000','-13:59','+14:00'),"
        "(3,'3001-01-18 10:00:59.999999','-13:59','+14:00'),"
        "(4,'3001-01-18 10:01:00.000000','-13:59','+14:00')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, CONVERT_TZ(dt,from_tz,to_tz) "
                   "FROM boundaries ORDER BY id",
            .values = values,
            .column_count = 2U,
            .row_count = 4U,
            .warning_count = 0U,
            .context = "row-backed UTC boundaries",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_length_aware_conversion_api(void) {
    static const char input[] = {
        '3', '0', '0', '1', '-', '0', '1', '-', '1', '9', ' ', '0', '0',
        ':', '0', '0', ':', '0', '0', '.', '0', '0', '0', '0', '0', '0',
    };
    static const char from_zone[] = {'+', '0', '0', ':', '0', '0'};
    static const char to_zone[] = {'+', '0', '1', ':', '0', '0'};
    static const char expected[] = "3001-01-19 00:00:00.000000";
    mylite_db *database = NULL;
    char *result = NULL;
    bool is_null = false;
    int failures = 0;
    int rc = MYLITE_OK;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open direct boundary db");
    if (failures != 0) {
        return failures;
    }
    rc = mylite_convert_tz_value(
        database,
        input,
        sizeof(input),
        false,
        from_zone,
        sizeof(from_zone),
        false,
        to_zone,
        sizeof(to_zone),
        false,
        &result,
        &is_null
    );
    failures += mylite_test_expect_int(rc, MYLITE_OK, "length-aware conversion status");
    failures += mylite_test_expect_true(!is_null, "length-aware conversion is not NULL");
    failures += mylite_test_expect_text(result, expected, "length-aware original result");

    free(result);
    mylite_close(database);
    return failures;
}

static int test_invalid_fraction_warning(void) {
    static const char *const values[] = {NULL};
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open fraction warning db"
    );
    if (failures != 0) {
        return failures;
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONVERT_TZ("
                   "'2004-01-01 12:00:00.1234567','+00:00','+01:00')",
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "seven-digit fraction warning",
        }
    );

    mylite_close(database);
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
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
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
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return mylite_test_expect_true(
            mylite_result_value_is_null(result, row, column) != 0,
            context
        );
    }
    return mylite_test_expect_text(actual, expected, context);
}
