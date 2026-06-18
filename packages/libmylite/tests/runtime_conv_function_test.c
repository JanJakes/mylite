#include <mylite/mylite.h>

#include "runtime_test_support.h"

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
    core_column_count = 9,
    parsing_column_count = 8,
    boundary_column_count = 10,
    row_column_count = 6,
    row_warning_column_count = 2,
    null_short_circuit_column_count = 4,
    warning_column_count = 5,
    show_warning_column_count = 3,
    diagnostic_status_column_count = 3,
    warning_before_error_row_count = 2,
    mysql_error_parse = 1064,
    mysql_error_native_function_arity = 1582,
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

static int test_conv_values_and_file_safety(void);
static int test_conv_warnings_and_do(void);
static int test_conv_errors_and_unsupported_forms(void);
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
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_conv_values_and_file_safety();
    failures += test_conv_warnings_and_do();
    failures += test_conv_errors_and_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_conv_values_and_file_safety(void) {
    static const char row_all_ones[] =
        "1111111111111111111111111111111111111111111111111111111111111111";
    static const char *const core_columns[] = {
        "CONV(NULL,10,2)",
        "CONV(10,NULL,2)",
        "CONV(10,10,NULL)",
        "CONV(TRUE,10,2)",
        "CONV(FALSE,10,2)",
        "CONV(0,10,2)",
        "CONV(10,10,2)",
        "CONV(35,10,36)",
        "CONV(36,10,36)",
    };
    static const char *const core_values[] = {
        NULL,
        NULL,
        NULL,
        "1",
        "0",
        "0",
        "1010",
        "Z",
        "10",
    };
    static const char *const parsing_columns[] = {
        "CONV(1010,2,10)",
        "CONV(36,36,10)",
        "CONV(12,2,10)",
        "CONV(12,3,10)",
        "CONV(-17,18,10)",
        "CONV(-17,18,-10)",
        "CONV(-17,10,18)",
        "CONV(-17,10,-18)",
    };
    static const char *const parsing_values[] = {
        "10",
        "114",
        "1",
        "5",
        "18446744073709551591",
        "-25",
        "2D3FGB0B9CG4BD1H",
        "-H",
    };
    static const char *const boundary_columns[] = {
        "CONV(-1,10,10)",
        "CONV(-1,10,-10)",
        "CONV(-2,10,10)",
        "CONV(-2,10,-10)",
        "CONV(-9223372036854775808,10,10)",
        "CONV(-9223372036854775808,10,-10)",
        "CONV(9223372036854775807,10,10)",
        "CONV(9223372036854775808,10,10)",
        "CONV(18446744073709551615,10,10)",
        "CONV(18446744073709551615,10,16)",
    };
    static const char *const boundary_values[] = {
        "18446744073709551615",
        "-1",
        "18446744073709551614",
        "-2",
        "9223372036854775808",
        "-9223372036854775808",
        "9223372036854775807",
        "9223372036854775808",
        "18446744073709551615",
        "FFFFFFFFFFFFFFFF",
    };
    static const char *const row_columns[] = {
        "id",
        "CONV(id,10,2)",
        "CONV(id,10,16)",
        "CONV(id,10,-10)",
        "CASE WHEN id=10 THEN CONV(id,10,16) END",
        "CONCAT('x',CONV(id+1,10,36))",
    };
    static const char *const row_values[] = {
        NULL, NULL,     NULL, NULL, NULL,   NULL,  "-1", row_all_ones, "FFFFFFFFFFFFFFFF",
        "-1", NULL,     "x0", "10", "1010", "A",   "10", "A",          "xB",
        "35", "100011", "23", "35", NULL,   "x10",
    };
    static const char *const predicate_columns[] = {"id"};
    static const char *const predicate_values[] = {"10", "35"};
    static const char *const order_columns[] = {"id", "CONV(id,10,2)"};
    static const char *const order_values[] = {
        NULL,
        NULL,
        "35",
        "100011",
        "10",
        "1010",
        "-1",
        row_all_ones,
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    failures += execute_ok(database, "CREATE TABLE row_values(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO row_values VALUES (NULL),(10),(35),(-1)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONV(NULL,10,2),CONV(10,NULL,2),CONV(10,10,NULL),"
                   "CONV(TRUE,10,2),CONV(FALSE,10,2),CONV(0,10,2),"
                   "CONV(10,10,2),CONV(35,10,36),CONV(36,10,36)",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core CONV values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONV(1010,2,10),CONV(36,36,10),CONV(12,2,10),"
                   "CONV(12,3,10),CONV(-17,18,10),CONV(-17,18,-10),"
                   "CONV(-17,10,18),CONV(-17,10,-18)",
            .columns = parsing_columns,
            .column_count = parsing_column_count,
            .values = parsing_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV input-base parsing",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONV(-1,10,10),CONV(-1,10,-10),"
                   "CONV(-2,10,10),CONV(-2,10,-10),"
                   "CONV(-9223372036854775808,10,10),"
                   "CONV(-9223372036854775808,10,-10),"
                   "CONV(9223372036854775807,10,10),"
                   "CONV(9223372036854775808,10,10),"
                   "CONV(18446744073709551615,10,10),"
                   "CONV(18446744073709551615,10,16)",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV boundaries",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id,CONV(id,10,2),CONV(id,10,16),CONV(id,10,-10),"
                   "CASE WHEN id=10 THEN CONV(id,10,16) END,"
                   "CONCAT('x',CONV(id+1,10,36)) "
                   "FROM row_values ORDER BY id",
            .columns = row_columns,
            .column_count = row_column_count,
            .values = row_values,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "table-backed CONV values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM row_values "
                   "WHERE CONV(id,10,16)='A' OR CONV(id,10,16)='23' ORDER BY id",
            .columns = predicate_columns,
            .column_count = 1U,
            .values = predicate_values,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV predicate values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id,CONV(id,10,2) FROM row_values ORDER BY CONV(id,10,2),id",
            .columns = order_columns,
            .column_count = 2U,
            .values = order_values,
            .row_count = 4U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV order values",
        }
    );

    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged"
    );
    mylite_close(database);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "CONV preserves MyLite preamble"
    );
    remove_related_files(path);
    return failures;
}

