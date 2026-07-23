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

static int test_no_source_dual_and_do_week_temporal_functions(void);
static int test_week_temporal_warnings(void);
static int test_table_backed_week_temporal_functions_and_reopen(void);
static int test_week_temporal_diagnostics(void);
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

    failures += test_no_source_dual_and_do_week_temporal_functions();
    failures += test_week_temporal_warnings();
    failures += test_table_backed_week_temporal_functions_and_reopen();
    failures += test_week_temporal_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_week_temporal_functions(void) {
    static const char *const columns_core[] = {
        "WEEK('2008-02-20')",
        "WEEK('2008-02-20',0)",
        "WEEK('2008-02-20',1)",
        "WEEK('2008-12-31',1)",
        "WEEK('2000-01-01',0)",
        "WEEK('2000-01-01',2)",
        "YEARWEEK('2000-01-01')",
        "YEARWEEK('1987-01-01')",
        "WEEKDAY('2008-02-03 22:23:00')",
        "WEEKDAY('2007-11-06')",
        "WEEKOFYEAR('2008-02-20')",
        "WEEK(NULL)",
        "WEEKDAY(NULL)",
        "WEEKOFYEAR(NULL)",
        "YEARWEEK(NULL)",
    };
    static const char *const values_core[] = {
        "7",
        "7",
        "8",
        "53",
        "0",
        "52",
        "199952",
        "198652",
        "6",
        "1",
        "8",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const columns_modes[] = {
        "WEEK('2000-01-01',0)",
        "WEEK('2000-01-01',1)",
        "WEEK('2000-01-01',2)",
        "WEEK('2000-01-01',3)",
        "WEEK('2000-01-01',4)",
        "WEEK('2000-01-01',5)",
        "WEEK('2000-01-01',6)",
        "WEEK('2000-01-01',7)",
        "YEARWEEK('2008-12-31',0)",
        "YEARWEEK('2008-12-31',1)",
        "YEARWEEK('2008-12-31',2)",
        "YEARWEEK('2008-12-31',3)",
    };
    static const char *const values_modes[] = {
        "0",
        "0",
        "52",
        "52",
        "0",
        "0",
        "52",
        "52",
        "200852",
        "200901",
        "200852",
        "200901",
    };
    static const char *const columns_mode_coercion[] = {
        "WEEK('2008-02-20', -1)",
        "WEEK('2008-02-20', 8)",
        "WEEK('2008-02-20', 9)",
        "WEEK('2008-02-20', TRUE)",
        "WEEK('2008-02-20', FALSE)",
        "WEEK('2008-02-20', NULL)",
        "YEARWEEK('2008-02-20', -1)",
        "YEARWEEK('2008-02-20', 9)",
    };
    static const char *const values_mode_coercion[] = {
        "7",
        "7",
        "8",
        "8",
        "7",
        "7",
        "200807",
        "200808",
    };
    static const char *const columns_mode_boundaries[] = {
        "WEEK('2008-02-20', 9223372036854775807)",
        "WEEK('2008-02-20', -9223372036854775808)",
        "YEARWEEK('2008-02-20', 9223372036854775807)",
        "YEARWEEK('2008-02-20', -9223372036854775808)",
    };
    static const char *const values_mode_boundaries[] = {
        "7",
        "7",
        "200807",
        "200807",
    };
    static const char *const columns_dual[] = {
        "WEEK ('2007-02-03')",
        "weekday_value",
        "calendar_week",
        "year_week",
    };
    static const char *const values_dual[] = {"4", "5", "8", "200807"};
    static const char *const columns_row_status[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const columns_no_backslash[] = {
        "WEEK('2007-02-03')",
        "WEEKDAY('2007-02-03')",
        "WEEKOFYEAR('2008-02-20')",
        "YEARWEEK('2008-02-20')",
    };
    static const char *const values_no_backslash[] = {"4", "5", "8", "200807"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WEEK('2008-02-20'), WEEK('2008-02-20',0), "
                   "WEEK('2008-02-20',1), WEEK('2008-12-31',1), "
                   "WEEK('2000-01-01',0), WEEK('2000-01-01',2), "
                   "YEARWEEK('2000-01-01'), YEARWEEK('1987-01-01'), "
                   "WEEKDAY('2008-02-03 22:23:00'), WEEKDAY('2007-11-06'), "
                   "WEEKOFYEAR('2008-02-20'), WEEK(NULL), WEEKDAY(NULL), "
                   "WEEKOFYEAR(NULL), YEARWEEK(NULL)",
            .columns = columns_core,
            .column_count = sizeof(columns_core) / sizeof(columns_core[0]),
            .values = values_core,
            .row_count = 1U,
            .context = "no-source week temporal functions",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WEEK('2000-01-01',0), WEEK('2000-01-01',1), "
                   "WEEK('2000-01-01',2), WEEK('2000-01-01',3), "
                   "WEEK('2000-01-01',4), WEEK('2000-01-01',5), "
                   "WEEK('2000-01-01',6), WEEK('2000-01-01',7), "
                   "YEARWEEK('2008-12-31',0), YEARWEEK('2008-12-31',1), "
                   "YEARWEEK('2008-12-31',2), YEARWEEK('2008-12-31',3)",
            .columns = columns_modes,
            .column_count = sizeof(columns_modes) / sizeof(columns_modes[0]),
            .values = values_modes,
            .row_count = 1U,
            .context = "week temporal mode table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WEEK('2008-02-20', -1), WEEK('2008-02-20', 8), "
                   "WEEK('2008-02-20', 9), WEEK('2008-02-20', TRUE), "
                   "WEEK('2008-02-20', FALSE), WEEK('2008-02-20', NULL), "
                   "YEARWEEK('2008-02-20', -1), YEARWEEK('2008-02-20', 9)",
            .columns = columns_mode_coercion,
            .column_count = sizeof(columns_mode_coercion) / sizeof(columns_mode_coercion[0]),
            .values = values_mode_coercion,
            .row_count = 1U,
            .context = "week temporal mode coercion",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WEEK('2008-02-20', 9223372036854775807), "
                   "WEEK('2008-02-20', -9223372036854775808), "
                   "YEARWEEK('2008-02-20', 9223372036854775807), "
                   "YEARWEEK('2008-02-20', -9223372036854775808)",
            .columns = columns_mode_boundaries,
            .column_count = sizeof(columns_mode_boundaries) / sizeof(columns_mode_boundaries[0]),
            .values = values_mode_boundaries,
            .row_count = 1U,
            .context = "week temporal mode signed boundaries",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WEEK ('2007-02-03'), WEEKDAY ('2007-02-03') AS weekday_value, "
                   "WEEKOFYEAR ('2008-02-20') AS calendar_week, "
                   "YEARWEEK ('2008-02-20') AS year_week FROM DUAL",
            .columns = columns_dual,
            .column_count = sizeof(columns_dual) / sizeof(columns_dual[0]),
            .values = values_dual,
            .row_count = 1U,
            .context = "dual week temporal functions",
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
            .context = "row count after week temporal select",
        }
    );

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT WEEKDAY(\"2007-02-03\")",
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
            .sql = "SELECT WEEK('2007-02-03'), WEEKDAY('2007-02-03'), "
                   "WEEKOFYEAR('2008-02-20'), YEARWEEK('2008-02-20')",
            .columns = columns_no_backslash,
            .column_count = sizeof(columns_no_backslash) / sizeof(columns_no_backslash[0]),
            .values = values_no_backslash,
            .row_count = 1U,
            .context = "week temporal functions after NO_BACKSLASH_ESCAPES",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_ok(
        database,
        "DO WEEK('2007-02-03'), WEEKDAY(NULL), WEEKOFYEAR('2008-02-20'), "
        "YEARWEEK('2008-02-20')",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "week do columns");
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "week do rows");
        failures +=
            mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "week do affected");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "week do warnings");
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
            .context = "row count after week temporal do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_week_temporal_warnings(void) {
    enum { zero_temporal_warning_count = 8 };

    static const char *const columns_zero[] = {
        "week_zero",
        "weekday_zero",
        "weekofyear_zero",
        "yearweek_zero",
        "week_partial",
        "weekday_partial",
        "weekofyear_partial",
        "yearweek_partial",
        "week_year_zero",
        "weekday_year_zero",
        "weekofyear_year_zero",
        "yearweek_year_zero",
    };
    static const char *const values_zero[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "1",
        "0",
        "1",
        "1",
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_zero_warnings[] = {
        "Warning", "1292", "Incorrect datetime value: '0000-00-00'",
        "Warning", "1292", "Incorrect datetime value: '0000-00-00'",
        "Warning", "1292", "Incorrect datetime value: '0000-00-00'",
        "Warning", "1292", "Incorrect datetime value: '0000-00-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-11-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-11-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-11-00'",
        "Warning", "1292", "Incorrect datetime value: '2001-11-00'",
    };
    static const char *const columns_warning_count[] = {"@@warning_count"};
    static const char *const values_warning_count[] = {"8"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "warnings", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT WEEK('0000-00-00') AS week_zero, "
                   "WEEKDAY('0000-00-00') AS weekday_zero, "
                   "WEEKOFYEAR('0000-00-00') AS weekofyear_zero, "
                   "YEARWEEK('0000-00-00') AS yearweek_zero, "
                   "WEEK('2001-11-00') AS week_partial, "
                   "WEEKDAY('2001-11-00') AS weekday_partial, "
                   "WEEKOFYEAR('2001-11-00') AS weekofyear_partial, "
                   "YEARWEEK('2001-11-00') AS yearweek_partial, "
                   "WEEK('0000-01-02') AS week_year_zero, "
                   "WEEKDAY('0000-01-02') AS weekday_year_zero, "
                   "WEEKOFYEAR('0000-01-02') AS weekofyear_year_zero, "
                   "YEARWEEK('0000-01-02') AS yearweek_year_zero",
            .columns = columns_zero,
            .column_count = sizeof(columns_zero) / sizeof(columns_zero[0]),
            .values = values_zero,
            .row_count = 1U,
            .warning_count = zero_temporal_warning_count,
            .context = "week zero date values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = columns_warnings,
            .column_count = 3U,
            .values = values_zero_warnings,
            .row_count = zero_temporal_warning_count,
            .context = "week zero date warnings",
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
            .warning_count = 0U,
            .context = "week zero warning count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_week_temporal_functions_and_reopen(void) {
    static const char *const columns_table[] = {
        "id",
        "WEEK(d)",
        "WEEKDAY(dt)",
        "WEEKOFYEAR(ts)",
        "YEARWEEK(s)",
        "WEEK(txt, 3)",
    };
    static const char *const values_table[] = {
        "1", "4",  "2",  "5",  "200704", "8",  "2", NULL, NULL, NULL, NULL,     NULL,
        "3", NULL, NULL, NULL, NULL,     NULL, "4", "1",  "0",  "52", "199952", "1",
    };
    static const char *const columns_reopen[] = {"YEARWEEK(s)", "WEEKOFYEAR(txt)"};
    static const char *const values_reopen[] = {"199952", "1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "
        "s VARCHAR(32), txt TEXT, tm TIME)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2007-02-03','2008-12-31 23:59:59','2008-02-03 13:29:17',"
        "'2007-02-03','2008-02-20','13:29:17'),"
        "(2,NULL,NULL,NULL,NULL,NULL,NULL),"
        "(3,'0000-00-00','2001-11-00 00:00:00',NULL,'not-a-date','2001-11-00','01:02:03'),"
        "(4,'2000-01-03','2000-01-03 01:02:03','2000-01-01 00:00:00',"
        "'2000-01-01','0000-01-02','04:05:06')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, WEEK(d), WEEKDAY(dt), WEEKOFYEAR(ts), YEARWEEK(s), "
                   "WEEK(txt, 3) FROM t ORDER BY id",
            .columns = columns_table,
            .column_count = sizeof(columns_table) / sizeof(columns_table[0]),
            .values = values_table,
            .row_count = 4U,
            .warning_count = 2U,
            .context = "table-backed week temporal functions",
        }
    );
    mylite_close(database);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen week database");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT YEARWEEK(s), WEEKOFYEAR(txt) FROM t WHERE id = 4",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .context = "week temporal reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_week_temporal_diagnostics(void) {
    char path[test_path_capacity];
    char nul_sql[] = "SELECT WEEK('2008\0-01-01')";
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, d DATE, tm TIME)", NULL);
    failures += execute_error(
        database,
        "SELECT WEEK()",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEEKDAY()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'WEEKDAY'",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEEKOFYEAR('2008-01-02', 'x')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'WEEKOFYEAR'",
        }
    );
    failures += execute_error(
        database,
        "SELECT YEARWEEK()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'YEARWEEK'",
        }
    );
    failures += execute_error(
        database,
        "SELECT YEARWEEK('2008-01-02', 3, 4)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'YEARWEEK'",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEEK(missing) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEEK(tm) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "week temporal functions do not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEEK(20070203)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "week temporal functions support only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEEK('2008-02-20', '3')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "mode support is limited to integer, boolean, and NULL literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT WEEK('2008-02-20', 9223372036854775808)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "mode literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error(
        database,
        "SELECT YEARWEEK('2008-02-20', -9223372036854775809)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "mode literals must fit the signed 64-bit range",
        }
    );
    failures += execute_error_len(
        database,
        nul_sql,
        sizeof(nul_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "week temporal function literals do not support NUL bytes",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = mylite_test_expect_int(rc, MYLITE_OK, sql);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: %s\n", sql, mylite_errmsg(database));
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return failures;
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
        fprintf(stderr, "%s: expected error, got success\n", sql);
        failures += 1;
    } else {
        failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
        failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
        failures +=
            mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    }
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
        failures += execute_ok(*out_database, "SET SESSION sql_mode = ''", NULL);
    }
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
