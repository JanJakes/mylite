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
    core_column_count = 15,
    boundary_column_count = 19,
    child_column_count = 14,
    diagnostic_column_count = 2,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
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

static int test_angle_values_and_file_safety(void);
static int test_angle_warnings_do_and_independent_handles(void);
static int test_angle_errors_and_unsupported_forms(void);
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

    failures += test_angle_values_and_file_safety();
    failures += test_angle_warnings_do_and_independent_handles();
    failures += test_angle_errors_and_unsupported_forms();

    return failures == 0 ? 0 : 1;
}

static int test_angle_values_and_file_safety(void) {
    static const char *const degree_columns[] = {
        "DEGREES(NULL)",
        "DEGREES(TRUE)",
        "DEGREES(FALSE)",
        "DEGREES(0)",
        "DEGREES(-0)",
        "DEGREES(+0)",
        "DEGREES(1)",
        "DEGREES(-1)",
        "DEGREES(2)",
        "DEGREES(3)",
        "DEGREES(90)",
        "DEGREES(180)",
        "DEGREES(360)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const degree_values[] = {
        NULL,
        "57.29577951308232",
        "0",
        "0",
        "0",
        "0",
        "57.29577951308232",
        "-57.29577951308232",
        "114.59155902616465",
        "171.88733853924697",
        "5156.620156177409",
        "10313.240312354817",
        "20626.480624709635",
        "0",
        "0",
    };
    static const char *const radian_columns[] = {
        "RADIANS(NULL)",
        "RADIANS(TRUE)",
        "RADIANS(FALSE)",
        "RADIANS(0)",
        "RADIANS(-0)",
        "RADIANS(+0)",
        "RADIANS(1)",
        "RADIANS(-1)",
        "RADIANS(2)",
        "RADIANS(3)",
        "RADIANS(90)",
        "RADIANS(180)",
        "RADIANS(360)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const radian_values[] = {
        NULL,
        "0.017453292519943295",
        "0",
        "0",
        "0",
        "0",
        "0.017453292519943295",
        "-0.017453292519943295",
        "0.03490658503988659",
        "0.05235987755982989",
        "1.5707963267948966",
        "3.141592653589793",
        "6.283185307179586",
        "0",
        "0",
    };
    static const char *const boundary_columns[] = {
        "DEGREES(9223372036854775807)",
        "DEGREES(9223372036854775808)",
        "DEGREES(18446744073709551615)",
        "DEGREES(-9223372036854775808)",
        "DEGREES(-18446744073709551615)",
        "RADIANS(9223372036854775807)",
        "RADIANS(9223372036854775808)",
        "RADIANS(18446744073709551615)",
        "RADIANS(-9223372036854775808)",
        "RADIANS(-18446744073709551615)",
        "DEGREES(100000000000000)",
        "RADIANS(100000000000000000)",
        "RADIANS(57295779513082320)",
        "RADIANS(57295779513082325)",
        "RADIANS(548826014839375280)",
        "RADIANS(1098524073332020107)",
        "RADIANS(1567766003366382319)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const boundary_values[] = {
        "5.2846029059076024e20",
        "5.2846029059076024e20",
        "1.0569205811815205e21",
        "-5.2846029059076024e20",
        "-1.0569205811815205e21",
        "1.6097821017949162e17",
        "1.6097821017949162e17",
        "3.2195642035898323e17",
        "-1.6097821017949162e17",
        "-3.2195642035898323e17",
        "5.729577951308232e15",
        "1745329251994329.5",
        "1e15",
        "1000000000000000.1",
        "9.578820979546356e15",
        "1.9172861992063388e16",
        "2.7362678659575876e16",
        "0",
        "0",
    };
    static const char *const child_columns[] = {
        "a",
        "b",
        "c",
        "d",
        "e",
        "f",
        "g",
        "h",
        "i",
        "j",
        "k",
        "l",
        "m",
        "n",
    };
    static const char *const child_values[] = {
        "57.29577951308232",
        "1.0569205811815205e21",
        "5.2846029059076024e20",
        "0",
        "114.59155902616465",
        "515.662015617741",
        NULL,
        "0.017453292519943295",
        "3.2195642035898323e17",
        "1.6097821017949162e17",
        "0",
        "0.03490658503988659",
        "0.15707963267948966",
        NULL,
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open angle values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DEGREES(NULL),DEGREES(TRUE),DEGREES(FALSE),DEGREES(0),"
                   "DEGREES(-0),DEGREES(+0),DEGREES(1),DEGREES(-1),DEGREES(2),"
                   "DEGREES(3),DEGREES(90),DEGREES(180),DEGREES(360),"
                   "@@warning_count,ROW_COUNT()",
            .columns = degree_columns,
            .column_count = core_column_count,
            .values = degree_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core degrees values",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT RADIANS(NULL),RADIANS(TRUE),RADIANS(FALSE),RADIANS(0),"
                   "RADIANS(-0),RADIANS(+0),RADIANS(1),RADIANS(-1),RADIANS(2),"
                   "RADIANS(3),RADIANS(90),RADIANS(180),RADIANS(360),"
                   "@@warning_count,ROW_COUNT()",
            .columns = radian_columns,
            .column_count = core_column_count,
            .values = radian_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core radians values",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DEGREES(9223372036854775807),DEGREES(9223372036854775808),"
                   "DEGREES(18446744073709551615),DEGREES(-9223372036854775808),"
                   "DEGREES(-18446744073709551615),RADIANS(9223372036854775807),"
                   "RADIANS(9223372036854775808),RADIANS(18446744073709551615),"
                   "RADIANS(-9223372036854775808),RADIANS(-18446744073709551615),"
                   "DEGREES(100000000000000),RADIANS(100000000000000000),"
                   "RADIANS(57295779513082320),RADIANS(57295779513082325),"
                   "RADIANS(548826014839375280),RADIANS(1098524073332020107),"
                   "RADIANS(1567766003366382319),"
                   "@@warning_count,ROW_COUNT()",
            .columns = boundary_columns,
            .column_count = boundary_column_count,
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "angle boundary values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DEGREES(5&3) a,DEGREES(~0) b,DEGREES(1<<63) c,"
                   "DEGREES(1<<64) d,DEGREES(5 DIV 2) e,DEGREES(IFNULL(NULL,9)) f,"
                   "DEGREES(NULLIF(1,1)) g,RADIANS(5&3) h,RADIANS(~0) i,"
                   "RADIANS(1<<63) j,RADIANS(1<<64) k,RADIANS(5 DIV 2) l,"
                   "RADIANS(IFNULL(NULL,9)) m,RADIANS(NULLIF(1,1)) n FROM DUAL",
            .columns = child_columns,
            .column_count = child_column_count,
            .values = child_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "angle child operands",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "angle catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "angle sqlite schema generation unchanged"
    );
    mylite_close(database);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "angle preamble");
    remove_related_files(path);
    return failures;
}

