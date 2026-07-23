#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

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
    core_column_count = 10,
    table_column_count = 7,
    mysql_error_argument_count = 1582,
    mysql_error_unknown_column = 1054,
    mysql_error_not_supported = 1064,
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

static int test_timediff_values_and_file_safety(void);
static int test_timediff_table_backed_and_reopen(void);
static int test_timediff_sql_modes_and_errors(void);
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
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_timediff_values_and_file_safety();
    failures += test_timediff_table_backed_and_reopen();
    failures += test_timediff_sql_modes_and_errors();

    return failures == 0 ? 0 : 1;
}

static int test_timediff_values_and_file_safety(void) {
    static const char *const core_columns[] = {
        "TIMEDIFF('2008-01-02 13:29:17','2008-01-02 13:29:16')",
        "TIMEDIFF('2008-01-02 13:29:16','2008-01-02 13:29:17')",
        "TIMEDIFF('01:02:03','00:00:04')",
        "TIMEDIFF('00:00:04','01:02:03')",
        "TIMEDIFF('-01:02:03','00:00:04')",
        "TIMEDIFF('101:02:03','01:02:03')",
        "TIMEDIFF('2008-01-02 13:29:17','01:02:03')",
        "TIMEDIFF('01:02:03','2008-01-02 13:29:17')",
        "TIMEDIFF(NULL,'bad')",
        "TIMEDIFF('01:02:03',NULL)",
    };
    static const char *const core_values[] = {
        "00:00:01",
        "-00:00:01",
        "01:01:59",
        "-01:01:59",
        "-01:02:07",
        "100:00:00",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const label_columns[] = {
        "TIMEDIFF('01:02:03','00:00:01')",
        "shifted",
    };
    static const char *const label_values[] = {"01:02:02", "00:00:01"};
    static const char *const row_status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    static const char *const warnings_columns[] = {"Level", "Code", "Message"};
    static const char *const clamp_values[] = {"838:59:59", "-838:59:59"};
    static const char *const clamp_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '839:00:00'",
        "Warning",
        "1292",
        "Truncated incorrect time value: '-839:00:00'",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open TIMEDIFF values file"
    );
    if (failures != 0) {
        remove_related_files(path);
        return failures;
    }
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE catalog_guard(id INT)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(database, "DO 0", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "TIMEDIFF('2008-01-02 13:29:17','2008-01-02 13:29:16'),"
                   "TIMEDIFF('2008-01-02 13:29:16','2008-01-02 13:29:17'),"
                   "TIMEDIFF('01:02:03','00:00:04'),"
                   "TIMEDIFF('00:00:04','01:02:03'),"
                   "TIMEDIFF('-01:02:03','00:00:04'),"
                   "TIMEDIFF('101:02:03','01:02:03'),"
                   "TIMEDIFF('2008-01-02 13:29:17','01:02:03'),"
                   "TIMEDIFF('01:02:03','2008-01-02 13:29:17'),"
                   "TIMEDIFF(NULL,'bad'),"
                   "TIMEDIFF('01:02:03',NULL)",
            .columns = core_columns,
            .column_count = core_column_count,
            .values = core_values,
            .row_count = 1U,
            .context = "core TIMEDIFF values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMEDIFF('01:02:03','00:00:01'), "
                   "TIMEDIFF('2008-01-02 13:29:17','2008-01-02 13:29:16') AS shifted "
                   "FROM DUAL",
            .columns = label_columns,
            .column_count = sizeof(label_columns) / sizeof(label_columns[0]),
            .values = label_values,
            .row_count = 1U,
            .context = "TIMEDIFF labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = row_status_columns,
            .column_count = sizeof(row_status_columns) / sizeof(row_status_columns[0]),
            .values = values_after_select,
            .row_count = 1U,
            .context = "row count after TIMEDIFF select",
        }
    );

    failures +=
        execute_ok(database, "DO TIMEDIFF('01:02:03','00:00:01'), TIMEDIFF(NULL,'bad')", &result);
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_column_count(result), 0U, "timediff do columns");
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "timediff do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "timediff do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "timediff do warnings"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = row_status_columns,
            .column_count = sizeof(row_status_columns) / sizeof(row_status_columns[0]),
            .values = values_after_do,
            .row_count = 1U,
            .context = "row count after TIMEDIFF do",
        }
    );

    failures += execute_ok(
        database,
        "SELECT TIMEDIFF('838:59:59','-00:00:01'), TIMEDIFF('-838:59:59','00:00:01')",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            2U,
            "timediff clamp warnings"
        );
        failures += expect_result_value(result, 0U, 0U, clamp_values[0], "positive clamp");
        failures += expect_result_value(result, 0U, 1U, clamp_values[1], "negative clamp");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warnings_columns,
            .column_count = sizeof(warnings_columns) / sizeof(warnings_columns[0]),
            .values = clamp_warnings,
            .row_count = 2U,
            .context = "timediff clamp warnings",
        }
    );

    failures += mylite_test_expect_uint64(
        session->catalog_generation,
        catalog_generation,
        "TIMEDIFF catalog generation"
    );
    failures += mylite_test_expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "TIMEDIFF sqlite schema generation"
    );
    failures += mylite_test_expect_int(
        read_file_at(path, 0, actual_preamble, sizeof(actual_preamble)),
        0,
        "preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "TIMEDIFF preamble"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_timediff_table_backed_and_reopen(void) {
    static const char *const table_columns[] = {
        "id",
        "date_delta",
        "time_delta",
        "datetime_delta",
        "timestamp_delta",
        "date_datetime",
        "time_datetime",
    };
    static const char *const table_values[] = {
        "1",
        "48:00:00",
        "01:01:59",
        "00:00:01",
        "00:00:02",
        NULL,
        NULL,
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const limited_columns[] = {"id", "time_delta"};
    static const char *const limited_values[] = {"1", "01:01:59"};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const invalid_columns[] = {"id", "bad_delta"};
    static const char *const invalid_values[] = {"1", NULL, "2", NULL};
    static const char *const invalid_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'bad'",
    };
    static const char *const clamp_columns[] = {"id", "clamped_delta"};
    static const char *const clamp_values[] = {"1", "838:59:59"};
    static const char *const clamp_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: '840:02:02'",
    };
    static const char *const reopen_columns[] = {"date_delta", "time_delta"};
    static const char *const reopen_values[] = {"48:00:00", "01:01:59"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open TIMEDIFF table file");
    if (failures != 0) {
        remove_related_files(path);
        return failures;
    }
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, d DATE NULL, d2 DATE NULL, tm TIME NULL, tm2 TIME NULL, "
        "dt DATETIME NULL, dt2 DATETIME NULL, ts TIMESTAMP NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1,'2008-01-03','2008-01-01','01:02:03','00:00:04',"
        "'2008-01-02 13:29:17','2008-01-02 13:29:16','2008-01-02 13:29:15'),"
        "(2,NULL,'2008-01-01',NULL,'00:00:04',NULL,'2008-01-02 13:29:16',NULL)",
        NULL
    );

    failures += execute_ok(
        database,
        "SELECT id, TIMEDIFF(d,d2) AS date_delta, TIMEDIFF(tm,tm2) AS time_delta, "
        "TIMEDIFF(dt,dt2) AS datetime_delta, TIMEDIFF(dt,ts) AS timestamp_delta, "
        "TIMEDIFF(d,dt) AS date_datetime, TIMEDIFF(tm,dt) AS time_datetime "
        "FROM t ORDER BY id",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            table_column_count,
            "table columns"
        );
        failures += mylite_test_expect_size(mylite_result_row_count(result), 2U, "table rows");
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 0U, "table warnings");
        for (size_t column = 0U; column < table_column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                table_columns[column],
                "table column"
            );
        }
        for (size_t row = 0U; row < 2U; ++row) {
            for (size_t column = 0U; column < table_column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    table_values[(row * table_column_count) + column],
                    "table timediff"
                );
            }
        }
    }
    mylite_result_free(result);
    result = NULL;

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, TIMEDIFF(tm,tm2) AS time_delta "
                   "FROM t WHERE id = 1 ORDER BY id DESC LIMIT 2",
            .columns = limited_columns,
            .column_count = sizeof(limited_columns) / sizeof(limited_columns[0]),
            .values = limited_values,
            .row_count = 1U,
            .context = "table timediff where order limit",
        }
    );

    failures += execute_ok(
        database,
        "SELECT id, TIMEDIFF(tm,'bad') AS bad_delta FROM t ORDER BY id",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            sizeof(invalid_columns) / sizeof(invalid_columns[0]),
            "table invalid timediff columns"
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            2U,
            "table invalid timediff rows"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            1U,
            "table invalid timediff warnings"
        );
        for (size_t column = 0U; column < 2U; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                invalid_columns[column],
                "table invalid timediff column"
            );
        }
        for (size_t row = 0U; row < 2U; ++row) {
            for (size_t column = 0U; column < 2U; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    invalid_values[(row * 2U) + column],
                    "table invalid timediff"
                );
            }
        }
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = invalid_warnings,
            .row_count = 1U,
            .context = "table invalid timediff warnings",
        }
    );

    failures += execute_ok(
        database,
        "SELECT id, TIMEDIFF(tm,'-838:59:59') AS clamped_delta FROM t WHERE id = 1",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            sizeof(clamp_columns) / sizeof(clamp_columns[0]),
            "table clamped timediff columns"
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            1U,
            "table clamped timediff rows"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            1U,
            "table clamped timediff warnings"
        );
        for (size_t column = 0U; column < 2U; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                clamp_columns[column],
                "table clamped timediff column"
            );
            failures += expect_result_value(
                result,
                0U,
                column,
                clamp_values[column],
                "table clamped timediff"
            );
        }
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = clamp_warnings,
            .row_count = 1U,
            .context = "table clamped timediff warnings",
        }
    );

    mylite_close(database);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen TIMEDIFF file");
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMEDIFF(d,d2) AS date_delta, TIMEDIFF(tm,tm2) AS time_delta "
                   "FROM t WHERE id = 1",
            .columns = reopen_columns,
            .column_count = sizeof(reopen_columns) / sizeof(reopen_columns[0]),
            .values = reopen_values,
            .row_count = 1U,
            .context = "table timediff reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_timediff_sql_modes_and_errors(void) {
    static const char *const whitespace_columns[] = {"TIMEDIFF ('01:02:03','00:00:01')"};
    static const char *const whitespace_values[] = {"01:02:02"};
    static const char *const warnings_columns[] = {"Level", "Code", "Message"};
    static const char *const invalid_values[] = {NULL, NULL};
    static const char *const invalid_warnings[] = {
        "Warning",
        "1292",
        "Truncated incorrect time value: 'bad'",
        "Warning",
        "1292",
        "Truncated incorrect time value: 'bad'",
    };
    char sql_with_nul[] = "SELECT TIMEDIFF('01:02:03', '00:00:0\0')";
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open TIMEDIFF errors file"
    );
    if (failures != 0) {
        remove_related_files(path);
        return failures;
    }
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT, tm TIME, vc VARCHAR(16))", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1,'01:02:03','00:00:01')", NULL);

    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMEDIFF ('01:02:03','00:00:01')",
            .columns = whitespace_columns,
            .column_count = 1U,
            .values = whitespace_values,
            .row_count = 1U,
            .context = "timediff whitespace",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = 'IGNORE_SPACE'", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TIMEDIFF ('01:02:03','00:00:01')",
            .columns = whitespace_columns,
            .column_count = 1U,
            .values = whitespace_values,
            .row_count = 1U,
            .context = "timediff ignore_space whitespace",
        }
    );

    failures += execute_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'", NULL);
    failures += execute_error(
        database,
        "SELECT TIMEDIFF(\"01:02:03\",\"00:00:01\")",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_mode = ''", NULL);

    failures += execute_ok(
        database,
        "SELECT TIMEDIFF('bad','00:00:01'), TIMEDIFF('00:00:01','bad')",
        &result
    );
    if (failures == 0) {
        failures +=
            mylite_test_expect_size(mylite_result_warning_count(result), 2U, "invalid warnings");
        failures += expect_result_value(result, 0U, 0U, invalid_values[0], "invalid left");
        failures += expect_result_value(result, 0U, 1U, invalid_values[1], "invalid right");
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warnings_columns,
            .column_count = sizeof(warnings_columns) / sizeof(warnings_columns[0]),
            .values = invalid_warnings,
            .row_count = 2U,
            .context = "invalid timediff warnings",
        }
    );

    failures += execute_error(
        database,
        "SELECT TIMEDIFF()",
        (struct expected_sql_error){
            .code = mysql_error_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMEDIFF('01:02:03')",
        (struct expected_sql_error){
            .code = mysql_error_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMEDIFF('01:02:03','00:00:01','x')",
        (struct expected_sql_error){
            .code = mysql_error_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMEDIFF(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_not_supported,
            .sqlstate = "42000",
            .message_part = "TIMEDIFF() supports only string temporal literals",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMEDIFF(missing,'00:00:01')",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column",
        }
    );
    failures += execute_error(
        database,
        "SELECT TIMEDIFF(vc,tm) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_not_supported,
            .sqlstate = "42000",
            .message_part = "TIMEDIFF() does not yet support string descriptor columns",
        }
    );
    failures += execute_error_bytes(
        database,
        sql_with_nul,
        sizeof(sql_with_nul) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_not_supported,
            .sqlstate = "42000",
            .message_part = "TIMEDIFF() literals do not support NUL bytes",
        },
        "timediff embedded nul"
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
            "expected success for [%s], got rc=%d error=%d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
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
    int failures = 0;
    int rc = mylite_execute(database, sql, sql_length, &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", context);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, context);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, expected.sql, &result);
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
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += mylite_test_expect_text(
                mylite_result_column_name(result, column),
                expected.columns[column],
                expected.context
            );
        }
        for (size_t row = 0U; row < expected.row_count; ++row) {
            for (size_t column = 0U; column < expected.column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    expected.values[(row * expected.column_count) + column],
                    expected.context
                );
            }
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
    if (actual == NULL) {
        fprintf(stderr, "%s: expected [%s] at %zu,%zu, got NULL\n", context, expected, row, column);
        return 1;
    }
    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    if (path == NULL) {
        return;
    }
    remove(path);
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    fclose(file);
    return read_count == size ? 0 : 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
