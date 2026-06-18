#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
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

struct update_case {
    const char *label;
    const char *sql;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_temporal_row_scalar_update_contexts(void);
static int test_temporal_row_scalar_update_rejects_joined_update(void);
static int open_temporal_update_context_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, struct expected_dml_result expected);
static int expect_query(mylite_db *database, struct expected_query expected);
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

    failures += test_temporal_row_scalar_update_contexts();
    failures += test_temporal_row_scalar_update_rejects_joined_update();

    return failures == 0 ? 0 : 1;
}

static int test_temporal_row_scalar_update_contexts(void) {
    static const struct update_case updates[] = {
        {"date", "UPDATE t SET out_date = DATE(dt)"},
        {"time", "UPDATE t SET out_time = TIME(tm)"},
        {"date_format", "UPDATE t SET out_date_format = DATE_FORMAT(dt, '%Y-%m')"},
        {"time_format", "UPDATE t SET out_time_format = TIME_FORMAT(tm, '%H:%i')"},
        {"str_to_date", "UPDATE t SET out_str_to_date = STR_TO_DATE('2009-01-02', '%Y-%m-%d')"},
        {"datediff", "UPDATE t SET out_datediff = DATEDIFF(dt, d)"},
        {"timediff", "UPDATE t SET out_timediff = TIMEDIFF('03:04:05', tm)"},
        {"timestampdiff", "UPDATE t SET out_timestampdiff = TIMESTAMPDIFF(DAY, d, dt)"},
        {"timestamp", "UPDATE t SET out_timestamp = TIMESTAMP(d, tm)"},
        {"unix_timestamp", "UPDATE t SET out_unix_timestamp = UNIX_TIMESTAMP(dt)"},
        {"sec_to_time", "UPDATE t SET out_sec_to_time = SEC_TO_TIME(epoch)"},
        {"from_unixtime", "UPDATE t SET out_from_unixtime = FROM_UNIXTIME(epoch)"},
        {"from_days", "UPDATE t SET out_from_days = FROM_DAYS(daynr)"},
        {"makedate", "UPDATE t SET out_makedate = MAKEDATE(y, doy)"},
        {"maketime", "UPDATE t SET out_maketime = MAKETIME(h, mi, sec)"},
        {"time_to_sec", "UPDATE t SET out_time_to_sec = TIME_TO_SEC(tm)"},
        {"to_days", "UPDATE t SET out_to_days = TO_DAYS(d)"},
        {"to_seconds", "UPDATE t SET out_to_seconds = TO_SECONDS(dt)"},
        {"dayofmonth", "UPDATE t SET out_dayofmonth = DAYOFMONTH(d)"},
        {"dayofweek", "UPDATE t SET out_dayofweek = DAYOFWEEK(d)"},
        {"dayofyear", "UPDATE t SET out_dayofyear = DAYOFYEAR(d)"},
        {"dayname", "UPDATE t SET out_dayname = DAYNAME(d)"},
        {"last_day", "UPDATE t SET out_last_day = LAST_DAY(d)"},
        {"hour", "UPDATE t SET out_hour = HOUR(tm)"},
        {"minute", "UPDATE t SET out_minute = MINUTE(tm)"},
        {"second", "UPDATE t SET out_second = SECOND(tm)"},
        {"microsecond", "UPDATE t SET out_microsecond = MICROSECOND(s)"},
        {"month", "UPDATE t SET out_month = MONTH(d)"},
        {"monthname", "UPDATE t SET out_monthname = MONTHNAME(d)"},
        {"quarter", "UPDATE t SET out_quarter = QUARTER(d)"},
        {"week", "UPDATE t SET out_week = WEEK(d)"},
        {"weekday", "UPDATE t SET out_weekday = WEEKDAY(d)"},
        {"weekofyear", "UPDATE t SET out_weekofyear = WEEKOFYEAR(d)"},
        {"year", "UPDATE t SET out_year = YEAR(d)"},
        {"yearweek", "UPDATE t SET out_yearweek = YEARWEEK(d)"},
        {"extract", "UPDATE t SET out_extract = EXTRACT(YEAR_MONTH FROM d)"},
        {"timestampadd", "UPDATE t SET out_timestampadd = TIMESTAMPADD(SECOND, 3661, dt)"},
    };
    static const char *const columns[] = {
        "id",
        "out_date",
        "out_time",
        "out_date_format",
        "out_time_format",
        "out_str_to_date",
        "out_datediff",
        "out_timediff",
        "out_timestampdiff",
        "out_timestamp",
        "out_unix_timestamp",
        "out_sec_to_time",
        "out_from_unixtime",
        "out_from_days",
        "out_makedate",
        "out_maketime",
        "out_time_to_sec",
        "out_to_days",
        "out_to_seconds",
        "out_dayofmonth",
        "out_dayofweek",
        "out_dayofyear",
        "out_dayname",
        "out_last_day",
        "out_hour",
        "out_minute",
        "out_second",
        "out_microsecond",
        "out_month",
        "out_monthname",
        "out_quarter",
        "out_week",
        "out_weekday",
        "out_weekofyear",
        "out_year",
        "out_yearweek",
        "out_extract",
        "out_timestampadd",
    };
    static const char *const values[] = {
        "1",
        "2008-12-31",
        "01:02:03",
        "2008-12",
        "01:02",
        "2009-01-02",
        "315",
        "02:02:02",
        "315",
        "2008-02-20 01:02:03",
        "1230767999",
        "01:01:01",
        "1970-01-01 01:01:01",
        "2000-07-03",
        "2024-02-29",
        "12:34:56",
        "3723",
        "733457",
        "63397987199",
        "20",
        "4",
        "51",
        "Wednesday",
        "2008-02-29",
        "1",
        "2",
        "3",
        "456789",
        "2",
        "February",
        "1",
        "7",
        "2",
        "8",
        "2008",
        "200807",
        "200802",
        "2009-01-01 01:01:00",
    };
    static const char *const status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const status_values[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_temporal_update_context_database(&database, "assignment", path, sizeof(path));
    for (size_t index = 0U; failures == 0 && index < sizeof(updates) / sizeof(updates[0]);
         ++index) {
        failures += expect_dml_ok(
            database,
            updates[index].sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
        );
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id,out_date,out_time,out_date_format,out_time_format,out_str_to_date,"
                   "out_datediff,out_timediff,out_timestampdiff,out_timestamp,"
                   "out_unix_timestamp,out_sec_to_time,out_from_unixtime,out_from_days,"
                   "out_makedate,out_maketime,out_time_to_sec,out_to_days,out_to_seconds,"
                   "out_dayofmonth,out_dayofweek,out_dayofyear,out_dayname,out_last_day,"
                   "out_hour,out_minute,out_second,out_microsecond,out_month,out_monthname,"
                   "out_quarter,out_week,out_weekday,out_weekofyear,out_year,out_yearweek,"
                   "out_extract,out_timestampadd FROM t",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "temporal row-scalar update assignments",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = status_columns,
            .column_count = sizeof(status_columns) / sizeof(status_columns[0]),
            .values = status_values,
            .row_count = 1U,
            .context = "temporal row-scalar status after select",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporal_row_scalar_update_rejects_joined_update(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_temporal_update_context_database(&database, "joined", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE joined_source(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO joined_source VALUES (1)", NULL);
    failures += execute_error(
        database,
        "UPDATE t JOIN joined_source ON t.id = joined_source.id "
        "SET t.out_date = DATE(t.dt)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_temporal_update_context_database(
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
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open database");
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "SET time_zone = '+00:00'", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(
            *out_database,
            "CREATE TABLE t("
            "id INT, d DATE, tm TIME, dt DATETIME, s VARCHAR(64), epoch INT, daynr INT, "
            "y INT, doy INT, h INT, mi INT, sec INT, out_date VARCHAR(64), "
            "out_time VARCHAR(64), out_date_format VARCHAR(64), out_time_format VARCHAR(64), "
            "out_str_to_date VARCHAR(64), out_datediff INT, out_timediff VARCHAR(64), "
            "out_timestampdiff INT, out_timestamp VARCHAR(64), out_unix_timestamp INT, "
            "out_sec_to_time VARCHAR(64), out_from_unixtime VARCHAR(64), "
            "out_from_days VARCHAR(64), out_makedate VARCHAR(64), out_maketime VARCHAR(64), "
            "out_time_to_sec INT, out_to_days INT, out_to_seconds BIGINT, "
            "out_dayofmonth INT, out_dayofweek INT, out_dayofyear INT, out_dayname VARCHAR(64), "
            "out_last_day VARCHAR(64), out_hour INT, out_minute INT, out_second INT, "
            "out_microsecond INT, out_month INT, out_monthname VARCHAR(64), out_quarter INT, "
            "out_week INT, out_weekday INT, out_weekofyear INT, out_year INT, "
            "out_yearweek INT, out_extract INT, out_timestampadd VARCHAR(64))",
            NULL
        );
    }
    if (failures == 0) {
        failures += expect_dml_ok(
            *out_database,
            "INSERT INTO t(id,d,tm,dt,s,epoch,daynr,y,doy,h,mi,sec) VALUES"
            "(1,'2008-02-20','01:02:03','2008-12-31 23:59:59',"
            "'2009-01-02 03:04:05.456789',3661,730669,2024,60,12,34,56)",
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 0U}
        );
    }
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-temporal-row-scalar-update-contexts-%s-%d.mylite",
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char full_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(full_path, sizeof(full_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(full_path)) {
        (void)remove(full_path);
    }
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *local = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &local);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got %d: %s\n",
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(local);
        return 1;
    }
    if (out_result != NULL) {
        *out_result = local;
    } else {
        mylite_result_free(local);
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

static int expect_dml_ok(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            expected.column_count,
            expected.context
        );
        failures +=
            expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; failures == 0 && row < expected.row_count; ++row) {
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

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s row %zu column %zu: expected NULL, got [%s]\n",
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
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
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
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