static int test_angle_warnings_do_and_independent_handles(void) {
    static const char *const warning_columns[] = {
        "DEGREES(5 DIV 0)",
        "RADIANS(5 DIV 0)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {NULL, NULL, "0", "0"};
    static const char *const diagnostic_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const no_warning_values[] = {"0", "0"};
    static const char *const do_warning_values[] = {"2", "0"};
    static const char *const conversion_columns[] = {"DEGREES(2)", "RADIANS(2)"};
    static const char *const conversion_values[] = {
        "114.59155902616465",
        "0.03490658503988659",
    };
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open angle warning handle");
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DEGREES(5 DIV 0),RADIANS(5 DIV 0),@@warning_count,ROW_COUNT()",
            .columns = warning_columns,
            .column_count = 4U,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 2U,
            .affected_rows = 0,
            .context = "angle select warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DO DEGREES(NULL),DEGREES(2),RADIANS(NULL),RADIANS(2)",
            .columns = NULL,
            .column_count = 0U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "angle do no warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = diagnostic_columns,
            .column_count = diagnostic_column_count,
            .values = no_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "angle do count snapshot",
        }
    );

    failures += execute_ok(database, "DO DEGREES(5 DIV 0),RADIANS(5 DIV 0)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "angle DO column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "angle DO row count");
        failures += expect_size(mylite_result_warning_count(result), 2U, "angle DO warning count");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "angle DO affected rows");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = diagnostic_columns,
            .column_count = diagnostic_column_count,
            .values = do_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "angle do warning snapshot",
        }
    );
    mylite_close(database);

    if (make_test_path(first_path, sizeof(first_path), "first_handle") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second_handle") != 0) {
        return failures + 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    failures +=
        expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first angle file handle");
    failures +=
        expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second angle file handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT DEGREES(2),RADIANS(2)",
            .columns = conversion_columns,
            .column_count = 2U,
            .values = conversion_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first angle handle",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT DEGREES(2),RADIANS(2)",
            .columns = conversion_columns,
            .column_count = 2U,
            .values = conversion_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second angle handle",
        }
    );
    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int test_angle_errors_and_unsupported_forms(void) {
    enum {
        show_warning_column_count = 3,
        warning_before_error_row_count = 2,
        diagnostic_status_column_count = 3,
    };

    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_before_error_rows[] = {
        "Warning",
        "1365",
        "Division by 0",
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
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open angle unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (4), (NULL)", NULL);

    failures += execute_error(
        database,
        "SELECT DEGREES()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DEGREES'",
        }
    );
    failures += execute_error(
        database,
        "DO DEGREES(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DEGREES'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'RADIANS'",
        }
    );
    failures += execute_error(
        database,
        "DO RADIANS(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'RADIANS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'DEGREES' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'RADIANS' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES(3037000500*3037000500)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS(3037000500*3037000500)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_error(
        database,
        "SELECT DEGREES(5 DIV 0),RADIANS(3037000500*3037000500)",
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
            .context = "angle SELECT warning before error diagnostics",
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
            .context = "angle SELECT warning before error status",
        }
    );
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_error(
        database,
        "DO DEGREES(5 DIV 0),RADIANS(3037000500*3037000500)",
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
            .context = "angle DO warning before error diagnostics",
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
            .context = "angle DO warning before error status",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES(18446744073709551616)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS(-18446744073709551616)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES('2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS(2.5)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES(1e1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS(X'10')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES(b'10')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES(1/1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS(@@warning_count)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DEGREES()/RADIANS() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES(?)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT DEGREES(id),RADIANS(id) FROM t ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+DEGREES(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT RADIANS(1)=0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT ABS(DEGREES(1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ABS() supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN TRUE THEN RADIANS(1) END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CASE supports",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
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
        "%s/mylite_degrees_radians_functions_%d_%s.mylite",
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
            "%s: expected text [%s], got [%s]\n",
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
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
