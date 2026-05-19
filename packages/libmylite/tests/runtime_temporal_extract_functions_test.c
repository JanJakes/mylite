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
    const char *context;
};

static int test_no_source_dual_and_do_temporal_extract(void);
static int test_table_backed_temporal_extract_and_reopen(void);
static int test_temporal_extract_diagnostics(void);
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

    failures += test_no_source_dual_and_do_temporal_extract();
    failures += test_table_backed_temporal_extract_and_reopen();
    failures += test_temporal_extract_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_temporal_extract(void) {
    static const char *const columns_core[] = {
        "DATE('2008-01-02 13:29:17')",
        "DATE('2008-01-02')",
        "DATE(NULL)",
        "TIME('2003-12-31 01:02:03')",
        "TIME('13:29:17')",
        "TIME('-13:29:17')",
        "TIME(\"-13:29:17\")",
        "TIME('-272:59:59')",
        "TIME('272:59:59')",
        "TIME(NULL)",
        "YEAR('2008-01-02 13:29:17')",
        "MONTH('2008-01-02 13:29:17')",
        "DAY('2008-01-02 13:29:17')",
        "DAYOFMONTH('2008-01-02 13:29:17')",
        "HOUR('2008-01-02 13:29:17')",
        "MINUTE('2008-01-02 13:29:17')",
        "SECOND('2008-01-02 13:29:17')",
        "HOUR('13:29:17')",
        "MINUTE('13:29:17')",
        "SECOND('13:29:17')",
        "HOUR('-13:29:17')",
        "HOUR('272:59:59')",
        "DATE('0000-00-00')",
        "DATE('0000-01-02')",
        "YEAR('0000-01-02')",
        "MONTH('0000-01-02')",
        "DAY('0000-01-02')",
        "YEAR('0000-00-00')",
        "MONTH('2008-00-00')",
        "DAY('2008-01-00')",
        "HOUR('0000-00-00 01:02:03')",
    };
    static const char *const values_core[] = {
        "2008-01-02", "2008-01-02", NULL,         "01:02:03", "13:29:17", "-13:29:17", "-13:29:17",
        "-272:59:59", "272:59:59",  NULL,         "2008",     "1",        "2",         "2",
        "13",         "29",         "17",         "13",       "29",       "17",        "13",
        "272",        "0000-00-00", "0000-01-02", "0",        "1",        "2",         "0",
        "0",          "0",          "1",
    };
    static const char *const columns_dual[] = {
        "DATE ('2008-01-02 13:29:17')",
        "tm",
        "yr",
        "hr",
    };
    static const char *const values_dual[] = {"2008-01-02", "13:29:17", "2008", "13"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
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
            .sql = "SELECT DATE('2008-01-02 13:29:17'), DATE('2008-01-02'), DATE(NULL), "
                   "TIME('2003-12-31 01:02:03'), TIME('13:29:17'), "
                   "TIME('-13:29:17'), TIME(\"-13:29:17\"), TIME('-272:59:59'), "
                   "TIME('272:59:59'), TIME(NULL), "
                   "YEAR('2008-01-02 13:29:17'), MONTH('2008-01-02 13:29:17'), "
                   "DAY('2008-01-02 13:29:17'), DAYOFMONTH('2008-01-02 13:29:17'), "
                   "HOUR('2008-01-02 13:29:17'), MINUTE('2008-01-02 13:29:17'), "
                   "SECOND('2008-01-02 13:29:17'), HOUR('13:29:17'), "
                   "MINUTE('13:29:17'), SECOND('13:29:17'), HOUR('-13:29:17'), "
                   "HOUR('272:59:59'), DATE('0000-00-00'), DATE('0000-01-02'), "
                   "YEAR('0000-01-02'), MONTH('0000-01-02'), DAY('0000-01-02'), "
                   "YEAR('0000-00-00'), MONTH('2008-00-00'), DAY('2008-01-00'), "
                   "HOUR('0000-00-00 01:02:03')",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .context = "no-source temporal extract",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE ('2008-01-02 13:29:17'), TIME ('13:29:17') AS tm, "
                   "YEAR ('2008-01-02') AS yr, HOUR ('13:29:17') AS hr FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual temporal extract",
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
            .context = "row count after temporal extract select",
        }
    );

    failures += execute_ok(
        database,
        "DO DATE('2008-01-02'), TIME('13:29:17'), YEAR(NULL), HOUR('13:29:17')",
        &result
    );
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 0U, "temporal extract do columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "temporal extract do rows");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "temporal extract do affected");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "temporal extract do warnings");
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
            .context = "row count after temporal extract do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_temporal_extract_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "date_text",
        "time_from_date",
        "time_from_datetime",
        "time_from_timestamp",
        "time_from_time",
        "year_text",
        "month_text",
        "day_text",
        "dayofmonth_text",
        "timestamp_year",
        "timestamp_hour",
        "hour_text",
        "minute_text",
        "second_text",
        "string_hour",
        "string_minute",
        "string_second",
    };
    static const char *const values_table[] = {
        "1",  "2008-01-02", "00:00:00", "13:29:17", "13:29:17", "13:29:17",  "2008", "1",  "2",
        "2",  "2008",       "13",       "13",       "29",       "17",        "13",   "29", "17",
        "2",  NULL,         NULL,       NULL,       NULL,       NULL,        NULL,   NULL, NULL,
        NULL, NULL,         NULL,       NULL,       NULL,       NULL,        NULL,   NULL, NULL,
        "3",  NULL,         "00:00:00", NULL,       NULL,       "-13:29:17", "0",    NULL, "0",
        NULL, NULL,         NULL,       "13",       "29",       "17",        NULL,   NULL, NULL,
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'not-a-date'",
        "Warning",
        "1292",
        "Truncated incorrect time value: 'not-a-date'",
        "Warning",
        "1292",
        "Truncated incorrect time value: 'not-a-date'",
    };
    static const char *const columns_warning_count[] = {"@@warning_count"};
    static const char *const values_warning_count[] = {"3"};
    static const char *const columns_order_limit[] = {"id", "TIME(tm)", "YEAR(d)", "HOUR(tm)"};
    static const char *const values_order_limit[] = {
        "3",
        "-13:29:17",
        "0",
        "13",
        "2",
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_reopen[] = {"YEAR(d)", "TIME(tm)", "HOUR(tm)"};
    static const char *const values_reopen[] = {"2008", "13:29:17", "13"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "
        "tm TIME NULL, s VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2008-01-02','2008-01-02 13:29:17','2008-01-02 13:29:17','13:29:17',"
        "'2008-01-02 13:29:17'),"
        "(2,NULL,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00',NULL,NULL,'-13:29:17','not-a-date')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DATE(dt) AS date_text, "
                   "TIME(d) AS time_from_date, TIME(dt) AS time_from_datetime, "
                   "TIME(ts) AS time_from_timestamp, TIME(tm) AS time_from_time, "
                   "YEAR(d) AS year_text, MONTH(dt) AS month_text, DAY(d) AS day_text, "
                   "DAYOFMONTH(dt) AS dayofmonth_text, YEAR(ts) AS timestamp_year, "
                   "HOUR(ts) AS timestamp_hour, HOUR(tm) AS hour_text, "
                   "MINUTE(tm) AS minute_text, SECOND(tm) AS second_text, HOUR(s) AS "
                   "string_hour, MINUTE(s) AS string_minute, SECOND(s) AS string_second "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table-backed temporal extract",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_warnings,
            .row_count = 3U,
            .context = "table-backed temporal extract warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = sizeof(columns_warning_count) / sizeof(columns_warning_count[0]),
            .values = values_warning_count,
            .row_count = 1U,
            .context = "table-backed temporal extract warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIME(tm), YEAR(d), HOUR(tm) FROM t WHERE id >= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_order_limit,
            .column_count = sizeof(columns_order_limit) / sizeof(columns_order_limit[0]),
            .values = values_order_limit,
            .row_count = 2U,
            .context = "table-backed temporal extract filtered ordered limited envelope",
        }
    );
    mylite_close(database);

    database = NULL;
    failures += mylite_open(path, &database) == MYLITE_OK ? 0 : 1;
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT YEAR(d), TIME(tm), HOUR(tm) FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopened temporal extract",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporal_extract_diagnostics(void) {
    enum { invalid_temporal_warning_count = 8 };

    static const char *const columns_invalid[] = {
        "DATE('not-a-date')",
        "TIME('not-a-date')",
        "YEAR('not-a-date')",
        "MONTH('not-a-date')",
        "DAY('not-a-date')",
        "HOUR('not-a-date')",
        "MINUTE('not-a-date')",
        "SECOND('not-a-date')",
    };
    static const char *const values_invalid[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
    };
    static const char *const columns_deferred_strings[] = {
        "TIME('2008-01-02')",
        "TIME('2008-01-02 13:29:17.999999')",
    };
    static const char *const values_deferred_strings[] = {NULL, NULL};
    static const char *const values_deferred_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '2008-01-02'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '2008-01-02 13:29:17.999999'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIME('2008-01-02'), TIME('2008-01-02 13:29:17.999999')",
            .columns = columns_deferred_strings,
            .column_count = sizeof(columns_deferred_strings) / sizeof(columns_deferred_strings[0]),
            .values = values_deferred_strings,
            .row_count = 1U,
            .context = "deferred time string coercion values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_deferred_warnings,
            .row_count = 2U,
            .context = "deferred time string coercion warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE('not-a-date'), TIME('not-a-date'), YEAR('not-a-date'), "
                   "MONTH('not-a-date'), DAY('not-a-date'), HOUR('not-a-date'), "
                   "MINUTE('not-a-date'), SECOND('not-a-date')",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .context = "invalid temporal extract values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_warnings,
            .row_count = invalid_temporal_warning_count,
            .context = "invalid temporal extract warnings",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE(20080102132917)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIME(20080102132917)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT YEAR('2008-01-02' + 0)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only string temporal literals",
        }
    );
    failures += execute_ok(database, "CREATE TABLE unsupported_types(id INT)", NULL);
    failures += execute_error(
        database,
        "SELECT HOUR(id) FROM unsupported_types",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only DATE, TIME, DATETIME, "
                            "TIMESTAMP, string, and NULL values",
        }
    );
    failures += execute_ok(database, "CREATE TABLE unsupported_strings(s VARCHAR(32))", NULL);
    failures += execute_error(
        database,
        "SELECT TIME(s) FROM unsupported_strings",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIME() does not yet support string descriptor columns",
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
        fprintf(
            stderr,
            "expected success for [%s], got %d: %s\n",
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(result);
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
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);
    size_t value_count = expected.row_count * expected.column_count;

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t index = 0U; failures == 0 && index < value_count; ++index) {
        failures += expect_result_value(
            result,
            index / expected.column_count,
            index % expected.column_count,
            expected.values[index],
            expected.context
        );
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
    int failures = make_test_path(path, path_size, name);

    if (failures == 0) {
        remove_related_files(path);
        failures += mylite_open(path, out_database) == MYLITE_OK ? 0 : 1;
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-temporal-extract-%s-%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
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
            fprintf(
                stderr,
                "%s: expected NULL at %zu,%zu, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
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