static int test_conv_warnings_and_do(void) {
    static const char *const warning_columns[] = {
        "CONV(2,2,10)",
        "CONV(9,8,10)",
        "CONV(19,8,10)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"0", "0", "1", "0", "0"};
    static const char *const row_warning_columns[] = {
        "id",
        "CONV(id,from_base,to_base)",
    };
    static const char *const row_warning_values[] = {
        "2",
        "0",
        "9",
        "0",
        "12",
        "5",
        "19",
        "1",
    };
    static const char *const null_short_circuit_columns[] = {
        "CONV(NULL,5 DIV 0,2)",
        "CONV(10,NULL,5 DIV 0)",
        "CONV(10,5 DIV 0,NULL)",
        "@@warning_count",
    };
    static const char *const null_short_circuit_values[] = {NULL, NULL, NULL, "0"};
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const null_short_circuit_warning_rows[] = {
        "Warning",
        "1365",
        "Division by 0",
    };
    static const char *const digit_warning_rows[] = {
        "Warning",
        "1292",
        "Truncated incorrect DECIMAL value: '2'",
        "Warning",
        "1292",
        "Truncated incorrect DECIMAL value: '9'",
    };
    static const char *const do_warning_rows[] = {
        "Warning",
        "1292",
        "Truncated incorrect DECIMAL value: '2'",
        "Warning",
        "1365",
        "Division by 0",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open warning db");
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures +=
        execute_ok(database, "CREATE TABLE row_warnings(id INT, from_base INT, to_base INT)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO row_warnings VALUES (2,2,10),(9,8,10),(12,3,10),(19,8,10)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONV(NULL,5 DIV 0,2),CONV(10,NULL,5 DIV 0),"
                   "CONV(10,5 DIV 0,NULL),@@warning_count",
            .columns = null_short_circuit_columns,
            .column_count = null_short_circuit_column_count,
            .values = null_short_circuit_values,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "CONV NULL short-circuit warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = null_short_circuit_warning_rows,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV NULL short-circuit diagnostics",
        }
    );

    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id,CONV(id,from_base,to_base) FROM row_warnings ORDER BY id",
            .columns = row_warning_columns,
            .column_count = row_warning_column_count,
            .values = row_warning_values,
            .row_count = 4U,
            .warning_count = 2U,
            .affected_rows = 0,
            .context = "table-backed CONV digit warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = digit_warning_rows,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "table-backed CONV warning diagnostics",
        }
    );

    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONV(2,2,10),CONV(9,8,10),CONV(19,8,10),"
                   "@@warning_count,ROW_COUNT()",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 2U,
            .affected_rows = 0,
            .context = "CONV invalid leading digit warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = digit_warning_rows,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV digit warning diagnostics",
        }
    );

    failures += execute_ok(database, "DO CONV(2,2,10),CONV(5 DIV 0,10,2)", &result);
    failures += expect_size(mylite_result_column_count(result), 0U, "CONV DO column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "CONV DO row count");
    failures += expect_size(mylite_result_warning_count(result), 2U, "CONV DO warning count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "CONV DO affected rows");
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = do_warning_rows,
            .row_count = 2U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV DO warning diagnostics",
        }
    );
    failures += execute_ok(database, "DO CONV(10,10,2),CONV(NULL,10,2)", &result);
    failures += expect_size(mylite_result_column_count(result), 0U, "CONV successful DO columns");
    failures += expect_size(mylite_result_row_count(result), 0U, "CONV successful DO rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "CONV successful DO warnings");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "CONV successful DO rows");
    mylite_result_free(result);

    mylite_close(database);
    return failures;
}

