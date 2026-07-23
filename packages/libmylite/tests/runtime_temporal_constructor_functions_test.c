#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
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
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
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
    const char *context;
};

static int test_no_source_dual_and_do_temporal_constructors(void);
static int test_table_backed_temporal_constructors_and_reopen(void);
static int test_temporal_constructor_ranges_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_temporal_constructors();
    failures += test_table_backed_temporal_constructors_and_reopen();
    failures += test_temporal_constructor_ranges_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_temporal_constructors(void) {
    static const char *const columns_core[] = {
        "null_days",
        "zero_days",
        "first_day",
        "sample_day",
        "last_day",
        "leap_day",
        "rolled_day",
        "two_digit_low",
        "two_digit_high",
        "time_value",
        "negative_time",
        "null_time",
        "post_overflow_zero",
        "true_days",
        "false_days",
        "bool_date",
        "bool_time",
    };
    static const char *const values_core[] = {
        NULL,
        "0000-00-00",
        "0001-01-01",
        "2007-10-07",
        "9999-12-31",
        "2024-02-29",
        "2024-01-01",
        "2069-01-01",
        "1970-01-01",
        "01:02:03",
        "-01:02:03",
        NULL,
        "0000-00-00",
        "0000-00-00",
        "0000-00-00",
        "2000-01-01",
        "01:01:01",
    };
    static const char *const columns_dual[] = {"dt"};
    static const char *const values_dual[] = {"2024-12-31"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_DAYS(NULL) AS null_days, FROM_DAYS(0) AS zero_days, "
                   "FROM_DAYS(+366) AS first_day, FROM_DAYS(733321) AS sample_day, "
                   "FROM_DAYS(3652424) AS last_day, MAKEDATE(2024, 60) AS leap_day, "
                   "MAKEDATE(2023, 366) AS rolled_day, MAKEDATE(69, 1) AS two_digit_low, "
                   "MAKEDATE(70, 1) AS two_digit_high, MAKETIME(1, 2, 3) AS time_value, "
                   "MAKETIME(-1, 2, 3) AS negative_time, MAKETIME(1, 2, NULL) AS null_time, "
                   "FROM_DAYS(3652500) AS post_overflow_zero, FROM_DAYS(TRUE) AS true_days, "
                   "FROM_DAYS(FALSE) AS false_days, MAKEDATE(FALSE, TRUE) AS bool_date, "
                   "MAKETIME(TRUE, TRUE, TRUE) AS bool_time",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "temporal constructor no-source values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MAKEDATE (2024, 366) AS dt FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "temporal constructor dual",
        }
    );
    failures +=
        execute_ok(database, "DO FROM_DAYS(366), MAKEDATE(2024, 1), MAKETIME(1, 2, 3)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "temporal constructor do status",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_temporal_constructors_and_reopen(void) {
    static const char *const columns[] = {"id", "from_day", "made_date", "made_time"};
    static const char *const values[] = {
        "1",
        "0001-01-01",
        "2024-01-01",
        "01:02:03",
        "2",
        "2007-10-07",
        "2024-02-29",
        "-01:02:03",
        "3",
        "9999-12-31",
        NULL,
        NULL,
    };
    static const char *const warning_columns[] = {"overflow_day", "clipped_time"};
    static const char *const warning_values[] = {NULL, "838:59:59"};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_row_warnings[] = {
        "Warning",
        "1441",
        "Datetime function: from_days field overflow",
        "Warning",
        "1292",
        "Truncated incorrect time value: '839:00:00'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table-backed", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, day_no INT, year_no INT, doy INT, hour_no INT, minute_no INT, "
        "second_no INT, label VARCHAR(8))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, 366, 2024, 1, 1, 2, 3, 'one'), "
        "(2, 733321, 2024, 60, -1, 2, 3, 'two'), "
        "(3, 3652424, 9999, 366, 1, 60, 0, 'three')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FROM_DAYS(day_no) AS from_day, "
                   "MAKEDATE(year_no, doy) AS made_date, "
                   "MAKETIME(hour_no, minute_no, second_no) AS made_time "
                   "FROM t ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "temporal constructor table-backed values",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE warning_t(id INT, day_no INT, hour_no INT, minute_no INT, second_no INT)",
        NULL
    );
    failures += execute_ok(database, "INSERT INTO warning_t VALUES (1, 3652425, 839, 0, 0)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_DAYS(day_no) AS overflow_day, "
                   "MAKETIME(hour_no, minute_no, second_no) AS clipped_time "
                   "FROM warning_t",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "temporal constructor row-backed warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_row_warnings,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "temporal constructor row-backed warning rows",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FROM_DAYS(day_no) AS from_day, "
                   "MAKEDATE(year_no, doy) AS made_date, "
                   "MAKETIME(hour_no, minute_no, second_no) AS made_time "
                   "FROM t ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "temporal constructor reopen values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporal_constructor_ranges_and_diagnostics(void) {
    static const char *const columns_from_days[] = {"overflow_one", "overflow_last"};
    static const char *const values_from_days[] = {NULL, NULL};
    static const char *const columns_maketime[] = {"clip_pos", "clip_neg"};
    static const char *const values_maketime[] = {"838:59:59", "-838:59:59"};
    static const char *const columns_nulls[] = {"bad_minute", "bad_second", "bad_year"};
    static const char *const values_nulls[] = {NULL, NULL, NULL};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_from_days_warnings[] = {
        "Warning",
        "1441",
        "Datetime function: from_days field overflow",
        "Warning",
        "1441",
        "Datetime function: from_days field overflow",
    };
    static const char *const values_maketime_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '839:00:00'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '-839:00:00'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(8))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, 'text')", NULL);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT FROM_DAYS(3652425) AS overflow_one, "
                   "FROM_DAYS(3652499) AS overflow_last",
            .columns = columns_from_days,
            .column_count = sizeof(columns_from_days) / sizeof(columns_from_days[0]),
            .values = values_from_days,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "from_days overflow",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_from_days_warnings,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "from_days overflow warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MAKETIME(839, 0, 0) AS clip_pos, "
                   "MAKETIME(-839, 0, 0) AS clip_neg",
            .columns = columns_maketime,
            .column_count = sizeof(columns_maketime) / sizeof(columns_maketime[0]),
            .values = values_maketime,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "maketime clipping",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_maketime_warnings,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "maketime clipping warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MAKETIME(1, 60, 0) AS bad_minute, "
                   "MAKETIME(1, 0, 60) AS bad_second, "
                   "MAKEDATE(9999, 366) AS bad_year",
            .columns = columns_nulls,
            .column_count = sizeof(columns_nulls) / sizeof(columns_nulls[0]),
            .values = values_nulls,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "temporal constructor null ranges",
        }
    );

    failures += execute_error(
        database,
        "SELECT FROM_DAYS() AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'FROM_DAYS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_DAYS(1, 2) AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'FROM_DAYS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKEDATE(1) AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'MAKEDATE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKETIME(1, 2) AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'MAKETIME'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_DAYS('366')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FROM_DAYS() supports only signed integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKEDATE(2024, 1.9)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MAKEDATE() supports only signed integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT MAKETIME(1, 2, 3.5)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "MAKETIME() supports only signed integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_DAYS(9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "row-scalar SELECT integer literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_DAYS(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FROM_DAYS(v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "FROM_DAYS() supports only integer descriptor columns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
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

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
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
