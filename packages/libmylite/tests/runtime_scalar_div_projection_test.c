#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

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
    path_suffix_capacity = 16,
    core_column_count = 9,
    precedence_column_count = 8,
    null_operand_column_count = 6,
    zero_divisor_column_count = 4,
    nested_warning_column_count = 5,
    left_null_short_circuit_column_count = 8,
    boundary_column_count = 8,
    division_by_zero_warning_count = 2,
    left_null_warning_count = 3,
    mysql_error_parse = 1064,
    mysql_error_bigint_out_of_range = 1690,
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
    size_t warning_count;
    int64_t affected_rows;
    const char *context;
};

static int test_scalar_div_values_and_file_safety(void);
static int test_scalar_div_warnings_and_diagnostics(void);
static int test_scalar_div_overflow_and_unsupported_forms(void);
static int test_scalar_div_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_scalar_div_values_and_file_safety();
    failures += test_scalar_div_warnings_and_diagnostics();
    failures += test_scalar_div_overflow_and_unsupported_forms();
    failures += test_scalar_div_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_div_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "5 DIV 2",
        "-5 DIV 2",
        "5 DIV -2",
        "-5 DIV -2",
        "TRUE DIV 2",
        "FALSE DIV 2",
        "5 DIV TRUE",
        "+5 DIV +2",
        "-5 DIV +2",
    };
    static const char *const core_values[] = {"2", "-2", "-2", "2", "0", "0", "5", "2", "-2"};
    static const char *const precedence_columns[] = {
        "1+5 DIV 2*3",
        "(1+5) DIV 2*3",
        "5 DIV 2+3",
        "5*3 DIV 4",
        "5 DIV 3 DIV 2",
        "-(5 DIV 2)",
        "5 DIV 2 % 2",
        "5 % 2 DIV 1",
    };
    static const char *const precedence_values[] = {"7", "9", "5", "3", "0", "-2", "0", "1"};
    static const char *const function_columns[] = {"a", "b", "c"};
    static const char *const function_values[] = {"2", "2", NULL};
    static const char *const boundary_columns[] = {
        "9223372036854775807 DIV 2",
        "-9223372036854775807 DIV 2",
        "(-9223372036854775807-1) DIV 2",
        "(-9223372036854775807-1) DIV -2",
        "9223372036854775807 DIV -1",
        "9223372036854775807 DIV 1",
        "9223372036854775807 DIV -9223372036854775807",
        "(-9223372036854775807-1) DIV 9223372036854775807",
    };
    static const char *const boundary_values[] = {
        "4611686018427387903",
        "-4611686018427387903",
        "-4611686018427387904",
        "4611686018427387904",
        "-9223372036854775807",
        "9223372036854775807",
        "-1",
        "-1",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 5 DIV 2, -5 DIV 2, 5 DIV -2, -5 DIV -2, TRUE DIV 2, "
                   "FALSE DIV 2, 5 DIV TRUE, +5 DIV +2, -5 DIV +2",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core div values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1+5 DIV 2*3, (1+5) DIV 2*3, 5 DIV 2+3, "
                   "5*3 DIV 4, 5 DIV 3 DIV 2, -(5 DIV 2), 5 DIV 2 % 2, "
                   "5 % 2 DIV 1",
            .columns = precedence_columns,
            .column_count = precedence_column_count,
            .values = precedence_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div precedence",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT IFNULL(NULL,5) DIV 2 AS a, 5 DIV IF(1,2,3) b, "
                   "NULLIF(5,5) DIV 2 c FROM DUAL",
            .columns = function_columns,
            .column_count = 3U,
            .values = function_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div scalar function operands",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 9223372036854775807 DIV 2, -9223372036854775807 DIV 2, "
                   "(-9223372036854775807-1) DIV 2, "
                   "(-9223372036854775807-1) DIV -2, "
                   "9223372036854775807 DIV -1, 9223372036854775807 DIV 1, "
                   "9223372036854775807 DIV -9223372036854775807, "
                   "(-9223372036854775807-1) DIV 9223372036854775807",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div signed boundaries",
        }
    );

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "scalar div leaves catalog generation unchanged"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "scalar div leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read div preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "scalar div leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_scalar_div_warnings_and_diagnostics(void) {
    static const char *const null_columns[] = {
        "NULL DIV 0",
        "0 DIV NULL",
        "5 DIV NULL",
        "NULL DIV 5",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const null_values[] = {NULL, NULL, NULL, NULL, "0", "0"};
    static const char *const zero_columns[] = {
        "5 DIV 0",
        "5 DIV FALSE",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const zero_values[] = {NULL, NULL, "0", "0"};
    static const char *const nested_columns[] = {
        "5 DIV 0 DIV 1",
        "5 DIV NULL DIV 0",
        "NULL DIV 0 DIV 0",
        "(5 DIV 0) + 1",
        "@@warning_count",
    };
    static const char *const nested_values[] = {NULL, NULL, NULL, NULL, "0"};
    static const char *const left_null_columns[] = {
        "NULL DIV (5 DIV 0)",
        "+NULL DIV (5 DIV 0)",
        "IFNULL(NULL,NULL) DIV (5 DIV 0)",
        "COALESCE(NULL,NULL) DIV (5 DIV 0)",
        "NULLIF(NULL,5 DIV 0) DIV (5 DIV 0)",
        "NULLIF(1,1) DIV (5 DIV 0)",
        "(5 DIV 0) DIV (5 DIV 0)",
        "@@warning_count",
    };
    static const char *const left_null_values[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, "0"};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_values[] = {
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
    };
    static const char *const left_null_warning_values[] = {
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
        "Warning",
        "1365",
        "Division by 0",
    };
    static const char *const following_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const no_warning_values[] = {"0", "-1"};
    static const char *const warning_count_values[] = {"2", "-1"};
    static const char *const following_warning_column[] = {"@@warning_count"};
    static const char *const following_warning_value[] = {"2"};
    static const char *const following_left_null_warning_value[] = {"3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "warnings") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open warning handle");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE rc_seed(id INT)", NULL);
    failures += execute_ok(database, "UPDATE rc_seed SET id = 1 WHERE id = 2", NULL);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NULL DIV 0, 0 DIV NULL, 5 DIV NULL, NULL DIV 5, "
                   "@@warning_count, ROW_COUNT()",
            .columns = null_columns,
            .column_count = null_operand_column_count,
            .values = null_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div null operands no warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = following_columns,
            .column_count = 2U,
            .values = no_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div null operands following counts",
        }
    );

    failures += execute_ok(database, "UPDATE rc_seed SET id = 1 WHERE id = 2", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 5 DIV 0, 5 DIV FALSE, @@warning_count, ROW_COUNT()",
            .columns = zero_columns,
            .column_count = zero_divisor_column_count,
            .values = zero_values,
            .row_count = 1U,
            .warning_count = division_by_zero_warning_count,
            .affected_rows = 0,
            .context = "div zero divisor warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = 3U,
            .values = warning_values,
            .row_count = division_by_zero_warning_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div zero divisor show warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = following_columns,
            .column_count = 2U,
            .values = warning_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div zero divisor following counts",
        }
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 5 DIV 0 DIV 1, 5 DIV NULL DIV 0, NULL DIV 0 DIV 0, "
                   "(5 DIV 0) + 1, @@warning_count",
            .columns = nested_columns,
            .column_count = nested_warning_column_count,
            .values = nested_values,
            .row_count = 1U,
            .warning_count = division_by_zero_warning_count,
            .affected_rows = 0,
            .context = "div nested null and zero warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = following_warning_column,
            .column_count = 1U,
            .values = following_warning_value,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div nested following warning count",
        }
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NULL DIV (5 DIV 0), +NULL DIV (5 DIV 0), "
                   "IFNULL(NULL,NULL) DIV (5 DIV 0), "
                   "COALESCE(NULL,NULL) DIV (5 DIV 0), "
                   "NULLIF(NULL,5 DIV 0) DIV (5 DIV 0), "
                   "NULLIF(1,1) DIV (5 DIV 0), (5 DIV 0) DIV (5 DIV 0), "
                   "@@warning_count",
            .columns = left_null_columns,
            .column_count = left_null_short_circuit_column_count,
            .values = left_null_values,
            .row_count = 1U,
            .warning_count = left_null_warning_count,
            .affected_rows = 0,
            .context = "div left null short circuit warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = 3U,
            .values = left_null_warning_values,
            .row_count = left_null_warning_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div left null short circuit show warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = following_warning_column,
            .column_count = 1U,
            .values = following_left_null_warning_value,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "div left null short circuit following warning count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_div_overflow_and_unsupported_forms(void) {
    static const char *const table_div_columns[] = {"id DIV 2"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported handle");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);

    failures += execute_error(
        database,
        "SELECT (-9223372036854775807-1) DIV 1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT (-9223372036854775807-1) DIV -1",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 3037000500*3037000500 DIV 2",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+1/0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT '5' DIV 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_error(
        database,
        "SELECT 5.5 DIV 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id DIV 2 FROM t",
            .columns = table_div_columns,
            .column_count = sizeof(table_div_columns) / sizeof(table_div_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "table div projection",
        }
    );
    failures += execute_error(
        database,
        "SELECT DIV(5,2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT 5 DIV",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near",
        }
    );
    failures += execute_error(
        database,
        "SELECT DIV 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_scalar_div_independent_handles(void) {
    static const char *const first_columns[] = {"first_result", "5 DIV 0"};
    static const char *const first_values[] = {"2", NULL};
    static const char *const first_count_columns[] = {"@@warning_count"};
    static const char *const first_count_values[] = {"1"};
    static const char *const second_columns[] = {"second_result", "8 DIV 3"};
    static const char *const second_values[] = {"2", "2"};
    static const char *const second_count_columns[] = {"@@warning_count"};
    static const char *const second_count_values[] = {"0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures +=
        mylite_test_expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT 7 DIV 3 AS first_result, 5 DIV 0",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "first handle div",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT 5 DIV 2 AS second_result, 8 DIV 3",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle div",
        }
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = first_count_columns,
            .column_count = 1U,
            .values = first_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle warning count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = second_count_columns,
            .column_count = 1U,
            .values = second_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle warning count",
        }
    );

    mylite_close(second);
    mylite_close(first);
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

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
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
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        perror(path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    if (read_size != size) {
        fprintf(stderr, "expected to read %zu bytes from %s, got %zu\n", size, path, read_size);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte content mismatch\n", context);
        return 1;
    }
    return 0;
}
