#include "mylite_test_support.h"

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

static int test_no_source_dual_and_do_time_second_conversion(void);
static int test_table_backed_time_second_conversion_and_reopen(void);
static int test_time_second_conversion_warnings_and_diagnostics(void);
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

    failures += test_no_source_dual_and_do_time_second_conversion();
    failures += test_table_backed_time_second_conversion_and_reopen();
    failures += test_time_second_conversion_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_time_second_conversion(void) {
    static const char *const columns_core[] = {
        "long_time",
        "positive_time",
        "negative_time",
        "hundred_hours",
        "null_time",
        "positive_seconds",
        "signed_positive_seconds",
        "signed_negative_seconds",
        "true_seconds",
        "false_seconds",
        "null_seconds",
        "warnings",
    };
    static const char *const values_core[] = {
        "80580",
        "2378",
        "-2378",
        "360000",
        NULL,
        "00:39:38",
        "00:39:38",
        "-00:39:38",
        "00:00:01",
        "00:00:00",
        NULL,
        "0",
    };
    static const char *const columns_dual[] = {"secs", "tm"};
    static const char *const values_dual[] = {"2378", "00:39:38"};
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
            .sql = "SELECT TIME_TO_SEC('22:23:00') AS long_time, "
                   "TIME_TO_SEC('00:39:38') AS positive_time, "
                   "TIME_TO_SEC('-00:39:38') AS negative_time, "
                   "TIME_TO_SEC('100:00:00') AS hundred_hours, "
                   "TIME_TO_SEC(NULL) AS null_time, "
                   "SEC_TO_TIME(2378) AS positive_seconds, "
                   "SEC_TO_TIME(+2378) AS signed_positive_seconds, "
                   "SEC_TO_TIME(-2378) AS signed_negative_seconds, "
                   "SEC_TO_TIME(TRUE) AS true_seconds, "
                   "SEC_TO_TIME(FALSE) AS false_seconds, "
                   "SEC_TO_TIME(NULL) AS null_seconds, "
                   "@@warning_count AS warnings",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "no-source time second conversion core",
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
            .context = "row count after time second select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_TO_SEC ('00:39:38') AS secs, SEC_TO_TIME (2378) AS tm FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "dual time second conversion",
        }
    );

    failures += execute_ok(database, "DO TIME_TO_SEC('00:00:01'), SEC_TO_TIME(1)", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "time second do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "time second do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "time second do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "time second do warnings"
        );
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
            .context = "row count after time second do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_time_second_conversion_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "date_secs",
        "time_secs",
        "datetime_secs",
        "timestamp_secs",
        "string_secs",
        "seconds_time",
    };
    static const char *const values_table[] = {
        "1", "0",  "2378",    "3723", "3723", "2378",   "00:39:38",
        "2", NULL, "-360000", "0",    NULL,   NULL,     "-100:00:00",
        "3", "0",  NULL,      NULL,   NULL,   "360000", "838:59:59",
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_table_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'not-a-time'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '3020400'",
    };
    static const char *const columns_limited[] = {"id", "time_secs", "seconds_time"};
    static const char *const values_limited[] = {
        "2",
        "-360000",
        "-100:00:00",
        "1",
        "2378",
        "00:39:38",
    };
    static const char *const columns_reopen[] = {"time_secs", "seconds_time"};
    static const char *const values_reopen[] = {"2378", "00:39:38"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, d DATE NULL, tm TIME NULL, dt DATETIME NULL, "
        "ts TIMESTAMP NULL, v VARCHAR(32), secs INT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '2003-12-31', '00:39:38', '2003-12-31 01:02:03', "
        "'2003-12-31 01:02:03', '00:39:38', 2378), "
        "(2, NULL, '-100:00:00', '2003-12-31 00:00:00', NULL, "
        "'not-a-time', -360000), "
        "(3, '2003-12-31', NULL, NULL, NULL, '100:00:00', 3020400)",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIME_TO_SEC(d) AS date_secs, TIME_TO_SEC(tm) AS time_secs, "
                   "TIME_TO_SEC(dt) AS datetime_secs, TIME_TO_SEC(ts) AS timestamp_secs, "
                   "TIME_TO_SEC(v) AS string_secs, SEC_TO_TIME(secs) AS seconds_time "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "table time second conversion projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_table_warnings,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table time second warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIME_TO_SEC(tm) AS time_secs, SEC_TO_TIME(secs) AS seconds_time "
                   "FROM t WHERE id <= 2 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "table time second where order limit",
        }
    );

    mylite_close(database);
    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen time second");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_TO_SEC(tm) AS time_secs, SEC_TO_TIME(secs) AS seconds_time "
                   "FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "reopened time second conversion",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_time_second_conversion_warnings_and_diagnostics(void) {
    static const char *const columns_clipping[] = {"max_tm", "clipped", "neg_clipped"};
    static const char *const values_clipping[] = {"838:59:59", "838:59:59", "-838:59:59"};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_clipping_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '3020400'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '-3020400'",
    };
    static const char *const columns_invalid[] = {"invalid_time"};
    static const char *const values_invalid[] = {NULL};
    static const char *const columns_invalid_datetime[] = {"invalid_datetime"};
    static const char *const values_invalid_datetime[] = {NULL};
    static const char *const values_invalid_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'not-a-time'",
    };
    static const char *const values_invalid_datetime_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '2003-12-31 24:00:00'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, v VARCHAR(32))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, '00:00:01'), (2, '2003-12-31 24:00:00')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SEC_TO_TIME(3020399) AS max_tm, "
                   "SEC_TO_TIME(3020400) AS clipped, "
                   "SEC_TO_TIME(-3020400) AS neg_clipped",
            .columns = columns_clipping,
            .column_count = sizeof(columns_clipping) / sizeof(columns_clipping[0]),
            .values = values_clipping,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "sec_to_time clipping",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_clipping_warnings,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "sec_to_time clipping warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_TO_SEC('not-a-time') AS invalid_time",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid time_to_sec",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_warnings,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "invalid time_to_sec warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_TO_SEC('2003-12-31 24:00:00') AS invalid_datetime",
            .columns = columns_invalid_datetime,
            .column_count = sizeof(columns_invalid_datetime) / sizeof(columns_invalid_datetime[0]),
            .values = values_invalid_datetime,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid datetime time_to_sec",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_datetime_warnings,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "invalid datetime time_to_sec warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME_TO_SEC(v) AS invalid_datetime FROM t WHERE id = 2",
            .columns = columns_invalid_datetime,
            .column_count = sizeof(columns_invalid_datetime) / sizeof(columns_invalid_datetime[0]),
            .values = values_invalid_datetime,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "invalid descriptor datetime time_to_sec",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_datetime_warnings,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "invalid descriptor datetime time_to_sec warnings",
        }
    );

    failures += execute_error(
        database,
        "SELECT TIME_TO_SEC() AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'TIME_TO_SEC'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_TO_SEC('00:00:01', '00:00:02') AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'TIME_TO_SEC'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SEC_TO_TIME() AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'SEC_TO_TIME'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SEC_TO_TIME(1, 2) AS x",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'SEC_TO_TIME'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_TO_SEC(123456)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only string temporal literals, "
                            "descriptor columns, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT SEC_TO_TIME('2378')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SEC_TO_TIME() supports only signed integer",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_TO_SEC(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT SEC_TO_TIME(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME_TO_SEC(id) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only DATE, TIME, DATETIME, "
                            "TIMESTAMP, string, and NULL values",
        }
    );
    failures += execute_error(
        database,
        "SELECT SEC_TO_TIME(v) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SEC_TO_TIME() supports only integer descriptor columns",
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
