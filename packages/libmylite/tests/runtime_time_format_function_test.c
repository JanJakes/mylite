#include <mylite/mylite.h>

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

static int test_no_source_dual_and_do_time_format(void);
static int test_table_backed_time_format_and_reopen(void);
static int test_time_format_warnings_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_error_bytes(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected,
    const char *context
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

    failures += test_no_source_dual_and_do_time_format();
    failures += test_table_backed_time_format_and_reopen();
    failures += test_time_format_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_time_format(void) {
    static const char *const columns_core[] = {
        "long_tokens",
        "negative_time",
        "datetime_time",
        "null_value",
        "null_format",
        "empty_format",
        "warnings",
    };
    static const char *const values_core[] = {
        "100|100|04|04|4|00|00|00|100:00:00|04:00:00 AM|AM|000000|%|q|%",
        "-01:02:03",
        "13:29:17",
        NULL,
        NULL,
        NULL,
        "0",
    };
    static const char *const columns_negative[] = {"sign_a", "sign_b", "sign_percent", "sign_q"};
    static const char *const values_negative[] = {"-x02x03x", "-02:03", "-%", "-q"};
    static const char *const columns_clock[] = {"am_midnight", "am_late", "pm_noon", "am_long"};
    static const char *const values_clock[] = {"00|12|AM", "11|11|AM", "12|12|PM", "24|12|AM"};
    static const char *const columns_dual[] = {"TIME_FORMAT ('01:02:03', '%H')", "quoted"};
    static const char *const values_dual[] = {"01", "01:02"};
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
            .sql = "SELECT TIME_FORMAT('100:00:00', "
                   "'%H|%k|%h|%I|%l|%i|%S|%s|%T|%r|%p|%f|%%|%q|%') AS "
                   "long_tokens, "
                   "TIME_FORMAT('-01:02:03', '%H:%i:%s') AS negative_time, "
                   "TIME_FORMAT('2008-01-02 13:29:17', '%H:%i:%s') AS datetime_time, "
                   "TIME_FORMAT(NULL, '%H') AS null_value, "
                   "TIME_FORMAT('01:02:03', NULL) AS null_format, "
                   "TIME_FORMAT('01:02:03', '') AS empty_format, "
                   "@@warning_count AS warnings",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source time_format core",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_FORMAT('-01:02:03', 'x%ix%sx') AS sign_a, "
                   "TIME_FORMAT('-01:02:03', '%i:%s') AS sign_b, "
                   "TIME_FORMAT('-01:02:03', '%%') AS sign_percent, "
                   "TIME_FORMAT('-01:02:03', '%q') AS sign_q",
            .columns = columns_negative,
            .column_count = sizeof(columns_negative) / sizeof(columns_negative[0]),
            .values = values_negative,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "negative time_format sign placement",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_FORMAT('00:00:00', '%H|%h|%p') AS am_midnight, "
                   "TIME_FORMAT('11:59:59', '%H|%h|%p') AS am_late, "
                   "TIME_FORMAT('12:00:00', '%H|%h|%p') AS pm_noon, "
                   "TIME_FORMAT('24:00:00', '%H|%h|%p') AS am_long",
            .columns = columns_clock,
            .column_count = sizeof(columns_clock) / sizeof(columns_clock[0]),
            .values = values_clock,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "time_format am pm",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_FORMAT ('01:02:03', '%H'), "
                   "TIME_FORMAT(\"01:02:03\", \"%H:%i\") AS quoted FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual time_format",
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
            .context = "row count after time_format select",
        }
    );

    failures +=
        execute_ok(database, "DO TIME_FORMAT('01:02:03', '%H'), TIME_FORMAT(NULL, '%H')", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "time_format do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "time_format do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "time_format do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "time_format do warnings");
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
            .context = "row count after time_format do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_time_format_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "time_text",
        "datetime_text",
        "timestamp_text",
        "date_text",
        "string_text",
    };
    static const char *const values_table[] = {
        "1",
        "01:02:03",
        "13.29",
        "13:29:17",
        "00:00:00",
        "04:05:06",
        "2",
        "100:00:00",
        "00.42",
        NULL,
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        "00:00:00",
        NULL,
    };
    static const char *const columns_limited[] = {"id", "formatted"};
    static const char *const values_limited[] = {"3", NULL, "2", "100:00"};
    static const char *const values_reopen[] = {"1", "01:02", "2", "100:00", "3", NULL};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE options("
        "id INT, tm TIME, dt DATETIME, ts TIMESTAMP NULL, d DATE, v VARCHAR(32)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO options VALUES "
        "(1, '01:02:03', '2008-01-02 13:29:17', '2008-01-02 13:29:17', "
        "'2008-01-02', '04:05:06'), "
        "(2, '100:00:00', '2008-01-02 00:42:00', NULL, NULL, 'bad'), "
        "(3, NULL, NULL, NULL, '2008-01-03', NULL)",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIME_FORMAT(tm, '%H:%i:%s') AS time_text, "
                   "TIME_FORMAT(dt, '%H.%i') AS datetime_text, "
                   "TIME_FORMAT(ts, '%T') AS timestamp_text, "
                   "TIME_FORMAT(d, '%H:%i:%s') AS date_text, "
                   "TIME_FORMAT(v, '%H:%i:%s') AS string_text "
                   "FROM options ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 1U,
            .context = "table time_format projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIME_FORMAT(tm, '%H:%i') AS formatted FROM options "
                   "WHERE id >= 1 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table time_format where order limit",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen time_format database");
    if (failures == 0) {
        failures += execute_ok(database, "USE app", NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIME_FORMAT(tm, '%H:%i') AS formatted FROM options ORDER BY id",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_reopen,
            .row_count = 3U,
            .warning_count = 0U,
            .context = "reopen time_format",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_time_format_warnings_and_diagnostics(void) {
    static const char embedded_nul_sql[] = "SELECT TIME_FORMAT('01:02:03', '%H\0:%i')";
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const invalid_time_warning_values[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'not-a-time'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(32), tm TIME)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1, '01:02:03', '01:02:03')", NULL);

    failures += execute_ok(database, "SELECT TIME_FORMAT('not-a-time', '%H:%i:%s')", &result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 1U, "invalid time_format column count");
        failures +=
            expect_size(mylite_result_row_count(result), 1U, "invalid time_format row count");
        failures += expect_result_value(result, 0U, 0U, NULL, "invalid time_format value");
        failures +=
            expect_size(mylite_result_warning_count(result), 1U, "invalid time_format warnings");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = 3U,
            .values = invalid_time_warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "invalid time_format warning diagnostics",
        }
    );

    failures += execute_ok(database, "SELECT TIME_FORMAT('2003-12-31', '%H:%i:%s')", &result);
    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            1U,
            "date string time_format column count"
        );
        failures +=
            expect_size(mylite_result_row_count(result), 1U, "date string time_format row count");
        failures += expect_result_value(result, 0U, 0U, NULL, "date string time_format value");
        failures += expect_size(
            mylite_result_warning_count(result),
            1U,
            "date string time_format warnings"
        );
    }
    mylite_result_free(result);

    failures += execute_error(
        database,
        "SELECT TIME_FORMAT()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'TIME_FORMAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_FORMAT('01:02:03')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'TIME_FORMAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_FORMAT('01:02:03', '%H', 'extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'TIME_FORMAT'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_FORMAT(missing, '%H') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_FORMAT(v, v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIME_FORMAT() supports only string format literals and NULL",
        }
    );
    failures += execute_error_bytes(
        database,
        embedded_nul_sql,
        sizeof(embedded_nul_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIME_FORMAT() literals do not support NUL bytes",
        },
        "embedded NUL TIME_FORMAT literal"
    );
    failures += execute_error(
        database,
        "SELECT TIME_FORMAT('01:02:03', '%Y')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIME_FORMAT() supports only time format specifiers",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_FORMAT(1, '%H')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, "
                            "TIMESTAMP, and NULL values",
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
    return execute_error_bytes(database, sql, strlen(sql), expected, sql);
}

static int execute_error_bytes(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    struct expected_sql_error expected,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_length, &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", context);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, context);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, context);
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
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-time-format-function-%s-%d.mylite",
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
