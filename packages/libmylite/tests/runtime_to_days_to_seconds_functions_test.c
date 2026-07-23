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
    invalid_temporal_warning_count = 9,
    invalid_string_warning_count = 4,
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

static int test_no_source_dual_and_do_to_days_to_seconds(void);
static int test_to_days_to_seconds_warnings(void);
static int test_table_backed_to_days_to_seconds_and_reopen(void);
static int test_to_days_to_seconds_diagnostics(void);
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

    failures += test_no_source_dual_and_do_to_days_to_seconds();
    failures += test_to_days_to_seconds_warnings();
    failures += test_table_backed_to_days_to_seconds_and_reopen();
    failures += test_to_days_to_seconds_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_to_days_to_seconds(void) {
    static const char *const columns_core[] = {
        "days_date",
        "days_datetime",
        "days_null",
        "day_one",
        "day_two",
        "feb_zero",
        "mar_zero",
        "year_one",
        "leap_day",
        "year_1000",
        "max_date",
        "seconds_date",
        "seconds_datetime",
        "seconds_leap_day",
        "seconds_leap_datetime",
    };
    static const char *const values_core[] = {
        "733321",
        "733321",
        NULL,
        "1",
        "2",
        "59",
        "60",
        "366",
        "733466",
        "365243",
        "3652424",
        "63358934400",
        "63358938123",
        "63371462400",
        "63371548799",
    };
    static const char *const columns_to_seconds[] = {
        "zero_day",
        "zero_second_day",
        "year_one_seconds",
        "sample_date",
        "sample_datetime",
        "null_seconds",
    };
    static const char *const values_to_seconds[] = {
        "86400",
        "172800",
        "31622400",
        "63426672000",
        "63426721412",
        NULL,
    };
    static const char *const columns_dual[] = {"TO_DAYS ('2007-10-07')", "sec_no"};
    static const char *const values_dual[] = {"733321", "63358938123"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_DAYS('2007-10-07') AS days_date, "
                   "TO_DAYS('2007-10-07 23:59:59') AS days_datetime, "
                   "TO_DAYS(NULL) AS days_null, TO_DAYS('0000-01-01') AS day_one, "
                   "TO_DAYS('0000-01-02') AS day_two, "
                   "TO_DAYS('0000-02-28') AS feb_zero, "
                   "TO_DAYS('0000-03-01') AS mar_zero, "
                   "TO_DAYS('0001-01-01') AS year_one, "
                   "TO_DAYS('2008-02-29') AS leap_day, "
                   "TO_DAYS('1000-01-01') AS year_1000, "
                   "TO_DAYS('9999-12-31') AS max_date, "
                   "TO_SECONDS('2007-10-07') AS seconds_date, "
                   "TO_SECONDS('2007-10-07 01:02:03') AS seconds_datetime, "
                   "TO_SECONDS('2008-02-29') AS seconds_leap_day, "
                   "TO_SECONDS('2008-02-29 23:59:59') AS seconds_leap_datetime",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source to_days to_seconds core",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_SECONDS('0000-01-01') AS zero_day, "
                   "TO_SECONDS('0000-01-02') AS zero_second_day, "
                   "TO_SECONDS('0001-01-01') AS year_one_seconds, "
                   "TO_SECONDS('2009-11-29') AS sample_date, "
                   "TO_SECONDS('2009-11-29 13:43:32') AS sample_datetime, "
                   "TO_SECONDS(NULL) AS null_seconds",
            .columns = columns_to_seconds,
            .column_count = sizeof(columns_to_seconds) / sizeof(columns_to_seconds[0]),
            .values = values_to_seconds,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "to_seconds sample values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row count after to_days select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_DAYS ('2007-10-07'), "
                   "TO_SECONDS('2007-10-07 01:02:03') AS sec_no FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual to_days to_seconds",
        }
    );

    failures += execute_ok(database, "DO TO_DAYS('2007-10-07'), TO_SECONDS('2007-10-07')", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "to_days do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "to_days do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "to_days do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "to_days do warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_row_status,
            .column_count = sizeof(columns_row_status) / sizeof(columns_row_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row count after to_days do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_to_days_to_seconds_warnings(void) {
    static const char *const columns_invalid[] = {
        "zero_days",
        "partial_days",
        "bad_month",
        "zero_seconds",
        "partial_seconds",
        "bad_time",
        "nonleap_days",
        "zero_nonleap_days",
        "nonleap_seconds",
    };
    static const char *const values_invalid[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_invalid_warnings[] = {
        "Warning", "1292", "Incorrect datetime value: '0000-00-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-11-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-00-01'",
        "Warning", "1292", "Incorrect datetime value: '0000-00-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-11-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-11-01 24:00:00'",
        "Warning", "1292", "Incorrect datetime value: '2007-02-29'",
        "Warning", "1292", "Incorrect datetime value: '0000-02-29'",
        "Warning", "1292", "Incorrect datetime value: '2007-02-29'",
    };
    static const char *const columns_invalid_strings[] = {
        "bad_days",
        "bad_seconds",
        "time_days",
        "time_seconds",
    };
    static const char *const values_invalid_strings[] = {NULL, NULL, NULL, NULL};
    static const char *const values_invalid_string_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
        "Warning",
        "1292",
        "Incorrect datetime value: '13:29:17'",
        "Warning",
        "1292",
        "Incorrect datetime value: '13:29:17'",
    };
    static const char *const columns_warning_count[] = {"@@warning_count"};
    static const char *const values_invalid_count[] = {"9"};
    static const char *const values_invalid_string_count[] = {"4"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "warnings", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_DAYS('0000-00-00') AS zero_days, "
                   "TO_DAYS('2001-11-00') AS partial_days, "
                   "TO_DAYS('2001-00-01') AS bad_month, "
                   "TO_SECONDS('0000-00-00') AS zero_seconds, "
                   "TO_SECONDS('2001-11-00') AS partial_seconds, "
                   "TO_SECONDS('2001-11-01 24:00:00') AS bad_time, "
                   "TO_DAYS('2007-02-29') AS nonleap_days, "
                   "TO_DAYS('0000-02-29') AS zero_nonleap_days, "
                   "TO_SECONDS('2007-02-29') AS nonleap_seconds",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = invalid_temporal_warning_count,
            .context = "invalid to_days to_seconds zero values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_warnings,
            .row_count = invalid_temporal_warning_count,
            .context = "invalid to_days warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = 1U,
            .values = values_invalid_count,
            .row_count = 1U,
            .context = "invalid to_days warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_DAYS('not-a-date') AS bad_days, "
                   "TO_SECONDS('not-a-date') AS bad_seconds, "
                   "TO_DAYS('13:29:17') AS time_days, "
                   "TO_SECONDS('13:29:17') AS time_seconds",
            .columns = columns_invalid_strings,
            .column_count = sizeof(columns_invalid_strings) / sizeof(columns_invalid_strings[0]),
            .values = values_invalid_strings,
            .row_count = 1U,
            .warning_count = invalid_string_warning_count,
            .context = "invalid to_days strings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_string_warnings,
            .row_count = invalid_string_warning_count,
            .context = "invalid to_days string warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = 1U,
            .values = values_invalid_string_count,
            .row_count = 1U,
            .context = "invalid to_days string warning count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_to_days_to_seconds_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "days_d",
        "days_dt",
        "days_ts",
        "seconds_d",
        "seconds_dt",
        "seconds_ts",
        "days_s",
        "seconds_s",
    };
    static const char *const values_table[] = {
        "1",      "733321",      "733321", "733321", "63358934400", "63359020799", "63358938123",
        "733321", "63358949106", "2",      NULL,     NULL,          NULL,          NULL,
        NULL,     NULL,          NULL,     NULL,     "3",           NULL,          NULL,
        NULL,     NULL,          NULL,     NULL,     NULL,          NULL,
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
    };
    static const char *const columns_warning_count[] = {"@@warning_count"};
    static const char *const values_warning_count[] = {"2"};
    static const char *const columns_limited[] = {"id", "days_dt", "seconds_d"};
    static const char *const values_limited[] = {"1", "733321", "63358934400"};
    static const char *const columns_reopen[] = {"days_dt", "days_ts", "seconds_d"};
    static const char *const values_reopen[] = {"733321", "733321", "63358934400"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, s VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2007-10-07','2007-10-07 23:59:59','2007-10-07 01:02:03',"
        "'2007-10-07 04:05:06'),"
        "(2,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00','2001-11-00 00:00:00','2001-11-00 00:00:00','not-a-date')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT id, TO_DAYS(d) AS days_d, TO_DAYS(dt) AS days_dt, "
                "TO_DAYS(ts) AS days_ts, TO_SECONDS(d) AS seconds_d, "
                "TO_SECONDS(dt) AS seconds_dt, TO_SECONDS(ts) AS seconds_ts, TO_DAYS(s) AS days_s, "
                "TO_SECONDS(s) AS seconds_s FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "table-backed to_days to_seconds",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_warnings,
            .row_count = 2U,
            .context = "table-backed to_days warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = 1U,
            .values = values_warning_count,
            .row_count = 1U,
            .context = "table-backed to_days warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TO_DAYS(dt) AS days_dt, TO_SECONDS(d) AS seconds_d FROM t "
                   "WHERE id IN (1, 3) ORDER BY id LIMIT 1",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "to_days row envelope",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen to_days");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TO_DAYS(dt) AS days_dt, TO_DAYS(ts) AS days_ts, "
                   "TO_SECONDS(d) AS seconds_d FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "reopened to_days to_seconds",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_to_days_to_seconds_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, d DATE, tm TIME)", NULL);
    failures += execute_error(
        database,
        "SELECT TO_DAYS()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'TO_DAYS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_DAYS('2007-10-07', 1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'TO_DAYS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_SECONDS()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'TO_SECONDS'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_SECONDS('2007-10-07', 1)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'TO_SECONDS'",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT TO_DAYS(\"2007-10-07\")",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += execute_error(
        database,
        "SELECT TO_DAYS(20071007)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar date functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_DAYS(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_SECONDS(tm) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar date functions do not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT TO_SECONDS(id) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "calendar date functions support only string temporal literals, DATE, DATETIME, "
                "TIMESTAMP descriptor columns, string descriptor columns, and NULL",
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
    if (failures == 0) {
        failures += execute_ok(*out_database, "SET SESSION sql_mode = ''", NULL);
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
