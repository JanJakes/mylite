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

static int test_no_source_dual_and_do_timestamp(void);
static int test_table_backed_timestamp_and_reopen(void);
static int test_timestamp_warnings_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_no_source_dual_and_do_timestamp();
    failures += test_table_backed_timestamp_and_reopen();
    failures += test_timestamp_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_timestamp(void) {
    static const char *const columns_core[] = {
        "date_midnight",
        "datetime_same",
        "with_time",
        "with_days",
        "negative_time",
        "null_one",
        "null_two",
    };
    static const char *const values_core[] = {
        "2003-12-31 00:00:00",
        "2003-12-31 12:34:56",
        "2003-12-31 12:00:00",
        "2004-01-01 02:03:04",
        "2003-12-30 22:57:57",
        NULL,
        NULL,
    };
    static const char *const columns_dual[] = {"ts"};
    static const char *const values_dual[] = {"2003-12-31 00:00:00"};
    static const char *const columns_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('2003-12-31') AS date_midnight, "
                   "TIMESTAMP('2003-12-31 12:34:56') AS datetime_same, "
                   "TIMESTAMP('2003-12-31','12:00:00') AS with_time, "
                   "TIMESTAMP('2003-12-31','1 02:03:04') AS with_days, "
                   "TIMESTAMP('2003-12-31','-01:02:03') AS negative_time, "
                   "TIMESTAMP(NULL) AS null_one, "
                   "TIMESTAMP('2003-12-31', NULL) AS null_two",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .context = "no-source timestamp core",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP ('2003-12-31') AS ts FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual timestamp",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "timestamp select status",
        }
    );

    failures += execute_ok(
        database,
        "DO TIMESTAMP('2003-12-31'), TIMESTAMP('2003-12-31','00:00:01')",
        &result
    );
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "timestamp do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "timestamp do rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "timestamp do affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "timestamp do warnings");
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = columns_status,
            .column_count = sizeof(columns_status) / sizeof(columns_status[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "timestamp do status",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_timestamp_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "date_ts",
        "datetime_ts",
        "timestamp_ts",
        "string_ts",
        "date_plus_time",
        "datetime_plus_time",
    };
    static const char *const values_table[] = {
        "1",
        "2003-12-31 00:00:00",
        "2003-12-31 12:34:56",
        "2003-12-31 10:00:00",
        "2003-12-31 06:00:00",
        "2003-12-31 01:02:03",
        "2003-12-31 13:36:59",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_reopen[] = {"date_plus_time"};
    static const char *const values_reopen[] = {"2003-12-31 01:02:03"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE events("
        "id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "
        "tm TIME NULL, v VARCHAR(32)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO events VALUES "
        "(1, '2003-12-31', '2003-12-31 12:34:56', '2003-12-31 10:00:00', "
        "'01:02:03', '2003-12-31 06:00:00'), "
        "(2, NULL, NULL, NULL, NULL, NULL)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIMESTAMP(d) AS date_ts, "
                   "TIMESTAMP(dt) AS datetime_ts, TIMESTAMP(ts) AS timestamp_ts, "
                   "TIMESTAMP(v) AS string_ts, TIMESTAMP(d, tm) AS date_plus_time, "
                   "TIMESTAMP(dt, tm) AS datetime_plus_time "
                   "FROM events ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 2U,
            .context = "table timestamp projection",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen timestamp");
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SET time_zone = '+00:00'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP(d, tm) AS date_plus_time FROM events WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopened timestamp projection",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_timestamp_warnings_and_diagnostics(void) {
    static const char *const columns_invalid[] = {"bad_datetime"};
    static const char *const columns_null_short_circuit[] = {"null_short_circuit"};
    static const char *const values_invalid[] = {NULL};
    static const char *const columns_clipped[] = {"clipped_time"};
    static const char *const values_clipped[] = {"2004-02-03 22:59:59"};
    static const char *const columns_overflow[] = {"overflow_time"};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_invalid_datetime_warning[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
    };
    static const char *const values_invalid_time_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'bad'",
    };
    static const char *const values_clipped_time_warning[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '839:00:00'",
    };
    static const char *const values_overflow_warning[] = {
        "Warning",
        "1441",
        "Datetime function: add_time field overflow",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, d DATE NULL, tm TIME NULL, v VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, '2003-12-31', '01:02:03', '2003-12-31 06:00:00')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('bad') AS bad_datetime",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "timestamp invalid datetime",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_datetime_warning,
            .row_count = 1U,
            .context = "timestamp invalid datetime warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('bad', NULL) AS bad_datetime",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "timestamp invalid datetime before null time",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_datetime_warning,
            .row_count = 1U,
            .context = "timestamp invalid datetime before null time warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('bad', 'bad') AS bad_datetime",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "timestamp invalid datetime before invalid time",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_datetime_warning,
            .row_count = 1U,
            .context = "timestamp invalid datetime before invalid time warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('bad', '839:00:00') AS bad_datetime",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "timestamp invalid datetime before clipped time",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_datetime_warning,
            .row_count = 1U,
            .context = "timestamp invalid datetime before clipped time warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP(NULL, 'bad') AS null_short_circuit",
            .columns = columns_null_short_circuit,
            .column_count =
                sizeof(columns_null_short_circuit) / sizeof(columns_null_short_circuit[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "timestamp null first before invalid time",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('2003-12-31','bad') AS bad_datetime",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "timestamp invalid time",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_time_warning,
            .row_count = 1U,
            .context = "timestamp invalid time warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('2003-12-31','839:00:00') AS clipped_time",
            .columns = columns_clipped,
            .column_count = sizeof(columns_clipped) / sizeof(columns_clipped[0]),
            .values = values_clipped,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "timestamp clipped time",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_clipped_time_warning,
            .row_count = 1U,
            .context = "timestamp clipped time warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMP('9999-12-31 23:59:59','00:00:01') AS overflow_time",
            .columns = columns_overflow,
            .column_count = sizeof(columns_overflow) / sizeof(columns_overflow[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "timestamp overflow time",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_overflow_warning,
            .row_count = 1U,
            .context = "timestamp overflow warning",
        }
    );

    failures += execute_error(
        database,
        "SELECT TIMESTAMP()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'TIMESTAMP'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMP('2003-12-31', '00:00:00', 'extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'TIMESTAMP'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMP(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMP(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMP() supports only string temporal literals, DATE, "
                            "DATETIME, TIMESTAMP descriptor columns, string descriptor "
                            "columns, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMP('2003-12-31', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMP() time argument supports only string time literals, "
                            "TIME descriptor columns, string descriptor columns, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMP(tm) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMP() does not yet support TIME as first argument",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMP(d, missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
        "/tmp/mylite-timestamp-function-%s-%d.mylite",
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
