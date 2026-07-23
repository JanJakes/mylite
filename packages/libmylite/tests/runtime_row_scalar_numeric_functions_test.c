#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

struct expected_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

enum {
    mysql_error_parse = 1064,
    mysql_error_bigint_out_of_range = 1690,
    integer_projection_column_count = 10,
    floating_projection_column_count = 17,
    nested_projection_column_count = 5,
    explicit_projection_column_count = 5,
    string_projection_column_count = 8,
    string_projection_row_count = 5,
    string_projection_warning_count = 26,
    string_projection_first_warning_row = 0,
    string_projection_round_places_warning_row = 4,
    string_projection_bit_count_warning_row = 16,
    string_projection_log_warning_row = 24,
    literal_projection_column_count = 4,
    literal_projection_warning_count = 6,
};

static int test_row_scalar_numeric_functions(void);
static int open_app_database(mylite_db **out_database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error_contains(
    mylite_db *database,
    const char *sql,
    struct expected_error expected
);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_warning_row(
    const mylite_result *result,
    size_t row,
    const char *code,
    const char *message,
    const char *context
);

int main(void) {
    return test_row_scalar_numeric_functions() == 0 ? 0 : 1;
}

static int test_row_scalar_numeric_functions(void) {
    static const char *const integer_projection_values[] = {
        "1", "4", "-1", "-4", "-4", "-4", "-4", "0", "2", "2", "2", "0", "0",  "0", "0",
        "0", "0", "0",  "0",  "1",  "3",  "9",  "1", "9", "9", "9", "9", "10", "3", "4",
    };
    static const char *const rounded_float_projection_values[] = {
        "1", "0", "0", "0", "0", "0", "0", "1", "0", "1", "1", "0", "2", "2", "3", "1", "1",
        "2", "1", "0", "0", "0", "0", "1", "1", "0", "1", "1", "0", "2", "2", "3", "4", "4",
        "3", "2", "0", "0", "0", "0", "1", "1", "0", "1", "1", "0", "2", "2", "3", "9", "9",
    };
    static const char *const abs_bit_count_ids[] = {"1", "3"};
    static const char *const rounded_sin_ids[] = {"2", "3"};
    static const char *const sqrt_null_ids[] = {"1"};
    static const char *const log_null_ids[] = {"1", "2"};
    static const char *const acos_null_ids[] = {"1", "3"};
    static const char *const nested_values[] = {
        "1",
        "5",
        "0",
        "1",
        "-1",
        "2",
        "1",
        "1",
        "100",
        "0",
        "3",
        "10",
        "0",
        "9",
        "1",
    };
    static const char *const explicit_numeric_predicate_values[] = {"2", "0", "8", "1", "5"};
    static const char *const string_projection_values[] = {
        "1",
        "64",
        "1",
        "64",
        "64",
        "8",
        "4.1588830833596715",
        "1",
        "2",
        "64",
        "1",
        "64",
        "64",
        "8",
        "4.1588830833596715",
        "1",
        "3",
        "0",
        "0",
        "0",
        "0",
        "0",
        NULL,
        "0",
        "4",
        "25",
        "-1",
        "-25",
        "-20",
        NULL,
        NULL,
        "63",
        "5",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const literal_projection_values[] = {"64", "12.3", "3", NULL};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE numeric_rows("
        "id INT PRIMARY KEY, "
        "i INT, "
        "mask INT, "
        "angle INT, "
        "nullable INT"
        ")"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE text_rows(id INT, label VARCHAR(32), places VARCHAR(8))"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO numeric_rows(id, i, mask, angle, nullable) VALUES "
        "(1, -4, 3, 0, NULL), "
        "(2, 0, 8, 1, 5), "
        "(3, 9, 15, 2, NULL)"
    );
    failures += expect_statement_ok(
        database,
        "INSERT INTO text_rows VALUES "
        "(1, '64', '1'), "
        "(2, '64x', '1x'), "
        "(3, 'x64', 'x'), "
        "(4, ' -2.5e1abc', '-1x'), "
        "(5, NULL, NULL)"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ABS(i), SIGN(i), CEIL(i), CEILING(i), FLOOR(i), "
                   "ROUND(i), ROUND(i, -1), SQRT(ABS(i)), BIT_COUNT(mask) "
                   "FROM numeric_rows ORDER BY id",
            .values = integer_projection_values,
            .column_count = integer_projection_column_count,
            .row_count = 3U,
            .context = "integer numeric function projection",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ROUND(DEGREES(RADIANS(angle))), ROUND(ACOS(1)), "
                   "ROUND(ASIN(0)), ROUND(ATAN(0)), ROUND(ATAN2(0, 1)), "
                   "ROUND(SIN(angle)), ROUND(COS(angle - angle)), ROUND(TAN(0)), "
                   "ROUND(COT(1)), ROUND(EXP(0)), ROUND(LN(1)), "
                   "ROUND(LOG(10, 100)), ROUND(LOG10(100)), ROUND(LOG2(8)), "
                   "ROUND(POW(id, 2)), ROUND(POWER(id, 2)) "
                   "FROM numeric_rows ORDER BY id",
            .values = rounded_float_projection_values,
            .column_count = floating_projection_column_count,
            .row_count = 3U,
            .context = "rounded floating numeric function projection",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ABS(label), SIGN(label), ROUND(label), "
                   "ROUND(label, places), SQRT(label), LOG(label), BIT_COUNT(label) "
                   "FROM text_rows ORDER BY id",
            .values = string_projection_values,
            .column_count = string_projection_column_count,
            .row_count = string_projection_row_count,
            .warning_count = string_projection_warning_count,
            .context = "string numeric function projection",
        }
    );
    failures += execute_ok(database, "SHOW WARNINGS", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            string_projection_warning_count,
            "string numeric warning row count"
        );
        failures += expect_warning_row(
            result,
            string_projection_first_warning_row,
            "1292",
            "Truncated incorrect DOUBLE value: '64x'",
            "first string numeric warning"
        );
        failures += expect_warning_row(
            result,
            string_projection_round_places_warning_row,
            "1292",
            "Truncated incorrect INTEGER value: '1x'",
            "round places string warning"
        );
        failures += expect_warning_row(
            result,
            string_projection_bit_count_warning_row,
            "1292",
            "Truncated incorrect INTEGER value: 'x64'",
            "bit count string warning"
        );
        failures += expect_warning_row(
            result,
            string_projection_log_warning_row,
            "3020",
            "Invalid argument for logarithm",
            "logarithm string warning"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ABS('64x'), ROUND('12.345x', '1.9'), "
                   "BIT_COUNT('7x'), LOG('x') FROM numeric_rows WHERE id = 1",
            .values = literal_projection_values,
            .column_count = literal_projection_column_count,
            .row_count = 1U,
            .warning_count = literal_projection_warning_count,
            .context = "string literal numeric function projection",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM numeric_rows "
                   "WHERE ABS(i) >= 4 AND (BIT_COUNT(mask) = 2 OR BIT_COUNT(mask) = 4) "
                   "ORDER BY ABS(i), id",
            .values = abs_bit_count_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "numeric predicates and order by",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, mask, angle, nullable "
                   "FROM numeric_rows WHERE BIT_COUNT(mask) = 1 ORDER BY id",
            .values = explicit_numeric_predicate_values,
            .column_count = explicit_projection_column_count,
            .row_count = 1U,
            .context = "explicit column numeric predicate routing",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM numeric_rows WHERE ROUND(SIN(angle)) = 1 ORDER BY id",
            .values = rounded_sin_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "nested numeric predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM numeric_rows WHERE SQRT(i) IS NULL ORDER BY id",
            .values = sqrt_null_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "sqrt domain predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM numeric_rows WHERE LOG(i) IS NULL ORDER BY id",
            .values = log_null_ids,
            .column_count = 1U,
            .row_count = 2U,
            .warning_count = 2U,
            .context = "log domain predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM numeric_rows WHERE ACOS(ABS(i)) IS NULL ORDER BY id",
            .values = acos_null_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "acos domain predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ABS(i) + 1, IFNULL(SIGN(nullable), 0), "
                   "IF(ABS(i), ROUND(POWER(id, 2)), 100), "
                   "CASE WHEN SIGN(i) = 0 THEN 0 ELSE SIGN(i) END "
                   "FROM numeric_rows ORDER BY id",
            .values = nested_values,
            .column_count = nested_projection_column_count,
            .row_count = 3U,
            .context = "numeric arithmetic and control flow operands",
        }
    );
    failures += execute_error_contains(
        database,
        "SELECT ABS(X'3634') FROM numeric_rows",
        (struct expected_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "numeric row-scalar arguments",
        }
    );
    failures += execute_error_contains(
        database,
        "SELECT COT(0) FROM numeric_rows",
        (struct expected_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "DOUBLE value is out of range",
        }
    );

    mylite_close(database);
    return failures;
}

static int open_app_database(mylite_db **out_database) {
    int rc = mylite_test_open_temporary(out_database);

    if (rc != MYLITE_OK) {
        return mylite_test_expect_int(rc, MYLITE_OK, "open temporary database");
    }
    return expect_statement_ok(*out_database, "CREATE DATABASE app") +
           expect_statement_ok(*out_database, "USE app");
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected SQL to succeed: %s\nerror %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
    }
    return 0;
}

static int execute_error_contains(
    mylite_db *database,
    const char *sql,
    struct expected_error expected
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected SQL to fail: %s\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "error sqlstate");
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return execute_ok(database, sql, NULL);
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t expected_value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            query.warning_count,
            query.context
        );
        for (size_t index = 0U; index < expected_value_count; ++index) {
            failures += expect_result_value(
                result,
                index / query.column_count,
                index % query.column_count,
                query.values[index],
                query.context
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

static int expect_warning_row(
    const mylite_result *result,
    size_t row,
    const char *code,
    const char *message,
    const char *context
) {
    int failures = 0;

    failures += expect_result_value(result, row, 0U, "Warning", context);
    failures += expect_result_value(result, row, 1U, code, context);
    failures += expect_result_value(result, row, 2U, message, context);
    return failures;
}
