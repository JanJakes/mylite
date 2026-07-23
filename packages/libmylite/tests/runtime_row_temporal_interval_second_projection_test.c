#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    mysql_error_unknown_column = 1054,
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

static int test_row_temporal_interval_projection_and_reopen(void);
static int test_row_temporal_interval_diagnostics(void);
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
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_row_temporal_interval_projection_and_reopen();
    failures += test_row_temporal_interval_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_row_temporal_interval_projection_and_reopen(void) {
    static const char *const columns_projection[] = {
        "id",
        "add_d",
        "sub_dt",
        "add_ts_neg",
        "sub_v_neg",
        "add_txt",
        "null_interval",
    };
    static const char *const values_projection[] = {
        "1",
        "2008-01-02 00:00:01",
        "2008-01-02 13:29:16",
        "2008-01-02 13:29:16",
        "2008-01-02 00:00:02",
        "2008-01-02 13:29:19",
        NULL,
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "3",
        "2024-02-29 00:00:01",
        "2024-02-28 23:59:58",
        "2024-02-28 23:59:58",
        NULL,
        NULL,
        NULL,
        "4",
        "9999-12-31 00:00:01",
        "9999-12-31 23:59:58",
        NULL,
        NULL,
        "1000-01-01 00:00:02",
        NULL,
    };
    static const char *const columns_warnings[] = {"Level", "Code", "Message"};
    static const char *const values_warnings[] = {
        "Warning",
        "1292",
        "Incorrect datetime value: 'bad'",
        "Warning",
        "1292",
        "Incorrect datetime value: '2016-07-00'",
        "Warning",
        "1441",
        "Datetime function: datetime field overflow",
    };
    static const char *const columns_limited[] = {"id", "shifted"};
    static const char *const values_limited[] = {"3", NULL, "1", "2008-01-02 13:29:18"};
    static const char *const columns_qualified[] = {"id", "shifted"};
    static const char *const values_qualified[] = {"1", "2008-01-02 13:29:18"};
    static const char *const columns_units[] = {"id", "d_day", "d_minute", "dt_month", "d_week"};
    static const char *const values_units[] = {
        "1",
        "2008-01-03",
        "2008-01-02 00:01:00",
        "2008-02-02 13:29:17",
        "2008-01-09",
        "3",
        "2024-03-01",
        "2024-02-29 00:01:00",
        "2024-03-28 23:59:59",
        "2024-03-07",
    };
    static const char *const columns_reopen[] = {"shifted"};
    static const char *const values_reopen[] = {"2008-01-02 13:29:18"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "projection", path, sizeof(path));
    mylite_file_preamble_init(expected_preamble);
    failures += execute_ok(
        database,
        "CREATE TABLE t("
        "id INT, d DATE NULL, dt DATETIME NULL, ts TIMESTAMP NULL, "
        "v VARCHAR(32) NULL, txt TEXT NULL"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES "
        "(1, '2008-01-02', '2008-01-02 13:29:17', '2008-01-02 13:29:17', "
        "'2008-01-02', '2008-01-02 13:29:17'), "
        "(2, NULL, NULL, NULL, NULL, NULL), "
        "(3, '2024-02-29', '2024-02-28 23:59:59', '2024-02-28 23:59:59', "
        "'bad', '2016-07-00'), "
        "(4, '9999-12-31', '9999-12-31 23:59:59', NULL, "
        "'9999-12-31 23:59:59', '1000-01-01 00:00:00')",
        NULL
    );

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "DATE_ADD(d, INTERVAL 1 SECOND) AS add_d, "
                   "DATE_SUB(dt, INTERVAL 1 SECOND) AS sub_dt, "
                   "ADDDATE(ts, INTERVAL -1 SECOND) AS add_ts_neg, "
                   "SUBDATE(v, INTERVAL -2 SECOND) AS sub_v_neg, "
                   "DATE_ADD(txt, INTERVAL 2 SECOND) AS add_txt, "
                   "DATE_ADD(dt, INTERVAL NULL SECOND) AS null_interval "
                   "FROM t ORDER BY id",
            .columns = columns_projection,
            .column_count = sizeof(columns_projection) / sizeof(columns_projection[0]),
            .values = values_projection,
            .row_count = 4U,
            .warning_count = 3U,
            .context = "row temporal interval projection",
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
            .warning_count = 0U,
            .context = "row temporal interval warnings",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DATE_ADD(txt, INTERVAL 1 SECOND) AS shifted "
                   "FROM t WHERE id IN (1, 3) ORDER BY txt DESC LIMIT 2",
            .columns = columns_limited,
            .column_count = sizeof(columns_limited) / sizeof(columns_limited[0]),
            .values = values_limited,
            .row_count = 2U,
            .warning_count = 1U,
            .context = "row temporal interval where order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, DATE_ADD(t.dt, INTERVAL 1 SECOND) AS shifted "
                   "FROM t WHERE id = 1",
            .columns = columns_qualified,
            .column_count = sizeof(columns_qualified) / sizeof(columns_qualified[0]),
            .values = values_qualified,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row temporal interval qualified column",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, "
                   "DATE_ADD(d, INTERVAL 1 DAY) AS d_day, "
                   "DATE_ADD(d, INTERVAL 1 MINUTE) AS d_minute, "
                   "DATE_ADD(dt, INTERVAL 1 MONTH) AS dt_month, "
                   "DATE_ADD(d, INTERVAL '1' WEEK) AS d_week "
                   "FROM t WHERE id IN (1, 3) ORDER BY id",
            .columns = columns_units,
            .column_count = sizeof(columns_units) / sizeof(columns_units[0]),
            .values = values_units,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "row temporal interval core units",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read row temporal interval preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "row temporal interval leaves preamble unchanged"
    );
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen projection database"
    );
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATE_ADD(dt, INTERVAL 1 SECOND) AS shifted FROM t WHERE id = 1",
            .columns = columns_reopen,
            .column_count = sizeof(columns_reopen) / sizeof(columns_reopen[0]),
            .values = values_reopen,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row temporal interval reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_row_temporal_interval_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE t(id INT, d DATE, dt DATETIME, tm TIME)", NULL);
    failures += execute_ok(
        database,
        "INSERT INTO t VALUES (1, '2008-01-02', '2008-01-02 13:29:17', '01:02:03')",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD(missing, INTERVAL 1 SECOND) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD(tm, INTERVAL 1 SECOND) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_ADD() does not yet support TIME values",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD(id, INTERVAL 1 SECOND) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE_ADD() supports only DATE, DATETIME, TIMESTAMP, string "
                            "descriptor columns, string temporal literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD(dt, INTERVAL id SECOND) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_ADD() INTERVAL SECOND supports only signed integer literals, exact signed "
                "integer string literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_ADD(dt, INTERVAL 1+1 SECOND) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_ADD() INTERVAL SECOND supports only signed integer literals, exact signed "
                "integer string literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT DATE_SUB(dt, INTERVAL '1x' SECOND) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "DATE_SUB() INTERVAL SECOND supports only signed integer literals, exact signed "
                "integer string literals, and NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT ADDDATE(dt, INTERVAL 9223372036854775808 SECOND) FROM t",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "ADDDATE() INTERVAL SECOND literals must fit the signed 64-bit range",
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
