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
    datediff_table_column_count = 5,
    datediff_invalid_column_count = 5,
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
    const char *context;
};

static int test_no_source_dual_and_do_datediff(void);
static int test_table_backed_datediff_and_reopen(void);
static int test_datediff_warnings_and_diagnostics(void);
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

    failures += test_no_source_dual_and_do_datediff();
    failures += test_table_backed_datediff_and_reopen();
    failures += test_datediff_warnings_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_datediff(void) {
    static const char *const columns_core[] = {
        "DATEDIFF('2007-12-31 23:59:59', '2007-12-30')",
        "DATEDIFF('2010-11-30 23:59:59', '2010-12-31')",
        "DATEDIFF('2008-01-02 13:29:17', '2008-01-02 23:59:59')",
        "DATEDIFF(NULL, '2008-01-01')",
        "DATEDIFF('2008-01-01', NULL)",
        "DATEDIFF('1000-01-01', '9999-12-31')",
        "DATEDIFF('9999-12-31', '1000-01-01')",
        "DATEDIFF('1582-10-15', '1582-10-04')",
        "DATEDIFF('2000-03-01', '2000-02-28')",
        "DATEDIFF('1900-03-01', '1900-02-28')",
        "DATEDIFF('0001-01-01', '0000-12-31')",
        "DATEDIFF('0000-03-01', '0000-02-28')",
    };
    static const char *const values_core[] = {
        "1",
        "-31",
        "0",
        NULL,
        NULL,
        "-3287181",
        "3287181",
        "11",
        "2",
        "1",
        "1",
        "1",
    };
    static const char *const columns_dual[] = {"DATEDIFF ('2008-01-02','2008-01-01')", "diff"};
    static const char *const values_dual[] = {"1", "1"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const columns_no_backslash[] = {"DATEDIFF('2008-01-02','2008-01-01')"};
    static const char *const values_no_backslash[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "DATEDIFF('2007-12-31 23:59:59', '2007-12-30'), "
                   "DATEDIFF('2010-11-30 23:59:59', '2010-12-31'), "
                   "DATEDIFF('2008-01-02 13:29:17', '2008-01-02 23:59:59'), "
                   "DATEDIFF(NULL, '2008-01-01'), DATEDIFF('2008-01-01', NULL), "
                   "DATEDIFF('1000-01-01', '9999-12-31'), "
                   "DATEDIFF('9999-12-31', '1000-01-01'), "
                   "DATEDIFF('1582-10-15', '1582-10-04'), "
                   "DATEDIFF('2000-03-01', '2000-02-28'), "
                   "DATEDIFF('1900-03-01', '1900-02-28'), "
                   "DATEDIFF('0001-01-01', '0000-12-31'), "
                   "DATEDIFF('0000-03-01', '0000-02-28')",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .context = "no-source datediff",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATEDIFF ('2008-01-02','2008-01-01'), "
                   "DATEDIFF('2008-01-02','2008-01-01') AS diff FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual datediff",
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
            .context = "row count after datediff select",
        }
    );

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT DATEDIFF(\"2008-01-02\",\"2008-01-01\")",
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
            .sql = "SELECT DATEDIFF('2008-01-02','2008-01-01')",
            .columns = columns_no_backslash,
            .column_count = 1U,
            .values = values_no_backslash,
            .row_count = 1U,
            .context = "datediff after NO_BACKSLASH_ESCAPES",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_ok(
        database,
        "DO DATEDIFF('2008-01-02','2008-01-01'), DATEDIFF(NULL,'2008-01-01')",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "datediff do columns");
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "datediff do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "datediff do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "datediff do warnings"
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
            .context = "row count after datediff do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_datediff_and_reopen(void) {
    static const char *const columns_table[] = {"id", "dt_d", "ts_dt", "s_d", "d_literal"};
    static const char *const values_table[] = {
        "1",
        "1",
        "1",
        "3",
        "1",
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
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'not-a-date'",
    };
    static const char *const columns_text_warning[] = {"txt_diff"};
    static const char *const values_text_warning[] = {NULL};
    static const char *const values_text_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: '2001-11-00'",
    };
    static const char *const columns_limited[] = {"id", "days"};
    static const char *const values_limited[] = {"2", NULL, "1", "1"};
    static const char *const columns_reopen[] = {"dt_d"};
    static const char *const values_reopen[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "
        "s VARCHAR(32), txt TEXT, tm TIME"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '2008-01-02', '2008-01-03 13:29:17', "
        "'2008-01-04 23:59:59', '2008-01-05 01:02:03', '2008-01-06', '01:02:03'), "
        "(2, NULL, NULL, NULL, NULL, NULL, NULL), "
        "(3, '0000-00-00', '2001-11-00 00:00:00', NULL, "
        "'not-a-date', '2001-11-00', '02:03:04')",
        NULL
    );

    failures += execute_ok(
        database,
        "SELECT id, DATEDIFF(dt,d) AS dt_d, DATEDIFF(ts,dt) AS ts_dt, "
        "DATEDIFF(s,d) AS s_d, DATEDIFF(d,'2008-01-01') AS d_literal "
        "FROM t ORDER BY id",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            datediff_table_column_count,
            "table columns"
        );
        failures += mylite_test_expect_size(mylite_result_row_count(result), 3U, "table rows");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 1U, "table warnings");
        for (size_t column = 0U; column < datediff_table_column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                columns_table[column],
                "table column"
            );
        }
        for (size_t row = 0U; row < 3U; ++row) {
            for (size_t column = 0U; column < datediff_table_column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    values_table[(row * datediff_table_column_count) + column],
                    "table datediff"
                );
            }
        }
    }
    mylite_result_free(result);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_warnings,
            .row_count = 1U,
            .context = "table datediff warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DATEDIFF(d,'2008-01-01') AS days "
                   "FROM t WHERE id < 3 ORDER BY id DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .context = "table datediff where order limit",
        }
    );
    failures += execute_ok(
        database,
        "SELECT DATEDIFF(txt,'2001-10-31') AS txt_diff FROM t WHERE id = 3",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 1U, "text warning columns");
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 1U, "text warning rows");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 1U, "text warning count");
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, 0U),
            columns_text_warning[0],
            "text warning column"
        );
        failures += expect_result_value(result, 0U, 0U, values_text_warning[0], "text warning");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = sizeof(columns_warnings) / sizeof(columns_warnings[0]),
            .values = values_text_warnings,
            .row_count = 1U,
            .context = "table text zero datediff warnings",
        }
    );

    mylite_close(database);
    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen datediff database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATEDIFF(dt,d) AS dt_d FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "reopened datediff",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_datediff_warnings_and_diagnostics(void) {
    static const char *const columns_zero[] = {
        "zero_pair",
        "partial",
        "year_zero",
        "zero_right",
    };
    static const char *const values_zero[] = {NULL, NULL, "1", NULL};
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_zero_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: '0000-00-00'",
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
    static const char *const columns_invalid[] =
        {"bad_left", "bad_right", "time_only", "null_bad", "bad_null"};
    static const char *const values_invalid[] = {NULL, NULL, NULL, NULL, NULL};
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
        "Incorrect datetime value: 'bad'",
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures +=
        execute_ok(database, "CREATE TABLE t(id INT, d DATE NULL, tm TIME, v VARCHAR(32))", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, '2008-01-02', '01:02:03', '2008-01-03')",
        NULL
    );

    failures += execute_ok(
        database,
        "SELECT DATEDIFF('0000-00-00','0000-00-00') AS zero_pair, "
        "DATEDIFF('2001-11-00','2001-10-31') AS partial, "
        "DATEDIFF('0000-01-02','0000-01-01') AS year_zero, "
        "DATEDIFF('2008-01-02','0000-00-00') AS zero_right",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 4U, "zero columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "zero rows");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 4U, "zero warnings");
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
            .row_count = 4U,
            .context = "zero datediff warnings",
        }
    );

    failures += execute_ok(
        database,
        "SELECT DATEDIFF('not-a-date','2008-01-01') AS bad_left, "
        "DATEDIFF('2008-01-01','not-a-date') AS bad_right, "
        "DATEDIFF('13:29:17','2008-01-01') AS time_only, "
        "DATEDIFF(NULL,'bad') AS null_bad, DATEDIFF('bad',NULL) AS bad_null",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            datediff_invalid_column_count,
            "invalid columns"
        );
        failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, "invalid rows");
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            datediff_invalid_column_count,
            "invalid warnings"
        );
        for (size_t column = 0U; column < datediff_invalid_column_count; ++column) {
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
            .row_count = datediff_invalid_column_count,
            .context = "invalid datediff warnings",
        }
    );

    failures += execute_error(
        database,
        "SELECT DATEDIFF()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DATEDIFF'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF('2008-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DATEDIFF'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF('2008-01-03','2008-01-02','2008-01-01')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'DATEDIFF'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF(missing, d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF(1, '2008-01-01')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATEDIFF() supports only string temporal literals, DATE, DATETIME, "
                            "TIMESTAMP descriptor columns, string descriptor columns, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF(TRUE, FALSE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATEDIFF() supports only string temporal literals, DATE, DATETIME, "
                            "TIMESTAMP descriptor columns, string descriptor columns, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF(DATE_ADD(d, INTERVAL 1 SECOND), d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATEDIFF() supports only string temporal literals, DATE, DATETIME, "
                            "TIMESTAMP descriptor columns, string descriptor columns, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF(tm, d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATEDIFF() does not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATEDIFF(id, d) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATEDIFF() supports only DATE, DATETIME, TIMESTAMP, string "
                            "descriptor columns, string temporal literals, and NULL",
        }
    );
    {
        static const char sql[] = "SELECT DATEDIFF('2008\0-01-01', '2008-01-01')";

        failures += execute_error_len(
            database,
            sql,
            sizeof(sql) - 1U,
            (struct expected_sql_error){
                .code = mysql_error_parse,
                .sqlstate = "42000",
                .message_part = "DATEDIFF() literals do not support NUL bytes",
            }
        );
    }

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
