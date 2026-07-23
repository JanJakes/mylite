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
    const char *context;
};

static int test_no_source_dual_and_do_temporal_extract(void);
static int test_table_backed_temporal_extract_and_reopen(void);
static int test_temporal_extract_predicates(void);
static int test_temporal_extract_diagnostics(void);
static int test_extract_function(void);
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

    failures += test_no_source_dual_and_do_temporal_extract();
    failures += test_table_backed_temporal_extract_and_reopen();
    failures += test_temporal_extract_predicates();
    failures += test_temporal_extract_diagnostics();
    failures += test_extract_function();

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
        "QUARTER('2008-01-02')",
        "QUARTER('2008-04-01')",
        "QUARTER('2008-07-01')",
        "QUARTER('2008-10-01')",
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
        "MICROSECOND('12:00:00.123456')",
        "MICROSECOND('12:00:00.1')",
        "MICROSECOND('12:00:00.1234567')",
        "MICROSECOND('12:00:00.9999995')",
        "MICROSECOND('2019-12-31 23:59:59.000010')",
        "MICROSECOND(NULL)",
        "DATE('0000-00-00')",
        "DATE('0000-01-02')",
        "YEAR('0000-01-02')",
        "QUARTER('0000-00-00')",
        "QUARTER('0000-01-02')",
        "QUARTER('2008-00-00')",
        "QUARTER('2001-11-00')",
        "MONTH('0000-01-02')",
        "DAY('0000-01-02')",
        "YEAR('0000-00-00')",
        "MONTH('2008-00-00')",
        "DAY('2008-01-00')",
        "HOUR('0000-00-00 01:02:03')",
    };
    static const char *const values_core[] = {
        "2008-01-02", "2008-01-02", NULL, "01:02:03", "13:29:17",   "-13:29:17",  "-13:29:17",
        "-272:59:59", "272:59:59",  NULL, "2008",     "1",          "1",          "2",
        "3",          "4",          "2",  "2",        "13",         "29",         "17",
        "13",         "29",         "17", "13",       "272",        "123456",     "100000",
        "123457",     "0",          "10", NULL,       "0000-00-00", "0000-01-02", "0",
        "0",          "1",          "0",  "4",        "1",          "2",          "0",
        "0",          "0",          "1",
    };
    static const char *const columns_dual[] = {
        "DATE ('2008-01-02 13:29:17')",
        "tm",
        "yr",
        "qtr",
        "hr",
    };
    static const char *const values_dual[] = {"2008-01-02", "13:29:17", "2008", "2", "13"};
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
                   "QUARTER('2008-01-02'), QUARTER('2008-04-01'), "
                   "QUARTER('2008-07-01'), QUARTER('2008-10-01'), "
                   "DAY('2008-01-02 13:29:17'), DAYOFMONTH('2008-01-02 13:29:17'), "
                   "HOUR('2008-01-02 13:29:17'), MINUTE('2008-01-02 13:29:17'), "
                   "SECOND('2008-01-02 13:29:17'), HOUR('13:29:17'), "
                   "MINUTE('13:29:17'), SECOND('13:29:17'), HOUR('-13:29:17'), "
                   "HOUR('272:59:59'), MICROSECOND('12:00:00.123456'), "
                   "MICROSECOND('12:00:00.1'), MICROSECOND('12:00:00.1234567'), "
                   "MICROSECOND('12:00:00.9999995'), "
                   "MICROSECOND('2019-12-31 23:59:59.000010'), MICROSECOND(NULL), "
                   "DATE('0000-00-00'), DATE('0000-01-02'), "
                   "YEAR('0000-01-02'), QUARTER('0000-00-00'), "
                   "QUARTER('0000-01-02'), QUARTER('2008-00-00'), "
                   "QUARTER('2001-11-00'), MONTH('0000-01-02'), DAY('0000-01-02'), "
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
                   "YEAR ('2008-01-02') AS yr, QUARTER ('2008-04-01') AS qtr, "
                   "HOUR ('13:29:17') AS hr FROM DUAL",
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
        "DO DATE('2008-01-02'), TIME('13:29:17'), YEAR(NULL), "
        "QUARTER('2008-04-01'), HOUR('13:29:17'), MICROSECOND('13:29:17.000006')",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "temporal extract do columns"
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            0U,
            "temporal extract do rows"
        );
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "temporal extract do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "temporal extract do warnings"
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
        "quarter_text",
        "day_text",
        "dayofmonth_text",
        "timestamp_year",
        "timestamp_quarter",
        "timestamp_hour",
        "hour_text",
        "minute_text",
        "second_text",
        "string_quarter",
        "char_quarter",
        "text_quarter",
        "string_hour",
        "string_minute",
        "string_second",
    };
    static const char *const values_table[] = {
        "1",  "2008-01-02", "00:00:00", "13:29:17", "13:29:17", "13:29:17", "2008",      "1",  "1",
        "2",  "2",          "2008",     "1",        "13",       "13",       "29",        "17", "1",
        "4",  "3",          "13",       "29",       "17",       "2",        NULL,        NULL, NULL,
        NULL, NULL,         NULL,       NULL,       NULL,       NULL,       NULL,        NULL, NULL,
        NULL, NULL,         NULL,       NULL,       NULL,       NULL,       NULL,        NULL, NULL,
        NULL, "3",          NULL,       "00:00:00", NULL,       NULL,       "-13:29:17", "0",  NULL,
        "0",  "0",          NULL,       NULL,       NULL,       NULL,       "13",        "29", "17",
        NULL, "0",          "4",        NULL,       NULL,       NULL,
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
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
    static const char *const values_warning_count[] = {"4"};
    static const char *const columns_microsecond_table[] = {
        "id",
        "MICROSECOND(d)",
        "MICROSECOND(tm)",
        "MICROSECOND(dt)",
        "MICROSECOND(ts)",
        "MICROSECOND(s)",
        "MICROSECOND(c)",
        "MICROSECOND(x)",
    };
    static const char *const values_microsecond_table[] = {
        "1",  "0",  "0",  "0",  "0", "6", "7", "8",  "2",  NULL, NULL,     NULL,
        NULL, NULL, NULL, NULL, "3", "0", "0", NULL, NULL, NULL, "123457", "0",
    };
    static const char *const values_microsecond_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'bad'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '0000-00-00'",
    };
    static const char *const values_microsecond_warning_count[] = {"2"};
    static const char *const columns_order_limit[] = {
        "id",
        "TIME(tm)",
        "YEAR(d)",
        "QUARTER(d)",
        "HOUR(tm)",
    };
    static const char *const values_order_limit[] = {
        "3",
        "-13:29:17",
        "0",
        "0",
        "13",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_reopen[] = {
        "YEAR(d)",
        "QUARTER(d)",
        "TIME(tm)",
        "HOUR(tm)",
        "MICROSECOND(tm)",
    };
    static const char *const values_reopen[] = {"2008", "1", "13:29:17", "13", "0"};
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
        "tm TIME NULL, s VARCHAR(32), c CHAR(19), x TEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2008-01-02','2008-01-02 13:29:17','2008-01-02 13:29:17','13:29:17',"
        "'2008-01-02 13:29:17','2008-10-01','2008-07-01'),"
        "(2,NULL,NULL,NULL,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00',NULL,NULL,'-13:29:17','not-a-date','2005-00-00','2001-11-00')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DATE(dt) AS date_text, "
                   "TIME(d) AS time_from_date, TIME(dt) AS time_from_datetime, "
                   "TIME(ts) AS time_from_timestamp, TIME(tm) AS time_from_time, "
                   "YEAR(d) AS year_text, MONTH(dt) AS month_text, "
                   "QUARTER(d) AS quarter_text, DAY(d) AS day_text, "
                   "DAYOFMONTH(dt) AS dayofmonth_text, YEAR(ts) AS timestamp_year, "
                   "QUARTER(ts) AS timestamp_quarter, HOUR(ts) AS timestamp_hour, "
                   "HOUR(tm) AS hour_text, MINUTE(tm) AS minute_text, "
                   "SECOND(tm) AS second_text, QUARTER(s) AS string_quarter, "
                   "QUARTER(c) AS char_quarter, QUARTER(x) AS text_quarter, "
                   "HOUR(s) AS string_hour, MINUTE(s) AS string_minute, "
                   "SECOND(s) AS string_second "
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
            .row_count = 4U,
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
    failures += execute_ok(
        database,
        "CREATE TABLE microsecond_values(id INT, d DATE, tm TIME, dt DATETIME, "
        "ts TIMESTAMP NULL, s VARCHAR(64), c CHAR(32), x TEXT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO microsecond_values VALUES "
        "(1,'2024-01-02','13:29:17','2024-01-02 13:29:17','2024-01-02 13:29:17',"
        "'12:00:00.000006','2019-12-31 23:59:59.000007',"
        "'2001-11-00 01:02:03.000008'),"
        "(2,NULL,NULL,NULL,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00','13:29:17',NULL,NULL,"
        "'bad','12:00:00.1234567','0000-00-00')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, MICROSECOND(d), MICROSECOND(tm), MICROSECOND(dt), "
                   "MICROSECOND(ts), MICROSECOND(s), MICROSECOND(c), MICROSECOND(x) "
                   "FROM microsecond_values ORDER BY id",
            .columns = columns_microsecond_table,
            .column_count =
                sizeof(columns_microsecond_table) / sizeof(columns_microsecond_table[0]),
            .values = values_microsecond_table,
            .row_count = 3U,
            .context = "table-backed microsecond function",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_microsecond_warnings,
            .row_count = 2U,
            .context = "table-backed microsecond warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = sizeof(columns_warning_count) / sizeof(columns_warning_count[0]),
            .values = values_microsecond_warning_count,
            .row_count = 1U,
            .context = "table-backed microsecond warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIME(tm), YEAR(d), QUARTER(d), HOUR(tm) "
                   "FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2",
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
            .sql = "SELECT YEAR(d), QUARTER(d), TIME(tm), HOUR(tm), MICROSECOND(tm) "
                   "FROM t WHERE id = 1",
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

static int test_temporal_extract_predicates(void) {
    static const char *const columns_id[] = {"id"};
    static const char *const values_year_2008[] = {"1", "2"};
    static const char *const values_month_day[] = {"1"};
    static const char *const values_hour_or_minute[] = {"1", "2"};
    static const char *const values_second_null_safe[] = {"1", "3"};
    static const char *const values_day[] = {"1"};
    static const char *const values_dayofweek[] = {"1"};
    static const char *const values_dayofyear[] = {"1"};
    static const char *const values_between_year[] = {"1", "2"};
    static const char *const values_between_dayofmonth[] = {"1", "2"};
    static const char *const values_not_between_year[] = {"3"};
    static const char *const values_week[] = {"2"};
    static const char *const values_weekday[] = {"1"};
    static const char *const values_weekofyear[] = {"1"};
    static const char *const values_yearweek[] = {"2"};
    static const char *const values_microsecond[] = {"1"};
    static const char *const values_time_to_sec[] = {"3"};
    static const char *const values_to_days[] = {"1"};
    static const char *const values_to_seconds[] = {"1"};
    static const char *const values_extract_month[] = {"2"};
    static const char *const values_extract_day_hour[] = {"1"};
    static const char *const values_extract_hour_second[] = {"3"};
    static const char *const values_truth[] = {"1", "2"};
    static const char *const values_not_truth[] = {"3"};
    static const char *const values_null[] = {"4"};
    static const char *const values_not_null[] = {"1", "2", "3"};
    static const char *const values_order_limit[] = {"3", "2"};
    static const char *const values_joined_year[] = {"1", "2"};
    static const char *const values_joined_grouped_left[] = {"2"};
    static const char *const values_invalid_null[] = {"2", "3"};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_invalid_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
    };
    static const char *const columns_warning_count[] = {"@@warning_count"};
    static const char *const values_warning_count[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "predicates", path, sizeof(path));
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, tm TIME NULL, txt VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2008-01-02','2008-01-02 13:29:17','13:29:17',"
        "'2008-01-02 13:29:17.123456'),"
        "(2,'2008-02-03','2008-02-03 00:42:00','00:42:00',"
        "'2008-02-03 00:42:00'),"
        "(3,'0000-00-00',NULL,'-13:29:17',"
        "'0000-00-00 01:02:03'),"
        "(4,NULL,NULL,NULL,NULL)",
        NULL
    );
    failures += execute_ok(database, "CREATE TABLE other(id INT, tag INT)", NULL);
    failures += execute_ok(database, "INSERT INTO other VALUES (1,5),(2,7),(3,5),(5,5)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEAR(d) = 2008 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_year_2008,
            .row_count = 2U,
            .context = "temporal extract YEAR predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE MONTH(dt) = 1 AND DAYOFMONTH(dt) = 2 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_month_day,
            .row_count = 1U,
            .context = "temporal extract MONTH and DAYOFMONTH predicates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE HOUR(tm) = 0 OR MINUTE(dt) = 29 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_hour_or_minute,
            .row_count = 2U,
            .context = "temporal extract HOUR or MINUTE predicates",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE SECOND(tm) <=> 17 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_second_null_safe,
            .row_count = 2U,
            .context = "temporal extract SECOND null-safe predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE DAY(d) = 2 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_day,
            .row_count = 1U,
            .context = "temporal extract DAY predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE DAYOFWEEK(d) = 4 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_dayofweek,
            .row_count = 1U,
            .context = "temporal extract DAYOFWEEK predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE DAYOFYEAR(d) = 2 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_dayofyear,
            .row_count = 1U,
            .context = "temporal extract DAYOFYEAR predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEAR(d) BETWEEN 2000 AND 2010 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_between_year,
            .row_count = 2U,
            .context = "temporal extract YEAR BETWEEN predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE DAYOFMONTH(dt) BETWEEN 2 AND 3 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_between_dayofmonth,
            .row_count = 2U,
            .context = "temporal extract DAYOFMONTH BETWEEN predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEAR(d) NOT BETWEEN 2000 AND 2010 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_not_between_year,
            .row_count = 1U,
            .context = "temporal extract YEAR NOT BETWEEN predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE WEEK(d, 3) = 5 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_week,
            .row_count = 1U,
            .context = "temporal extract WEEK predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE WEEKDAY(d) = 2 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_weekday,
            .row_count = 1U,
            .context = "temporal extract WEEKDAY predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE WEEKDAY(d) + 1 = 3 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_weekday,
            .row_count = 1U,
            .context = "temporal extract WEEKDAY arithmetic predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE WEEKOFYEAR(d) = 1 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_weekofyear,
            .row_count = 1U,
            .context = "temporal extract WEEKOFYEAR predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEARWEEK(d, 3) = 200805 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_yearweek,
            .row_count = 1U,
            .context = "temporal extract YEARWEEK predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE MICROSECOND(txt) = 123456 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_microsecond,
            .row_count = 1U,
            .context = "temporal extract MICROSECOND predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE TIME_TO_SEC(tm) < 0 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_time_to_sec,
            .row_count = 1U,
            .context = "temporal extract TIME_TO_SEC predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE TO_DAYS(d) = 733408 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_to_days,
            .row_count = 1U,
            .context = "temporal extract TO_DAYS predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE TO_SECONDS(dt) = 63366499757 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_to_seconds,
            .row_count = 1U,
            .context = "temporal extract TO_SECONDS predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE EXTRACT(MONTH FROM d) = 2 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_extract_month,
            .row_count = 1U,
            .context = "EXTRACT MONTH predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE EXTRACT(DAY_HOUR FROM dt) = 213 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_extract_day_hour,
            .row_count = 1U,
            .context = "EXTRACT DAY_HOUR predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE EXTRACT(HOUR_SECOND FROM tm) = -132917 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_extract_hour_second,
            .row_count = 1U,
            .context = "EXTRACT HOUR_SECOND predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEAR(d) ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_truth,
            .row_count = 2U,
            .context = "temporal extract truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE NOT YEAR(d) ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_not_truth,
            .row_count = 1U,
            .context = "temporal extract not truth predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEAR(d) <=> NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_null,
            .row_count = 1U,
            .context = "temporal extract null-safe NULL predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEAR(d) IS NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_null,
            .row_count = 1U,
            .context = "temporal extract IS NULL predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE YEAR(d) IS NOT NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_not_null,
            .row_count = 3U,
            .context = "temporal extract IS NOT NULL predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t AS app_t WHERE QUARTER(app_t.d) <= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_order_limit,
            .row_count = 2U,
            .context = "temporal extract predicate alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t WHERE EXTRACT(YEAR FROM d) = 2008 ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_year_2008,
            .row_count = 2U,
            .context = "EXTRACT predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.id FROM t JOIN other ON t.id = other.id "
                   "WHERE YEAR(t.d) = 2008 ORDER BY t.id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_joined_year,
            .row_count = 2U,
            .context = "joined temporal extract predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.id FROM t LEFT JOIN other ON t.id = other.id "
                   "WHERE YEAR(t.d) = 2008 AND MONTH(t.dt) = 2 AND other.tag IN (7) "
                   "GROUP BY t.id ORDER BY t.id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_joined_grouped_left,
            .row_count = 1U,
            .context = "joined grouped temporal extract predicates",
        }
    );

    failures += execute_ok(database, "CREATE TABLE bad(id INT, txt VARCHAR(32))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO bad VALUES (1,'2008-01-02'),(2,'not-a-date'),(3,NULL),(4,'0000-00-00')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM bad WHERE YEAR(txt) IS NULL ORDER BY id",
            .columns = columns_id,
            .column_count = sizeof(columns_id) / sizeof(columns_id[0]),
            .values = values_invalid_null,
            .row_count = 2U,
            .context = "temporal extract invalid string IS NULL predicate",
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
            .context = "temporal extract predicate invalid warnings",
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
            .context = "temporal extract predicate warning count",
        }
    );

    failures += execute_error(
        database,
        "SELECT id FROM t WHERE DATE(d) = '2008-01-02'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "temporal extract predicates support only numeric temporal extractor functions",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE YEAR(d) = '2008'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract predicates support only integer, boolean, and NULL "
                            "comparison literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE YEAR(d) = 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "temporal extract predicate comparison literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM t WHERE YEAR(missing) = 2008",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing'",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporal_extract_diagnostics(void) {
    enum { invalid_temporal_warning_count = 9 };

    static const char *const columns_invalid[] = {
        "DATE('not-a-date')",
        "TIME('not-a-date')",
        "YEAR('not-a-date')",
        "MONTH('not-a-date')",
        "QUARTER('not-a-date')",
        "DAY('not-a-date')",
        "HOUR('not-a-date')",
        "MINUTE('not-a-date')",
        "SECOND('not-a-date')",
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
    static const char *const values_warnings[] = {
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
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
    static const char *const columns_microsecond_date_invalid[] = {
        "MICROSECOND('2024-01-02')",
        "MICROSECOND('not-a-date')",
    };
    static const char *const values_microsecond_date_invalid[] = {"0", NULL};
    static const char *const values_microsecond_date_invalid_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '2024-01-02'",
        "Warning",
        "1292",
        "Truncated incorrect time value: 'not-a-date'",
    };
    static const char *const columns_microsecond_invalid_datetime[] = {
        "MICROSECOND('2019-01-02 24:00:00')",
        "MICROSECOND('2019-01-02 99:00:00')",
        "MICROSECOND('2019-01-02 24:00:00.123456')",
    };
    static const char *const values_microsecond_invalid_datetime[] = {NULL, NULL, NULL};
    static const char *const values_microsecond_invalid_datetime_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '2019-01-02 24:00:00'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '2019-01-02 99:00:00'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '2019-01-02 24:00:00.123456'",
    };
    static const char *const columns_microsecond_invalid_string_column[] = {
        "id",
        "MICROSECOND(s)",
    };
    static const char *const values_microsecond_invalid_string_column[] = {
        "1",
        NULL,
        "2",
        NULL,
    };
    static const char *const values_microsecond_invalid_string_column_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '2019-01-02 24:00:00.123456'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '2019-01-02 99:00:00'",
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
            .sql = "SELECT MICROSECOND('2024-01-02'), MICROSECOND('not-a-date')",
            .columns = columns_microsecond_date_invalid,
            .column_count = sizeof(columns_microsecond_date_invalid) /
                            sizeof(columns_microsecond_date_invalid[0]),
            .values = values_microsecond_date_invalid,
            .row_count = 1U,
            .context = "microsecond date-only and invalid values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_microsecond_date_invalid_warnings,
            .row_count = 2U,
            .context = "microsecond date-only and invalid warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT MICROSECOND('2019-01-02 24:00:00'), "
                   "MICROSECOND('2019-01-02 99:00:00'), "
                   "MICROSECOND('2019-01-02 24:00:00.123456')",
            .columns = columns_microsecond_invalid_datetime,
            .column_count = sizeof(columns_microsecond_invalid_datetime) /
                            sizeof(columns_microsecond_invalid_datetime[0]),
            .values = values_microsecond_invalid_datetime,
            .row_count = 1U,
            .context = "microsecond invalid datetime time parts",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_microsecond_invalid_datetime_warnings,
            .row_count = 3U,
            .context = "microsecond invalid datetime time-part warnings",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE microsecond_invalid_string_values(id INT, s VARCHAR(64))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO microsecond_invalid_string_values VALUES "
        "(1,'2019-01-02 24:00:00.123456'),"
        "(2,'2019-01-02 99:00:00')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, MICROSECOND(s) FROM microsecond_invalid_string_values "
                   "ORDER BY id",
            .columns = columns_microsecond_invalid_string_column,
            .column_count = sizeof(columns_microsecond_invalid_string_column) /
                            sizeof(columns_microsecond_invalid_string_column[0]),
            .values = values_microsecond_invalid_string_column,
            .row_count = 2U,
            .context = "row-backed microsecond invalid datetime time parts",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_microsecond_invalid_string_column_warnings,
            .row_count = 2U,
            .context = "row-backed microsecond invalid datetime warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE('not-a-date'), TIME('not-a-date'), YEAR('not-a-date'), "
                   "MONTH('not-a-date'), QUARTER('not-a-date'), DAY('not-a-date'), "
                   "HOUR('not-a-date'), MINUTE('not-a-date'), SECOND('not-a-date')",
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
    failures += execute_error(
        database,
        "SELECT QUARTER(20080102)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT MICROSECOND(20080102132917)",
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
    failures += execute_error(
        database,
        "SELECT MICROSECOND(id) FROM unsupported_types",
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
    failures += execute_ok(database, "CREATE TABLE unsupported_times(tm TIME)", NULL);
    failures += execute_error(
        database,
        "SELECT QUARTER(tm) FROM unsupported_times",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE()/YEAR()/QUARTER()/MONTH()/DAY()/DAYOFMONTH() do not support TIME values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_extract_function(void) {
    static const char *const columns_core[] = {
        "EXTRACT(YEAR FROM '2019-07-02 01:02:03')",
        "EXTRACT(MONTH FROM '2019-07-02 01:02:03')",
        "EXTRACT(DAY FROM '2019-07-02 01:02:03')",
        "EXTRACT(HOUR FROM '2019-07-02 01:02:03')",
        "EXTRACT(MINUTE FROM '2019-07-02 01:02:03')",
        "EXTRACT(SECOND FROM '2019-07-02 01:02:03')",
        "EXTRACT(QUARTER FROM '2019-07-02 01:02:03')",
        "EXTRACT(YEAR_MONTH FROM '2019-07-02 01:02:03')",
        "EXTRACT(DAY_HOUR FROM '2019-07-02 01:02:03')",
        "EXTRACT(DAY_MINUTE FROM '2019-07-02 01:02:03')",
        "EXTRACT(DAY_SECOND FROM '2019-07-02 01:02:03')",
        "EXTRACT(HOUR_MINUTE FROM '2019-07-02 01:02:03')",
        "EXTRACT(HOUR_SECOND FROM '2019-07-02 01:02:03')",
        "EXTRACT(MINUTE_SECOND FROM '2019-07-02 01:02:03')",
        "EXTRACT(MICROSECOND FROM '12:00:00.123456')",
        "EXTRACT(MICROSECOND FROM '12:00:00.1')",
        "EXTRACT(MICROSECOND FROM '12:00:00.1234567')",
        "EXTRACT(MICROSECOND FROM '12:00:00.9999995')",
        "EXTRACT(MICROSECOND FROM '2019-12-31 23:59:59.000010')",
        "EXTRACT(MICROSECOND FROM '-13:29:17.000006')",
        "EXTRACT(MICROSECOND FROM '12:00:00')",
        "EXTRACT(YEAR FROM NULL)",
        "EXTRACT(HOUR FROM '-13:29:17')",
        "EXTRACT(MINUTE FROM '-13:29:17')",
        "EXTRACT(SECOND FROM '-13:29:17')",
        "EXTRACT(HOUR_MINUTE FROM '-13:29:17')",
        "EXTRACT(DAY_SECOND FROM '-13:29:17')",
        "EXTRACT(MINUTE_SECOND FROM '-13:29:17')",
    };
    static const char *const values_core[] = {
        "2019",    "7",   "2",     "1",   "2",      "3",      "3",       "201907", "201", "20102",
        "2010203", "102", "10203", "203", "123456", "100000", "123457",  "0",      "10",  "-6",
        "0",       NULL,  "-13",   "-29", "-17",    "-1329",  "-132917", "-2917",
    };
    static const char *const columns_dual[] = {
        "EXTRACT(YEAR FROM '2019-07-02')",
        "qtr",
        "hm",
    };
    static const char *const values_dual[] = {"2019", "3", "102"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const columns_table[] = {
        "id",
        "EXTRACT(YEAR FROM d)",
        "EXTRACT(MICROSECOND FROM d)",
        "EXTRACT(QUARTER FROM dt)",
        "EXTRACT(YEAR_MONTH FROM dt)",
        "EXTRACT(DAY_SECOND FROM dt)",
        "EXTRACT(MICROSECOND FROM dt)",
        "EXTRACT(YEAR FROM ts)",
        "EXTRACT(DAY_SECOND FROM ts)",
        "EXTRACT(HOUR FROM tm)",
        "EXTRACT(HOUR FROM s)",
        "EXTRACT(DAY_SECOND FROM tm)",
        "EXTRACT(DAY_SECOND FROM s)",
        "EXTRACT(MICROSECOND FROM sf)",
    };
    static const char *const values_table[] = {
        "1",   "2019", "0",       "3",      "201907", "2132917", "0",  "2019", "2132917",
        "-13", "13",   "-132917", "132917", "-12",    "2",       NULL, NULL,   NULL,
        NULL,  NULL,   NULL,      NULL,     NULL,     NULL,      NULL, NULL,   NULL,
        NULL,  "3",    "0",       "0",      NULL,     NULL,      NULL, NULL,   NULL,
        NULL,  "13",   NULL,      "132917", NULL,     NULL,
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_table_warnings[] = {
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
    static const char *const columns_filtered[] = {"id", "EXTRACT(DAY_SECOND FROM tm)"};
    static const char *const values_filtered[] = {"3", "132917", "2", NULL};
    static const char *const columns_reopen[] = {
        "EXTRACT(YEAR_MONTH FROM dt)",
        "EXTRACT(HOUR FROM tm)",
    };
    static const char *const values_reopen[] = {"201907", "-13"};
    static const char *const columns_invalid[] = {
        "EXTRACT(YEAR FROM 'not-a-date')",
        "EXTRACT(HOUR FROM 'not-a-date')",
        "EXTRACT(DAY_SECOND FROM 'not-a-date')",
        "EXTRACT(QUARTER FROM 'not-a-date')",
        "EXTRACT(MICROSECOND FROM 'not-a-date')",
        "EXTRACT(MICROSECOND FROM '2019-01-02 24:00:00.123456')",
        "EXTRACT(MICROSECOND FROM '2024-01-02')",
    };
    static const char *const values_invalid[] = {NULL, NULL, NULL, NULL, NULL, NULL, "0"};
    static const char *const values_invalid_warnings[] = {
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
        "Warning", "1292", "Incorrect datetime value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: 'not-a-date'",
        "Warning", "1292", "Truncated incorrect time value: '2019-01-02 24:00:00.123456'",
        "Warning", "1292", "Truncated incorrect time value: '2024-01-02'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "extract", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT EXTRACT(YEAR FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(MONTH FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(DAY FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(HOUR FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(MINUTE FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(SECOND FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(QUARTER FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(YEAR_MONTH FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(DAY_HOUR FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(DAY_MINUTE FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(DAY_SECOND FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(HOUR_MINUTE FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(HOUR_SECOND FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(MINUTE_SECOND FROM '2019-07-02 01:02:03'), "
                   "EXTRACT(MICROSECOND FROM '12:00:00.123456'), "
                   "EXTRACT(MICROSECOND FROM '12:00:00.1'), "
                   "EXTRACT(MICROSECOND FROM '12:00:00.1234567'), "
                   "EXTRACT(MICROSECOND FROM '12:00:00.9999995'), "
                   "EXTRACT(MICROSECOND FROM '2019-12-31 23:59:59.000010'), "
                   "EXTRACT(MICROSECOND FROM '-13:29:17.000006'), "
                   "EXTRACT(MICROSECOND FROM '12:00:00'), "
                   "EXTRACT(YEAR FROM NULL), EXTRACT(HOUR FROM '-13:29:17'), "
                   "EXTRACT(MINUTE FROM '-13:29:17'), EXTRACT(SECOND FROM '-13:29:17'), "
                   "EXTRACT(HOUR_MINUTE FROM '-13:29:17'), "
                   "EXTRACT(DAY_SECOND FROM '-13:29:17'), "
                   "EXTRACT(MINUTE_SECOND FROM '-13:29:17')",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .context = "no-source extract",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT EXTRACT(YEAR FROM '2019-07-02'), "
                   "EXTRACT(QUARTER FROM '2019-07-02') AS qtr, "
                   "EXTRACT(HOUR_MINUTE FROM '01:02:03') AS hm FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual extract",
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
            .context = "row count after extract select",
        }
    );
    failures += execute_ok(
        database,
        "DO EXTRACT(YEAR FROM '2019-07-02'), EXTRACT(HOUR FROM '-13:29:17'), "
        "EXTRACT(MICROSECOND FROM '12:00:00.000123'), EXTRACT(YEAR FROM NULL)",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "extract do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "extract do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "extract do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "extract do warnings");
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
            .context = "row count after extract do",
        }
    );

    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "
        "tm TIME NULL, s VARCHAR(32), sf VARCHAR(32))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2019-07-02','2019-07-02 13:29:17','2019-07-02 13:29:17','-13:29:17',"
        "'13:29:17','-13:29:17.000012'),"
        "(2,NULL,NULL,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00',NULL,NULL,'13:29:17','not-a-date','not-a-date')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, EXTRACT(YEAR FROM d), EXTRACT(MICROSECOND FROM d), "
                   "EXTRACT(QUARTER FROM dt), "
                   "EXTRACT(YEAR_MONTH FROM dt), EXTRACT(DAY_SECOND FROM dt), "
                   "EXTRACT(MICROSECOND FROM dt), EXTRACT(YEAR FROM ts), "
                   "EXTRACT(DAY_SECOND FROM ts), EXTRACT(HOUR FROM tm), "
                   "EXTRACT(HOUR FROM s), EXTRACT(DAY_SECOND FROM tm), "
                   "EXTRACT(DAY_SECOND FROM s), EXTRACT(MICROSECOND FROM sf) "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .context = "table-backed extract",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_table_warnings,
            .row_count = 3U,
            .context = "table-backed extract warnings",
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
            .context = "table-backed extract warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, EXTRACT(DAY_SECOND FROM tm) FROM t WHERE id >= 1 "
                   "ORDER BY id DESC LIMIT 2",
            .columns = columns_filtered,
            .column_count = sizeof(columns_filtered) / sizeof(columns_filtered[0]),
            .values = values_filtered,
            .row_count = 2U,
            .context = "table-backed extract filtered ordered limited envelope",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_open(path, &database) == MYLITE_OK ? 0 : 1;
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT EXTRACT(YEAR_MONTH FROM dt), EXTRACT(HOUR FROM tm) FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopened extract",
        }
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT EXTRACT(YEAR FROM 'not-a-date'), "
                   "EXTRACT(HOUR FROM 'not-a-date'), "
                   "EXTRACT(DAY_SECOND FROM 'not-a-date'), "
                   "EXTRACT(QUARTER FROM 'not-a-date'), "
                   "EXTRACT(MICROSECOND FROM 'not-a-date'), "
                   "EXTRACT(MICROSECOND FROM '2019-01-02 24:00:00.123456'), "
                   "EXTRACT(MICROSECOND FROM '2024-01-02')",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .context = "invalid extract values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_warnings,
            .row_count = (sizeof(values_invalid_warnings) / sizeof(values_invalid_warnings[0])) /
                         (sizeof(columns_warnings) / sizeof(columns_warnings[0])),
            .context = "invalid extract warnings",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXTRACT(WEEK FROM '2019-07-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "EXTRACT() unit WEEK is not yet supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT EXTRACT(YEAR FROM 20190702)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "temporal extract functions support only string temporal literals",
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);
    size_t value_count = expected.row_count * expected.column_count;

    if (failures == 0) {
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
    }
    for (size_t column = 0U; failures == 0 && column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
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
    int failures = mylite_test_make_path(path, path_size, name);

    if (failures == 0) {
        remove_related_files(path);
        failures += mylite_open(path, out_database) == MYLITE_OK ? 0 : 1;
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
    return mylite_test_expect_text(actual, expected, context);
}
