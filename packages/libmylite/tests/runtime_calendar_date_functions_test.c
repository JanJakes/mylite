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
    invalid_datetime_warning_count = 6,
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

static int test_no_source_dual_and_do_calendar_date_functions(void);
static int test_calendar_date_warnings(void);
static int test_table_backed_calendar_date_functions_and_reopen(void);
static int test_calendar_date_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_error_len(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected
);
static int expect_query(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_calendar_date_functions();
    failures += test_calendar_date_warnings();
    failures += test_table_backed_calendar_date_functions_and_reopen();
    failures += test_calendar_date_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_calendar_date_functions(void) {
    static const char *const columns_core[] = {
        "DAYOFWEEK('2007-02-03')",
        "DAYOFYEAR('2007-02-03')",
        "LAST_DAY('2007-02-03')",
        "DAYOFWEEK('2008-01-02 13:29:17')",
        "DAYOFYEAR('2008-12-31 23:59:59')",
        "LAST_DAY('2008-02-03 13:29:17')",
        "DAYOFWEEK('0001-01-01')",
        "DAYOFWEEK('0999-12-31')",
        "DAYOFYEAR('0999-12-31')",
        "LAST_DAY('0000-02-01')",
        "DAYOFWEEK(NULL)",
        "DAYOFYEAR(NULL)",
        "LAST_DAY(NULL)",
    };
    static const char *const values_core[] = {
        "7",
        "34",
        "2007-02-28",
        "4",
        "366",
        "2008-02-29",
        "2",
        "3",
        "365",
        "0000-02-28",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_dual[] = {
        "DAYOFWEEK ('2007-02-03')",
        "doy",
        "month_end",
    };
    static const char *const values_dual[] = {"7", "34", "2007-02-28"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const columns_no_backslash[] = {
        "DAYOFWEEK('2007-02-03')",
        "DAYOFYEAR('2007-02-03')",
        "LAST_DAY('2007-02-03')",
    };
    static const char *const values_no_backslash[] = {"7", "34", "2007-02-28"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYOFWEEK('2007-02-03'), DAYOFYEAR('2007-02-03'), "
                   "LAST_DAY('2007-02-03'), DAYOFWEEK('2008-01-02 13:29:17'), "
                   "DAYOFYEAR('2008-12-31 23:59:59'), "
                   "LAST_DAY('2008-02-03 13:29:17'), DAYOFWEEK('0001-01-01'), "
                   "DAYOFWEEK('0999-12-31'), DAYOFYEAR('0999-12-31'), "
                   "LAST_DAY('0000-02-01'), DAYOFWEEK(NULL), DAYOFYEAR(NULL), "
                   "LAST_DAY(NULL)",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .context = "no-source calendar date functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYOFWEEK ('2007-02-03'), DAYOFYEAR ('2007-02-03') AS doy, "
                   "LAST_DAY ('2007-02-03') AS month_end FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual calendar date functions",
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
            .context = "row count after calendar date select",
        }
    );

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT DAYOFWEEK(\"2007-02-03\")",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYOFWEEK('2007-02-03'), DAYOFYEAR('2007-02-03'), "
                   "LAST_DAY('2007-02-03')",
            .columns = columns_no_backslash,
            .column_count = sizeof(columns_no_backslash) / sizeof(columns_no_backslash[0]),
            .values = values_no_backslash,
            .row_count = 1U,
            .context = "calendar date functions after NO_BACKSLASH_ESCAPES",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_ok(
        database,
        "DO DAYOFWEEK('2007-02-03'), DAYOFYEAR(NULL), LAST_DAY('2007-02-03')",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "calendar date do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "calendar date do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "calendar date do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "calendar date do warnings");
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
            .context = "row count after calendar date do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_calendar_date_warnings(void) {
    static const char *const columns_zero[] = {
        "dow_zero",
        "doy_partial",
        "last_partial",
        "dow_year_zero",
        "last_year_zero",
        "bad_year_zero_leap",
    };
    static const char *const values_zero[] = {
        NULL,
        NULL,
        "2001-11-30",
        "2",
        "0000-01-31",
        NULL,
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_zero_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-00-00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2001-11-00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-02-29'",
    };
    static const char *const values_zero_count[] = {"3"};
    static const char *const columns_invalid[] = {"dow_bad", "doy_bad", "last_bad"};
    static const char *const values_invalid[] = {NULL, NULL, NULL};
    static const char *const values_invalid_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
    };
    static const char *const values_invalid_count[] = {"3"};
    static const char *const columns_invalid_datetime[] = {
        "dow_24",
        "doy_24",
        "last_24",
        "dow_99",
        "doy_99",
        "last_99",
    };
    static const char *const values_invalid_datetime[] = {NULL, NULL, NULL, NULL, NULL, NULL};
    static const char *const values_invalid_datetime_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 24:00:00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 24:00:00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 24:00:00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 99:00:00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 99:00:00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 99:00:00'",
    };
    static const char *const values_invalid_datetime_count[] = {"6"};
    static const char *const columns_warning_count[] = {"@@warning_count"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "warnings", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYOFWEEK('0000-00-00') AS dow_zero, "
                   "DAYOFYEAR('2001-11-00') AS doy_partial, "
                   "LAST_DAY('2001-11-00') AS last_partial, "
                   "DAYOFWEEK('0000-01-02') AS dow_year_zero, "
                   "LAST_DAY('0000-01-02') AS last_year_zero, "
                   "LAST_DAY('0000-02-29') AS bad_year_zero_leap",
            .columns = columns_zero,
            .column_count = sizeof(columns_zero) / sizeof(columns_zero[0]),
            .values = values_zero,
            .row_count = 1U,
            .warning_count = 3U,
            .context = "calendar zero date values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_zero_warnings,
            .row_count = 3U,
            .context = "calendar zero warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = 1U,
            .values = values_zero_count,
            .row_count = 1U,
            .context = "calendar zero warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYOFWEEK('not-a-date') AS dow_bad, "
                   "DAYOFYEAR('not-a-date') AS doy_bad, LAST_DAY('not-a-date') AS last_bad",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 3U,
            .context = "calendar invalid strings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_warnings,
            .row_count = 3U,
            .context = "calendar invalid warnings",
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
            .context = "calendar invalid warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYOFWEEK('2008-01-02 24:00:00') AS dow_24, "
                   "DAYOFYEAR('2008-01-02 24:00:00') AS doy_24, "
                   "LAST_DAY('2008-01-02 24:00:00') AS last_24, "
                   "DAYOFWEEK('2008-01-02 99:00:00') AS dow_99, "
                   "DAYOFYEAR('2008-01-02 99:00:00') AS doy_99, "
                   "LAST_DAY('2008-01-02 99:00:00') AS last_99",
            .columns = columns_invalid_datetime,
            .column_count = sizeof(columns_invalid_datetime) / sizeof(columns_invalid_datetime[0]),
            .values = values_invalid_datetime,
            .row_count = 1U,
            .warning_count = invalid_datetime_warning_count,
            .context = "calendar invalid datetime time fields",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_datetime_warnings,
            .row_count = invalid_datetime_warning_count,
            .context = "calendar invalid datetime warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = 1U,
            .values = values_invalid_datetime_count,
            .row_count = 1U,
            .context = "calendar invalid datetime warning count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_calendar_date_functions_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "dow_d",
        "doy_dt",
        "last_dt",
        "last_ts",
        "dow_s",
        "doy_txt",
        "last_txt",
    };
    static const char *const values_table[] = {
        "1", "7",  "366", "2008-12-31", "2008-02-29", "7",  "34", "2008-02-29",
        "2", NULL, NULL,  NULL,         NULL,         NULL, NULL, NULL,
        "3", NULL, NULL,  "2001-11-30", NULL,         NULL, NULL, "2001-11-30",
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2001-11-00'",
    };
    static const char *const columns_warning_count[] = {"@@warning_count"};
    static const char *const values_warning_count[] = {"2"};
    static const char *const columns_limited[] = {"id", "last_dt"};
    static const char *const values_limited[] = {
        "3",
        "2001-11-30",
        "2",
        NULL,
    };
    static const char *const columns_reopen[] = {"last_ts"};
    static const char *const values_reopen[] = {"2008-02-29"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table-backed", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "
        "s VARCHAR(32), txt TEXT, tm TIME)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2007-02-03','2008-12-31 23:59:59','2008-02-03 13:29:17',"
        "'2007-02-03','2008-02-03','13:29:17'),"
        "(2,NULL,NULL,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00','2001-11-00 00:00:00',NULL,"
        "'not-a-date','2001-11-00','01:02:03')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DAYOFWEEK(d) AS dow_d, DAYOFYEAR(dt) AS doy_dt, "
                   "LAST_DAY(dt) AS last_dt, LAST_DAY(ts) AS last_ts, "
                   "DAYOFWEEK(s) AS dow_s, DAYOFYEAR(txt) AS doy_txt, "
                   "LAST_DAY(txt) AS last_txt FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "table-backed calendar date functions",
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
            .context = "table-backed calendar warnings",
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
            .context = "table-backed calendar warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, LAST_DAY(dt) AS last_dt FROM t WHERE id >= 1 ORDER BY id DESC "
                   "LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "calendar date row envelope",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen calendar database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_DAY(ts) AS last_ts FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = 1U,
            .values = values_reopen,
            .row_count = 1U,
            .context = "calendar date reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_calendar_date_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, d DATE, tm TIME)", NULL);
    failures += execute_error(
        database,
        "SELECT DAYOFWEEK()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DAYOFWEEK'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYOFYEAR('2007-02-03', 'x')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DAYOFYEAR'",
        }
    );
    failures += execute_error(
        database,
        "SELECT LAST_DAY()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'LAST_DAY'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYOFWEEK(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_error(
        database,
        "SELECT LAST_DAY(tm) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar date functions do not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYOFWEEK(20070203)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar date functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT LAST_DAY(DATE_ADD(d, INTERVAL 1 SECOND)) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar date functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYOFWEEK('2008\\0-01-01')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar date function literals do not support NUL bytes",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYOFWEEK('2008\\0-01-01') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar date function literals do not support NUL bytes",
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
    return execute_error_len(database, sql, strlen(sql), expected);
}

static int execute_error_len(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_length, &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%.*s: expected error, got success\n", (int)sql_length, sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "execute error");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "execute error");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "execute error");
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
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
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

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "SET sql_mode = ''", NULL);
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-calendar-date-functions-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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

    return expect_text(actual, expected, context);
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual
            );
            return 1;
        }
        return 0;
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
