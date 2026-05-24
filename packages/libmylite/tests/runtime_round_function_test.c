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
    core_column_count = 13,
    dual_column_count = 5,
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

static int test_round_values_and_file_safety(void);
static int test_round_places_values(void);
static int test_round_warnings_and_do(void);
static int test_round_errors_and_unsupported_forms(void);
static int test_round_independent_handles(void);
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

    failures += test_round_values_and_file_safety();
    failures += test_round_places_values();
    failures += test_round_warnings_and_do();
    failures += test_round_errors_and_unsupported_forms();
    failures += test_round_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_round_values_and_file_safety(void) {
    static const char huge81[] =
        "999999999999999999999999999999999999999999999999999999999999999999999999999999999";
    static const char *const core_columns[] = {
        "ROUND(NULL)",
        "ROUND(TRUE)",
        "ROUND(FALSE)",
        "ROUND(0)",
        "ROUND(-0)",
        "ROUND(+0)",
        "ROUND(-1)",
        "ROUND(9223372036854775807)",
        "ROUND(-9223372036854775808)",
        "ROUND(9223372036854775808)",
        "ROUND(18446744073709551615)",
        "ROUND(-18446744073709551615)",
        "ROUND(999999999999999999999999999999999999999999999999999999999999999999999999999999999)",
    };
    static const char *const core_values[] = {
        NULL,
        "1",
        "0",
        "0",
        "0",
        "0",
        "-1",
        "9223372036854775807",
        "-9223372036854775808",
        "9223372036854775808",
        "18446744073709551615",
        "-18446744073709551615",
        huge81,
    };
    static const char *const dual_columns[] = {"a", "b", "c", "d", "e"};
    static const char *const dual_values[] = {
        "18446744073709551615",
        "0",
        "7",
        "2",
        "-7",
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
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROUND(NULL),ROUND(TRUE),ROUND(FALSE),ROUND(0),ROUND(-0),"
                   "ROUND(+0),ROUND(-1),ROUND(9223372036854775807),"
                   "ROUND(-9223372036854775808),ROUND(9223372036854775808),"
                   "ROUND(18446744073709551615),ROUND(-18446744073709551615),"
                   "ROUND(999999999999999999999999999999999999999999999999999999999999"
                   "999999999999999999999)",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "core ROUND values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROUND(~0) AS a,ROUND(1<<64) b,ROUND(1+2*3) c,"
                   "ROUND(5 DIV 2) d,ROUND(IFNULL(NULL,-7)) e FROM DUAL",
            .columns = dual_columns,
            .column_count = dual_column_count,
            .values = dual_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "dual ROUND operands",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "ROUND catalog generation unchanged"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "ROUND sqlite schema generation unchanged"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "ROUND preamble");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_round_places_values(void) {
    static const char *const places_columns[] = {
        "ROUND(123,0)",     "ROUND(123,2)",  "ROUND(123,-1)", "ROUND(999,-2)",   "ROUND(15,-1)",
        "ROUND(14,-1)",     "ROUND(5,-1)",   "ROUND(4,-1)",   "ROUND(-15,-1)",   "ROUND(-14,-1)",
        "ROUND(-5,-1)",     "ROUND(-4,-1)",  "ROUND(NULL,1)", "ROUND(123,NULL)", "ROUND(123,TRUE)",
        "ROUND(123,FALSE)", "ROUND(123,+2)", "ROUND(123,-0)", "ROUND(123,-31)",
    };
    static const char *const places_values[] = {
        "123", "123", "120", "1000", "20",  "10",  "10",  "0",   "-20", "-10",
        "-10", "0",   NULL,  NULL,   "123", "123", "123", "123", "0",
    };
    static const char *const boundary_columns[] = {
        "ROUND(9223372036854775804,-1)",
        "ROUND(-9223372036854775804,-1)",
        "ROUND(-9223372036854775808,-20)",
        "ROUND(1+2*3,-1)",
        "ROUND(5 DIV 2,-1)",
        "ROUND(1<<64,-1)",
    };
    static const char *const boundary_values[] = {
        "9223372036854775800",
        "-9223372036854775800",
        "0",
        "10",
        "0",
        "0",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open ROUND places handle");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROUND(123,0),ROUND(123,2),ROUND(123,-1),ROUND(999,-2),"
                   "ROUND(15,-1),ROUND(14,-1),ROUND(5,-1),ROUND(4,-1),"
                   "ROUND(-15,-1),ROUND(-14,-1),ROUND(-5,-1),ROUND(-4,-1),"
                   "ROUND(NULL,1),ROUND(123,NULL),ROUND(123,TRUE),ROUND(123,FALSE),"
                   "ROUND(123,+2),ROUND(123,-0),ROUND(123,-31)",
            .columns = places_columns,
            .column_count = sizeof(places_columns) / sizeof(places_columns[0]),
            .values = places_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "ROUND places values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROUND(9223372036854775804,-1),"
                   "ROUND(-9223372036854775804,-1),ROUND(-9223372036854775808,-20),"
                   "ROUND(1+2*3,-1),ROUND(5 DIV 2,-1),ROUND(1<<64,-1) FROM DUAL",
            .columns = boundary_columns,
            .column_count = sizeof(boundary_columns) / sizeof(boundary_columns[0]),
            .values = boundary_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "ROUND places signed boundaries and operands",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_round_warnings_and_do(void) {
    static const char *const warning_columns[] = {
        "ROUND(5 DIV 0)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {NULL, "0", "0"};
    static const char *const places_warning_columns[] = {
        "ROUND(5 DIV 0,NULL)",
        "ROUND(NULL,5 DIV 0)",
        "ROUND(5 DIV 0,5 DIV 0)",
        "@@warning_count",
    };
    static const char *const places_warning_values[] = {NULL, NULL, NULL, "0"};
    static const char *const count_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const select_warning_values[] = {"1", "-1"};
    static const char *const no_warning_values[] = {"0", "0"};
    static const char *const do_warning_values[] = {"1", "0"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open warning handle");
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROUND(5 DIV 0),@@warning_count,ROW_COUNT()",
            .columns = warning_columns,
            .column_count = 3U,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 1U,
            .affected_rows = 0,
            .context = "ROUND select warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = count_columns,
            .column_count = 2U,
            .values = select_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "ROUND select warning snapshot",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROUND(5 DIV 0,NULL),ROUND(NULL,5 DIV 0),"
                   "ROUND(5 DIV 0,5 DIV 0),@@warning_count",
            .columns = places_warning_columns,
            .column_count = sizeof(places_warning_columns) / sizeof(places_warning_columns[0]),
            .values = places_warning_values,
            .row_count = 1U,
            .warning_count = 4U,
            .affected_rows = 0,
            .context = "ROUND places warning staging",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DO ROUND(NULL),ROUND(7),ROUND(18446744073709551615)",
            .columns = NULL,
            .column_count = 0U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "ROUND do no warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = count_columns,
            .column_count = 2U,
            .values = no_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "ROUND do count snapshot",
        }
    );

    failures += execute_ok(database, "DO ROUND(5 DIV 0)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "do warning columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "do warning rows");
        failures += expect_size(mylite_result_warning_count(result), 1U, "do warning count");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "do warning affected");
    }
    mylite_result_free(result);
    failures += execute_ok(database, "DO ROUND(123,-1),ROUND(NULL,5 DIV 0)", &result);
    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, "do places columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "do places rows");
        failures += expect_size(mylite_result_warning_count(result), 1U, "do places warning count");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "do places affected");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count,ROW_COUNT()",
            .columns = count_columns,
            .column_count = 2U,
            .values = do_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "ROUND do places warning snapshot",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_round_errors_and_unsupported_forms(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1), (2), (NULL)", NULL);

    failures += execute_error(
        database,
        "SELECT ROUND()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(1,2,3)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "DO ROUND()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(3037000500*3037000500)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(9223372036854775805,-1)",
        (struct expected_sql_error){
            .code = mysql_error_bigint_out_of_range,
            .sqlstate = "22003",
            .message_part = "BIGINT value is out of range",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(9223372036854775808,-1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit negative-place rounding",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(~0,-1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit negative-place rounding",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(123,'2')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit negative-place rounding",
        }
    );
    failures += execute_error(
        database,
        "SELECT "
        "ROUND(9999999999999999999999999999999999999999999999999999999999999999999999999999999999)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "at most 81 significant decimal digits",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND('64')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CEIL()/CEILING()/FLOOR()/ROUND() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(5.5)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CEIL()/CEILING()/FLOOR()/ROUND() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(X'40')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CEIL()/CEILING()/FLOOR()/ROUND() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(b'1111')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CEIL()/CEILING()/FLOOR()/ROUND() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(1/1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CEIL()/CEILING()/FLOOR()/ROUND() support",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(id) FROM t ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT 1+ROUND(7)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT ROUND(7)=7",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "scalar projection supports",
        }
    );
    failures += execute_error(
        database,
        "SELECT CASE WHEN TRUE THEN ROUND(7) END",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT CASE supports",
        }
    );
    failures += execute_error(
        database,
        "DO ROUND(@@warning_count)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "CEIL()/CEILING()/FLOOR()/ROUND() support",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_round_independent_handles(void) {
    static const char *const first_columns[] = {"ROUND(~0)"};
    static const char *const first_values[] = {"18446744073709551615"};
    static const char *const second_columns[] = {"ROUND(-9223372036854775809)"};
    static const char *const second_values[] = {"-9223372036854775809"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first memory handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second memory handle");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT ROUND(~0)",
            .columns = first_columns,
            .column_count = 1U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "first handle ROUND",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT ROUND(-9223372036854775809)",
            .columns = second_columns,
            .column_count = 1U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .affected_rows = 0,
            .context = "second handle ROUND",
        }
    );

    mylite_close(first);
    mylite_close(second);
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
        "%s/mylite_round_function_%d_%s.mylite",
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
    const char *actual_text = actual == NULL ? "(null)" : actual;
    const char *expected_text = expected == NULL ? "(null)" : expected;

    if (actual == NULL || expected == NULL) {
        if (actual == expected) {
            return 0;
        }
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected_text, actual_text);
        return 1;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
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
