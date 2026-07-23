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
    mysql_connection_collation_id = 255,
    mysql_calendar_name_display_length = 36,
    mysql_calendar_name_decimals = 31,
    invalid_calendar_name_warning_count = 6,
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

static int test_no_source_dual_metadata_and_do(void);
static int test_day_month_name_warnings(void);
static int test_table_backed_day_month_names_and_reopen(void);
static int test_day_month_name_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_calendar_name_metadata(
    const mylite_result *result,
    size_t column,
    const char *context
);
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

    failures += test_no_source_dual_metadata_and_do();
    failures += test_day_month_name_warnings();
    failures += test_table_backed_day_month_names_and_reopen();
    failures += test_day_month_name_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_metadata_and_do(void) {
    static const char *const columns_core[] = {
        "DAYNAME('2007-02-03')",
        "MONTHNAME('2008-02-03')",
        "DAYNAME('2008-01-02 13:29:17')",
        "MONTHNAME('2008-12-31 23:59:59')",
        "DAYNAME('0001-01-01')",
        "MONTHNAME('0999-12-31')",
        "DAYNAME(NULL)",
        "MONTHNAME(NULL)",
    };
    static const char *const values_core[] = {
        "Saturday",
        "February",
        "Wednesday",
        "December",
        "Monday",
        "December",
        NULL,
        NULL,
    };
    static const char *const columns_dual[] = {"day_name", "month_name"};
    static const char *const values_dual[] = {"Saturday", "February"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += execute_ok(
        database,
        "SELECT DAYNAME('2007-02-03'), MONTHNAME('2008-02-03'), "
        "DAYNAME('2008-01-02 13:29:17'), MONTHNAME('2008-12-31 23:59:59'), "
        "DAYNAME('0001-01-01'), MONTHNAME('0999-12-31'), DAYNAME(NULL), "
        "MONTHNAME(NULL)",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            sizeof(columns_core) / sizeof(columns_core[0]),
            "core calendar name column count"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 1U, "core calendar name rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "core calendar name affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "core calendar name warnings"
        );
        for (size_t column = 0U; column < sizeof(columns_core) / sizeof(columns_core[0]);
             ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                columns_core[column],
                "core calendar name column"
            );
            failures += expect_result_value(
                result,
                0U,
                column,
                values_core[column],
                "core calendar name value"
            );
            failures +=
                expect_calendar_name_metadata(result, column, "core calendar name metadata");
        }
    }
    mylite_result_free(result);

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYNAME ('2007-02-03') AS day_name, "
                   "MONTHNAME ('2008-02-03') AS month_name FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual calendar names",
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
            .context = "row count after calendar name select",
        }
    );

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT DAYNAME(\"2007-02-03\")",
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
            .sql = "SELECT DAYNAME('2007-02-03'), MONTHNAME('2008-02-03')",
            .columns = columns_core,
            .column_count = 2U,
            .values = values_core,
            .row_count = 1U,
            .context = "calendar names after NO_BACKSLASH_ESCAPES",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_ok(database, "DO DAYNAME('2007-02-03'), MONTHNAME(NULL)", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "calendar name do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "calendar name do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "calendar name do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "calendar name do warnings"
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
            .context = "row count after calendar name do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_day_month_name_warnings(void) {
    static const char *const columns_zero[] = {
        "DAYNAME('0000-00-00')",
        "DAYNAME('2001-11-00')",
        "MONTHNAME('2001-11-00')",
        "DAYNAME('0000-01-02')",
        "MONTHNAME('0000-01-02')",
        "MONTHNAME('0000-02-29')",
    };
    static const char *const values_zero[] = {
        NULL,
        NULL,
        "November",
        "Monday",
        "January",
        NULL,
    };
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
    static const char *const columns_invalid[] = {
        "DAYNAME('not-a-date')",
        "MONTHNAME('not-a-date')",
        "DAYNAME('13:29:17')",
        "MONTHNAME('13:29:17')",
        "DAYNAME('2008-01-02 24:00:00')",
        "MONTHNAME('2008-01-02 99:00:00')",
    };
    static const char *const values_invalid[] = {NULL, NULL, NULL, NULL, NULL, NULL};
    static const char *const values_invalid_warnings[] = {
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
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 24:00:00'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2008-01-02 99:00:00'",
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const columns_warning_count[] = {"@@warning_count"};
    static const char *const values_zero_warning_count[] = {"3"};
    static const char *const values_invalid_warning_count[] = {"6"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "warnings", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYNAME('0000-00-00'), DAYNAME('2001-11-00'), "
                   "MONTHNAME('2001-11-00'), DAYNAME('0000-01-02'), "
                   "MONTHNAME('0000-01-02'), "
                   "MONTHNAME('0000-02-29')",
            .columns = columns_zero,
            .column_count = sizeof(columns_zero) / sizeof(columns_zero[0]),
            .values = values_zero,
            .row_count = 1U,
            .warning_count = 3U,
            .context = "zero calendar name values",
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
            .context = "zero calendar name warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = 1U,
            .values = values_zero_warning_count,
            .row_count = 1U,
            .context = "zero calendar name warning count",
        }
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYNAME('not-a-date'), MONTHNAME('not-a-date'), "
                   "DAYNAME('13:29:17'), MONTHNAME('13:29:17'), "
                   "DAYNAME('2008-01-02 24:00:00'), "
                   "MONTHNAME('2008-01-02 99:00:00')",
            .columns = columns_invalid,
            .column_count = sizeof(columns_invalid) / sizeof(columns_invalid[0]),
            .values = values_invalid,
            .row_count = 1U,
            .warning_count = invalid_calendar_name_warning_count,
            .context = "invalid calendar name values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_invalid_warnings,
            .row_count = invalid_calendar_name_warning_count,
            .context = "invalid calendar name warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count",
            .columns = columns_warning_count,
            .column_count = 1U,
            .values = values_invalid_warning_count,
            .row_count = 1U,
            .context = "invalid calendar name warning count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_day_month_names_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "day_d",
        "month_dt",
        "day_ts",
        "month_s",
        "day_txt",
        "month_txt",
    };
    static const char *const values_table[] = {
        "1", "Saturday", "December", "Sunday", "February", "Sunday", "February",
        "2", NULL,       NULL,       NULL,     NULL,       NULL,     NULL,
        "3", NULL,       "November", NULL,     NULL,       NULL,     "November",
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
    static const char *const columns_limited[] = {"id", "month_dt"};
    static const char *const values_limited[] = {
        "3",
        "November",
        "2",
        NULL,
    };
    static const char *const columns_reopen[] = {"day_ts", "month_s"};
    static const char *const values_reopen[] = {"Sunday", "February"};
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
        "'2008-02-03','2008-02-03','13:29:17'),"
        "(2,NULL,NULL,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00','2001-11-00 00:00:00',NULL,"
        "'not-a-date','2001-11-00','01:02:03')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DAYNAME(d) AS day_d, MONTHNAME(dt) AS month_dt, "
                   "DAYNAME(ts) AS day_ts, MONTHNAME(s) AS month_s, "
                   "DAYNAME(txt) AS day_txt, MONTHNAME(txt) AS month_txt "
                   "FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "table-backed calendar names",
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
            .context = "table-backed calendar name warnings",
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
            .context = "table-backed calendar name warning count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, MONTHNAME(dt) AS month_dt FROM t ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "calendar name row envelope",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen calendar name database"
    );
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DAYNAME(ts) AS day_ts, MONTHNAME(s) AS month_s FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "calendar name reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_day_month_name_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, d DATE, tm TIME)", NULL);
    failures += execute_error(
        database,
        "SELECT DAYNAME()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DAYNAME'",
        }
    );
    failures += execute_error(
        database,
        "SELECT MONTHNAME('2007-02-03', 'x')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'MONTHNAME'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYNAME(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_error(
        database,
        "SELECT MONTHNAME(tm) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar name functions do not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYNAME(20070203)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar name functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT MONTHNAME(DATE_ADD(d, INTERVAL 1 SECOND)) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar name functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYNAME('2008\\0-01-01')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar name function literals do not support NUL bytes",
        }
    );
    failures += execute_error(
        database,
        "SELECT DAYNAME('2008\\0-01-01') FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "calendar name function literals do not support NUL bytes",
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

static int expect_calendar_name_metadata(
    const mylite_result *result,
    size_t column,
    const char *context
) {
    int failures = 0;

    failures += mylite_test_expect_int(
        (int)mylite_result_column_type(result, column),
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        context
    );
    failures += mylite_test_expect_uint32(mylite_result_column_flags(result, column), 0U, context);
    failures += mylite_test_expect_uint32(
        mylite_result_column_charset_id(result, column),
        mysql_connection_collation_id,
        context
    );
    failures += mylite_test_expect_uint32(
        mylite_result_column_collation_id(result, column),
        mysql_connection_collation_id,
        context
    );
    failures += mylite_test_expect_uint64(
        mylite_result_column_display_length(result, column),
        mysql_calendar_name_display_length,
        context
    );
    failures += mylite_test_expect_int(
        mylite_result_column_decimals(result, column),
        mysql_calendar_name_decimals,
        context
    );
    failures += mylite_test_expect_int(mylite_result_column_nullable(result, column), 1, context);
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
