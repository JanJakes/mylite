#include <mylite/mylite.h>

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
    test_path_suffix_capacity = 16,
    show_columns_field_count = 6,
    numeric_temporal_column_count = 5,
    numeric_temporal_insert_warning_count = 9,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_incorrect_date_value = 1292,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

static int test_relaxed_temporal_dml_defaults_and_persistence(void);
static int test_relaxed_temporal_strict_diagnostics(void);
static int test_relaxed_temporal_independent_time_zones(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_relaxed_temporal_dml_defaults_and_persistence();
    failures += test_relaxed_temporal_strict_diagnostics();
    failures += test_relaxed_temporal_independent_time_zones();

    return failures == 0 ? 0 : 1;
}

static int test_relaxed_temporal_dml_defaults_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "",
        "d",
        "date",
        "YES",
        "",
        "2024-01-02",
        "",
        "dt",
        "datetime",
        "YES",
        "",
        "2024-01-02 00:34:05",
        "",
        "ts",
        "timestamp",
        "YES",
        "",
        "2024-01-02 03:04:05",
        "",
    };
    static const char *const show_create_rows[] = {
        "temporal",
        "CREATE TABLE `temporal` (\n"
        "  `id` int NOT NULL,\n"
        "  `d` date DEFAULT '2024-01-02',\n"
        "  `dt` datetime DEFAULT '2024-01-02 00:34:05',\n"
        "  `ts` timestamp NULL DEFAULT '2024-01-02 03:04:05',\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const default_rows[] = {
        "1",
        "2024-01-02",
        "2024-01-02 00:34:05",
        "2024-01-02 03:04:05",
    };
    static const char *const insert_warning_rows[] = {
        "Note",
        "1265",
        "Data truncated for column 'd' at row 1",
        "Warning",
        "1265",
        "Data truncated for column 'ts' at row 1",
    };
    static const char *const inserted_rows[] = {
        "2",
        "2024-01-03",
        "2024-01-03 00:02:03",
        "2024-01-03 01:02:03",
    };
    static const char *const single_digit_hour_rows[] = {
        "1",
        "2016-04-20 04:26:20",
        "2016-04-20 04:26:20",
        "2",
        "2016-04-21 05:06:07",
        "2016-04-21 05:06:07",
    };
    static const char *const update_z_warning_rows[] = {
        "Warning",
        "1265",
        "Data truncated for column 'd' at row 1",
    };
    static const char *const replace_warning_rows[] = {
        "Note",
        "1265",
        "Data truncated for column 'd' at row 1",
    };
    static const char *const final_rows[] = {
        "1",
        "2024-01-04",
        "2024-01-02 00:34:05",
        "2024-01-02 03:04:05",
        "2",
        "2024-01-04",
        "2024-01-05 01:02:03",
        "2024-01-04 22:32:03",
    };
    static const char *const numeric_temporal_rows[] = {
        "10",
        "0000-00-00",
        "00:00:01",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
        "11",
        "0000-00-00",
        "-00:01:23",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
        "12",
        "0000-00-00",
        "00:01:23",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open relaxed file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET time_zone = '+00:00'");
    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_result(
        database,
        "CREATE TABLE temporal ("
        "id INT PRIMARY KEY, "
        "d DATE DEFAULT '2024-01-02T03:04:05', "
        "dt DATETIME DEFAULT '2024-01-02 03:04:05+02:30', "
        "ts TIMESTAMP NULL DEFAULT '2024-01-02T03:04:05Z')",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM temporal",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 4U,
            .context = "relaxed temporal SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE temporal",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "relaxed temporal SHOW CREATE TABLE",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO temporal (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal WHERE id = 1",
            .values = default_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "relaxed temporal defaults",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO temporal VALUES "
        "(2, '2024-01-03T01:02:03', "
        "'2024-01-03T01:02:03+01:00', "
        "'2024-01-03T01:02:03Z')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = insert_warning_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "relaxed temporal insert warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal WHERE id = 2",
            .values = inserted_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "relaxed temporal inserted row",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE temporal_single_hour ("
        "id INT PRIMARY KEY, "
        "dt DATETIME DEFAULT '2016-04-20 4:26:20', "
        "ts TIMESTAMP NULL DEFAULT '2016-04-20T4:26:20')"
    );
    failures += expect_dml_ok(database, "INSERT INTO temporal_single_hour (id) VALUES (1)", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO temporal_single_hour VALUES "
        "(2, '2016-04-20 4:26:20', '2016-04-20T4:26:20')",
        1
    );
    failures += expect_dml_ok(
        database,
        "UPDATE temporal_single_hour "
        "SET dt = '2016-04-21 5:06:07', ts = '2016-04-21T5:06:07' "
        "WHERE id = 2",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, dt, ts FROM temporal_single_hour ORDER BY id",
            .values = single_digit_hour_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "single digit hour temporal storage",
        }
    );
    failures += expect_statement_ok(database, "SET time_zone = '+02:00'");
    failures += expect_dml_ok(
        database,
        "UPDATE temporal SET dt = '2024-01-04T01:02:03+00:00' WHERE id = 2",
        1
    );
    failures += expect_statement_ok(database, "SET time_zone = '+00:00'");
    failures += expect_dml_result(
        database,
        "UPDATE temporal SET d = '2024-01-04T01:02:03Z' WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = update_z_warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "relaxed temporal update warning",
        }
    );
    failures += expect_dml_result(
        database,
        "REPLACE INTO temporal VALUES "
        "(2, '2024-01-05 01:02:03+02:30', "
        "'2024-01-05T01:02:03', "
        "'2024-01-05T01:02:03+02:30')",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = replace_warning_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "relaxed temporal replace warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal ORDER BY id",
            .values = final_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "relaxed temporal final rows",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE temporal_numbers("
        "id INT PRIMARY KEY, d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO temporal_numbers VALUES "
        "(10, TRUE, TRUE, TRUE, TRUE), "
        "(11, FALSE, FALSE, FALSE, FALSE), "
        "(12, 0, 123, 0, 0)",
        (struct expected_dml_result){
            .affected_rows = 3,
            .warning_count = numeric_temporal_insert_warning_count,
        }
    );
    failures += expect_dml_result(
        database,
        "UPDATE temporal_numbers SET d = FALSE, tm = -123, dt = FALSE, ts = FALSE "
        "WHERE id = 11",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, tm, dt, ts FROM temporal_numbers ORDER BY id",
            .values = numeric_temporal_rows,
            .column_count = numeric_temporal_column_count,
            .row_count = 3U,
            .context = "relaxed numeric temporal rows",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        MYLITE_FILE_PREAMBLE_SIZE,
        "relaxed temporal file preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen relaxed file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal ORDER BY id",
            .values = final_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "relaxed temporal reopened rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_relaxed_temporal_strict_diagnostics(void) {
    static const char *const accepted_strict_rows[] = {
        "1",
        "2024-01-02",
        "2024-01-02 00:34:05",
        "2024-01-02 03:04:05",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "strict") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open strict db");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET time_zone = '+00:00'");
    failures += expect_statement_result(
        database,
        "SET sql_mode = 'STRICT_TRANS_TABLES'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE temporal (id INT PRIMARY KEY, d DATE, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_dml_result(
        database,
        "INSERT INTO temporal VALUES "
        "(1, '2024-01-02T03:04:05', "
        "'2024-01-02T03:04:05+02:30', "
        "'2024-01-02T03:04:05')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal",
            .values = accepted_strict_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "strict relaxed temporal accepted row",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO temporal VALUES (2, '2024-01-02T03:04:05Z', NULL, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '2024-01-02T03:04:05Z' for column 'd' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO temporal VALUES "
        "(2, '2024-01-02', '2024-01-02T03:04:05Z', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part =
                "Incorrect datetime value: '2024-01-02T03:04:05Z' for column 'dt' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE temporal SET dt = '2024-01-02T03:04:05Z' WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part =
                "Incorrect datetime value: '2024-01-02T03:04:05Z' for column 'dt' at row 1",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_datetime_default "
        "(v DATETIME DEFAULT '2024-01-02T03:04:05Z')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_date_default (v DATE DEFAULT '2024-01-02T03:04:05Z')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO temporal VALUES "
        "(3, '2024-01-02', '2024-01-02T03:04:05+1:00', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part =
                "Incorrect datetime value: '2024-01-02T03:04:05+1:00' for column 'dt' at row 1",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_relaxed_temporal_independent_time_zones(void) {
    static const char *const utc_rows[] = {"2024-01-02 00:34:05"};
    static const char *const plus_two_rows[] = {"2024-01-02 02:34:05"};
    char utc_path[test_path_capacity];
    char plus_two_path[test_path_capacity];
    mylite_db *utc = NULL;
    mylite_db *plus_two = NULL;
    int failures = 0;

    if (make_test_path(utc_path, sizeof(utc_path), "utc") != 0 ||
        make_test_path(plus_two_path, sizeof(plus_two_path), "plus_two") != 0) {
        return 1;
    }
    remove_related_files(utc_path);
    remove_related_files(plus_two_path);

    failures += expect_int(mylite_open(utc_path, &utc), MYLITE_OK, "open UTC db");
    failures += expect_int(mylite_open(plus_two_path, &plus_two), MYLITE_OK, "open +02 db");
    failures += expect_statement_ok(utc, "CREATE DATABASE app");
    failures += expect_statement_ok(utc, "USE app");
    failures += expect_statement_ok(plus_two, "CREATE DATABASE app");
    failures += expect_statement_ok(plus_two, "USE app");
    failures += expect_statement_ok(utc, "SET time_zone = '+00:00'");
    failures += expect_statement_ok(plus_two, "SET time_zone = '+02:00'");
    failures += expect_statement_ok(utc, "CREATE TABLE t (v DATETIME)");
    failures += expect_statement_ok(plus_two, "CREATE TABLE t (v DATETIME)");
    failures += expect_dml_ok(utc, "INSERT INTO t VALUES ('2024-01-02T03:04:05+02:30')", 1);
    failures += expect_dml_ok(plus_two, "INSERT INTO t VALUES ('2024-01-02T03:04:05+02:30')", 1);
    failures += expect_query_values(
        utc,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = utc_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "UTC handle relaxed offset row",
        }
    );
    failures += expect_query_values(
        plus_two,
        (struct expected_query){
            .sql = "SELECT v FROM t",
            .values = plus_two_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "+02 handle relaxed offset row",
        }
    );

    mylite_close(utc);
    mylite_close(plus_two);
    remove_related_files(utc_path);
    remove_related_files(plus_two_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s failed: %d %s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    *out_result = result;
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s unexpectedly succeeded\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = 0;

    if (execute_ok(database, sql, &result) != 0) {
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 0U}
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = 0;

    if (execute_ok(database, sql, &result) != 0) {
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){.affected_rows = affected_rows, .warning_count = 0U}
    );
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;

    if (execute_ok(database, query.sql, &result) != 0) {
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures +=
        expect_size(mylite_result_warning_count(result), query.warning_count, query.context);
    for (size_t row = 0; row < query.row_count; ++row) {
        for (size_t column = 0; column < query.column_count; ++column) {
            size_t index = (row * query.column_count) + column;
            failures +=
                expect_result_value(result, row, column, query.values[index], query.context);
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

    if (expected == NULL && actual == NULL) {
        return 0;
    }
    if (expected == NULL || actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected value [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_relaxed_temporal_dml_literals_%d_%s.mylite",
        directory,
        current_process_id(),
        name
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %s\n", path);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
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
            "%s: expected text [%s], got [%s]\n",
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
