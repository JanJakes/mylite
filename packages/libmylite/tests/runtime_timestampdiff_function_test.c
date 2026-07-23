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
    timestampdiff_core_column_count = 8,
    timestampdiff_calendar_column_count = 8,
    timestampdiff_alias_column_count = 8,
    timestampdiff_table_column_count = 5,
    timestampdiff_invalid_column_count = 5,
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

static int test_no_source_dual_and_do_timestampdiff(void);
static int test_table_backed_timestampdiff_and_reopen(void);
static int test_timestampdiff_warnings_and_diagnostics(void);
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

    failures += test_no_source_dual_and_do_timestampdiff();
    failures += test_table_backed_timestampdiff_and_reopen();
    failures += test_timestampdiff_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_timestampdiff(void) {
    static const char *const columns_core[] = {
        "yr",
        "qtr",
        "mon",
        "wk",
        "dy",
        "hr",
        "mn",
        "sec",
    };
    static const char *const values_core[] = {
        "1",
        "4",
        "13",
        "56",
        "398",
        "9556",
        "573365",
        "34401906",
    };
    static const char *const values_reverse[] = {
        "-1",
        "-4",
        "-13",
        "-56",
        "-398",
        "-9556",
        "-573365",
        "-34401906",
    };
    static const char *const columns_calendar[] = {
        "yr_before",
        "yr_exact",
        "mon_short",
        "mon_next",
        "q_short",
        "q_reverse",
        "mon_reverse",
        "yr_reverse",
    };
    static const char *const values_calendar[] = {
        "0",
        "1",
        "0",
        "1",
        "0",
        "-1",
        "-1",
        "-1",
    };
    static const char *const columns_partial[] = {
        "day_pos",
        "hour_pos",
        "minute_pos",
        "second_pos",
        "second_neg",
        "minute_neg",
        "hour_neg",
        "day_neg",
    };
    static const char *const values_partial[] = {
        "0",
        "0",
        "0",
        "1",
        "-1",
        "0",
        "0",
        "0",
    };
    static const char *const columns_aliases[] = {
        "y",
        "q",
        "m",
        "w",
        "h",
        "min",
        "s",
        "zero_year",
    };
    static const char *const values_aliases[] = {
        "1",
        "1",
        "1",
        "2",
        "24",
        "2",
        "3",
        "2",
    };
    static const char *const columns_dual[] = {
        "TIMESTAMPDIFF (DAY,'2003-01-01','2003-01-02')",
        "tsd",
    };
    static const char *const values_dual[] = {"1", "1"};
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
            .sql = "SELECT "
                   "TIMESTAMPDIFF(YEAR,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS yr, "
                   "TIMESTAMPDIFF(QUARTER,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS qtr, "
                   "TIMESTAMPDIFF(MONTH,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS mon, "
                   "TIMESTAMPDIFF(WEEK,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS wk, "
                   "TIMESTAMPDIFF(DAY,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS dy, "
                   "TIMESTAMPDIFF(HOUR,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS hr, "
                   "TIMESTAMPDIFF(MINUTE,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS mn, "
                   "TIMESTAMPDIFF(SECOND,'2003-01-01 00:00:00','2004-02-03 04:05:06') AS sec",
            .columns = columns_core,
            .column_count = timestampdiff_core_column_count,
            .values = values_core,
            .row_count = 1U,
            .context = "no-source timestampdiff core",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "TIMESTAMPDIFF(YEAR,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS yr, "
                   "TIMESTAMPDIFF(QUARTER,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS qtr, "
                   "TIMESTAMPDIFF(MONTH,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS mon, "
                   "TIMESTAMPDIFF(WEEK,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS wk, "
                   "TIMESTAMPDIFF(DAY,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS dy, "
                   "TIMESTAMPDIFF(HOUR,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS hr, "
                   "TIMESTAMPDIFF(MINUTE,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS mn, "
                   "TIMESTAMPDIFF(SECOND,'2004-02-03 04:05:06','2003-01-01 00:00:00') AS sec",
            .columns = columns_core,
            .column_count = timestampdiff_core_column_count,
            .values = values_reverse,
            .row_count = 1U,
            .context = "no-source timestampdiff reverse",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "TIMESTAMPDIFF(YEAR,'2003-02-02','2004-02-01') AS yr_before, "
                   "TIMESTAMPDIFF(YEAR,'2003-02-02','2004-02-02') AS yr_exact, "
                   "TIMESTAMPDIFF(MONTH,'2003-01-31','2003-02-28') AS mon_short, "
                   "TIMESTAMPDIFF(MONTH,'2003-01-31','2003-03-01') AS mon_next, "
                   "TIMESTAMPDIFF(QUARTER,'2003-01-01','2003-03-31') AS q_short, "
                   "TIMESTAMPDIFF(QUARTER,'2003-07-01','2003-04-01') AS q_reverse, "
                   "TIMESTAMPDIFF(MONTH,'2003-03-01','2003-01-31') AS mon_reverse, "
                   "TIMESTAMPDIFF(YEAR,'2004-02-02','2003-02-02') AS yr_reverse",
            .columns = columns_calendar,
            .column_count = timestampdiff_calendar_column_count,
            .values = values_calendar,
            .row_count = 1U,
            .context = "timestampdiff calendar boundaries",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql =
                "SELECT "
                "TIMESTAMPDIFF(DAY,'2003-02-01 23:59:59','2003-02-02 00:00:00') AS day_pos, "
                "TIMESTAMPDIFF(HOUR,'2003-02-01 23:59:59','2003-02-02 00:00:00') AS hour_pos, "
                "TIMESTAMPDIFF(MINUTE,'2003-02-01 23:59:59','2003-02-02 00:00:00') AS minute_pos, "
                "TIMESTAMPDIFF(SECOND,'2003-02-01 23:59:59','2003-02-02 00:00:00') AS second_pos, "
                "TIMESTAMPDIFF(SECOND,'2003-02-02 00:00:00','2003-02-01 23:59:59') AS second_neg, "
                "TIMESTAMPDIFF(MINUTE,'2003-02-02 00:00:00','2003-02-01 23:59:59') AS minute_neg, "
                "TIMESTAMPDIFF(HOUR,'2003-02-02 00:00:00','2003-02-01 23:59:59') AS hour_neg, "
                "TIMESTAMPDIFF(DAY,'2003-02-02 00:00:00','2003-02-01 23:59:59') AS day_neg",
            .columns = columns_partial,
            .column_count = timestampdiff_calendar_column_count,
            .values = values_partial,
            .row_count = 1U,
            .context = "timestampdiff partial units",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "TIMESTAMPDIFF(SQL_TSI_YEAR,'2003-02-02','2004-02-02') AS y, "
                   "TIMESTAMPDIFF(SQL_TSI_QUARTER,'2003-01-01','2003-04-01') AS q, "
                   "TIMESTAMPDIFF(SQL_TSI_MONTH,'2003-01-31','2003-03-01') AS m, "
                   "TIMESTAMPDIFF(SQL_TSI_WEEK,'2003-02-01','2003-02-15') AS w, "
                   "TIMESTAMPDIFF(SQL_TSI_HOUR,'2003-02-01','2003-02-02') AS h, "
                   "TIMESTAMPDIFF(SQL_TSI_MINUTE,'2003-02-01','2003-02-01 00:02:03') AS min, "
                   "TIMESTAMPDIFF(SQL_TSI_SECOND,'2003-02-01','2003-02-01 00:00:03') AS s, "
                   "TIMESTAMPDIFF(DAY,'0000-01-02','0000-01-04') AS zero_year",
            .columns = columns_aliases,
            .column_count = timestampdiff_alias_column_count,
            .values = values_aliases,
            .row_count = 1U,
            .context = "timestampdiff SQL_TSI aliases",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPDIFF (DAY,'2003-01-01','2003-01-02'), "
                   "TIMESTAMPDIFF(SQL_TSI_DAY,'2003-01-01','2003-01-02') AS tsd FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual timestampdiff",
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
            .context = "row count after timestampdiff select",
        }
    );
    failures += execute_ok(
        database,
        "DO TIMESTAMPDIFF(DAY,'2003-01-01','2003-01-02'), "
        "TIMESTAMPDIFF(MONTH,NULL,'2003-01-01')",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "timestampdiff do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "timestampdiff do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "timestampdiff do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "timestampdiff do warnings"
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
            .context = "row count after timestampdiff do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_timestampdiff_and_reopen(void) {
    static const char *const columns_table[] = {"id", "dt_d", "ts_h", "txt_m", "lit_h"};
    static const char *const values_table[] = {
        "1",
        "1",
        "52",
        "1",
        "36",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        "3",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_limited[] = {"id", "days"};
    static const char *const values_limited[] = {"2", NULL, "1", "1"};
    static const char *const columns_bad_text[] = {"bad_txt"};
    static const char *const values_bad_text[] = {NULL};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_bad_text_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
    };
    static const char *const columns_reopen[] = {"ts_h"};
    static const char *const values_reopen[] = {"52"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, txt VARCHAR(32), tm TIME"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '2003-02-01', '2003-02-02 01:02:03', "
        "'2003-02-03 04:05:06', '2003-03-01', '01:02:03'), "
        "(2, NULL, NULL, NULL, NULL, NULL), "
        "(3, '0000-00-00', '2001-11-00 00:00:00', NULL, 'not-a-date', '02:03:04'), "
        "(4, '2003-01-01', NULL, NULL, 'not-a-date', '03:04:05')",
        NULL
    );
    failures += execute_ok(
        database,
        "SELECT id, TIMESTAMPDIFF(DAY,d,dt) AS dt_d, TIMESTAMPDIFF(HOUR,d,ts) AS ts_h, "
        "TIMESTAMPDIFF(MONTH,d,txt) AS txt_m, "
        "TIMESTAMPDIFF(HOUR,d,'2003-02-02 12:00:00') AS lit_h "
        "FROM t WHERE id < 4 ORDER BY id",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            timestampdiff_table_column_count,
            "table columns"
        );
        failures += mylite_test_expect_size(mylite_result_row_count(result), 3U, "table rows");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "table warnings");
        for (size_t column = 0U; column < timestampdiff_table_column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                columns_table[column],
                "table column"
            );
        }
        for (size_t row = 0U; row < 3U; ++row) {
            for (size_t column = 0U; column < timestampdiff_table_column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    values_table[(row * timestampdiff_table_column_count) + column],
                    "table timestampdiff"
                );
            }
        }
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIMESTAMPDIFF(DAY,d,dt) AS days "
                   "FROM t WHERE id < 3 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table timestampdiff where order limit",
        }
    );
    failures += execute_ok(
        database,
        "SELECT TIMESTAMPDIFF(DAY,d,txt) AS bad_txt FROM t WHERE id = 4",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 1U, "bad text columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "bad text rows");
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            1U,
            "bad text warning count"
        );
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, 0U),
            columns_bad_text[0],
            "bad text column"
        );
        failures += expect_result_value(result, 0U, 0U, values_bad_text[0], "bad text value");
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_bad_text_warnings,
            .row_count = 1U,
            .context = "bad text warnings",
        }
    );

    failures += execute_error(
        database,
        "SELECT TIMESTAMPDIFF(DAY,tm,d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPDIFF() does not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPDIFF(DAY,id,d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPDIFF() supports only DATE, DATETIME, TIMESTAMP, string "
                            "descriptor columns, string temporal literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPDIFF(DAY,DATE_ADD(d, INTERVAL 1 SECOND),d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPDIFF() supports only string temporal literals, DATE, "
                            "DATETIME, TIMESTAMP descriptor columns, string descriptor columns, "
                            "and NULL",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen timestampdiff database"
    );
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMESTAMPDIFF(HOUR,d,ts) AS ts_h FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopened timestampdiff",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_timestampdiff_warnings_and_diagnostics(void) {
    static const char *const columns_invalid[] = {
        "null_bad",
        "null_numeric",
        "bad_null",
        "bad_pair",
        "bad_right",
    };
    static const char *const values_invalid[] = {NULL, NULL, NULL, NULL, NULL};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_invalid_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad-left'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad-right'",
    };
    static const char *const columns_zero[] = {
        "full_zero",
        "partial_zero",
        "year_zero",
        "zero_right",
    };
    static const char *const values_zero[] = {NULL, NULL, "731610", NULL};
    static const char *const values_zero_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-00-00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2001-11-00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-00-00'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(
        database,
        "SELECT TIMESTAMPDIFF(DAY,NULL,'not-a-date') AS null_bad, "
        "TIMESTAMPDIFF(DAY,NULL,1) AS null_numeric, "
        "TIMESTAMPDIFF(DAY,'not-a-date',NULL) AS bad_null, "
        "TIMESTAMPDIFF(DAY,'bad-left','bad-right') AS bad_pair, "
        "TIMESTAMPDIFF(DAY,'2003-01-01','bad-right') AS bad_right",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            timestampdiff_invalid_column_count,
            "invalid columns"
        );
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "invalid rows");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 3U, "invalid warnings");
        for (size_t column = 0U; column < timestampdiff_invalid_column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                columns_invalid[column],
                "invalid column"
            );
            failures +=
                expect_result_value(result, 0U, column, values_invalid[column], "invalid value");
        }
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_warnings,
            .row_count = 3U,
            .context = "invalid timestampdiff warnings",
        }
    );

    failures += execute_ok(
        database,
        "SELECT TIMESTAMPDIFF(DAY,'0000-00-00','2003-02-01') AS full_zero, "
        "TIMESTAMPDIFF(DAY,'2001-11-00','2003-02-01') AS partial_zero, "
        "TIMESTAMPDIFF(DAY,'0000-01-02','2003-02-01') AS year_zero, "
        "TIMESTAMPDIFF(DAY,'2003-02-01','0000-00-00') AS zero_right",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 4U, "zero columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "zero rows");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 3U, "zero warnings");
        for (size_t column = 0U; column < 4U; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                columns_zero[column],
                "zero column"
            );
            failures += expect_result_value(result, 0U, column, values_zero[column], "zero value");
        }
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_zero_warnings,
            .row_count = 3U,
            .context = "zero timestampdiff warnings",
        }
    );

    failures += execute_error(
        database,
        "SELECT TIMESTAMPDIFF(MICROSECOND,'2003-01-01','2003-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPDIFF() does not yet support MICROSECOND",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPDIFF(DAY,1,'2003-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "TIMESTAMPDIFF() supports only string temporal literals, DATE, "
                            "DATETIME, TIMESTAMP descriptor columns, string descriptor columns, "
                            "and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMESTAMPDIFF(DAY,missing,'2003-01-02')",
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "execute error");
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "execute error");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "execute error"
    );
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
        failures += execute_ok(*out_database, "SET sql_mode = ''", NULL);
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
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

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