static int test_conv_errors_and_unsupported_forms(void) {
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_before_error_rows[] = {
        "Warning",
        "1292",
        "Truncated incorrect DECIMAL value: '2'",
        "Error",
        "1690",
        "BIGINT value is out of range in scalar arithmetic expression",
    };
    static const char *const diagnostic_status_columns[] = {
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_before_error_status[] = {"2", "1", "-1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open error db");
    failures += execute_error(
        database,
        "SELECT CONV()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'CONV'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONV(1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'CONV'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONV(1,10)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'CONV'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONV(1,10,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'CONV'",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONV(3037000500*3037000500,10,2)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_error(
        database,
        "SELECT CONV(2,2,10),CONV(3037000500*3037000500,10,2)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = warning_before_error_rows,
            .row_count = warning_before_error_row_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV SELECT warning before error diagnostics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,@@error_count,ROW_COUNT()",
            .columns = diagnostic_status_columns,
            .column_count = diagnostic_status_column_count,
            .values = warning_before_error_status,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV SELECT warning before error status",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_error(
        database,
        "DO CONV(2,2,10),CONV(3037000500*3037000500,10,2)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = show_warning_columns,
            .column_count = show_warning_column_count,
            .values = warning_before_error_rows,
            .row_count = warning_before_error_row_count,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV DO warning before error diagnostics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,@@error_count,ROW_COUNT()",
            .columns = diagnostic_status_columns,
            .column_count = diagnostic_status_column_count,
            .values = warning_before_error_status,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "CONV DO warning before error status",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONV('6E',18,8)",
        (struct expected_sql_error){
            .code = 0,
            .sqlstate = "42000",
            .message_part = "CONV() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT CONV(10,~0,2)",
        (struct expected_sql_error){
            .code = 0,
            .sqlstate = "42000",
            .message_part = "CONV() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+CONV(10,10,2)",
        (struct expected_sql_error){
            .code = 0,
            .sqlstate = "42000",
            .message_part = "SELECT scalar projection supports",
        }
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
        return failures;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return failures;
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
    if (expected.code != 0) {
        failures += expect_int(mylite_errcode(database), expected.code, sql);
    }
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
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
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(stderr, "%s: expected NULL at %zu,%zu, got %s\n", context, row, column, actual);
            return 1;
        }
        return 0;
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *tmpdir = getenv("TMPDIR");
    int written = 0;

    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_conv_function_%d_%s.mylite",
        tmpdir,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path too long\n");
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }
    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }
    return 0;
}
