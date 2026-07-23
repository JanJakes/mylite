#include "mylite_test_support.h"

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
    show_columns_column_count = 6,
    zero_in_date_show_columns_row_count = 5,
    combined_zero_modes_show_columns_row_count = 3,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_incorrect_timestamp_value = 1525,
    mysql_error_incorrect_date_value = 1292,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_zero_temporal_dml_modes(void);
static int test_zero_temporal_defaults_predicates_and_persistence(void);
static int test_zero_temporal_alter_defaults(void);
static int test_zero_temporal_update_replace_copy(void);
static int test_zero_temporal_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
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

    failures += test_zero_temporal_dml_modes();
    failures += test_zero_temporal_defaults_predicates_and_persistence();
    failures += test_zero_temporal_alter_defaults();
    failures += test_zero_temporal_update_replace_copy();
    failures += test_zero_temporal_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_zero_temporal_dml_modes(void) {
    static const char *const full_zero_rows[] = {
        "1",
        "0000-00-00",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    static const char *const partial_zero_rows[] = {
        "5",
        "2024-00-01",
        "2024-01-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    static const char *const adjusted_rows[] = {
        "6",
        "0000-00-00",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    static const char *const invalid_date_rows[] = {
        "9",
        "2024-02-31",
        "2024-02-31 00:00:00",
    };
    static const char *const zero_count_rows[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "dml_modes") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open DML modes file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE temporal_modes (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL)"
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_dml_ok(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(1, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal_modes",
            .values = full_zero_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "empty sql_mode full zero rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM temporal_modes", 1);

    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'");
    failures += expect_dml_ok(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(2, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal_modes",
            .values =
                (const char *const[]){
                    "2",
                    "0000-00-00",
                    "0000-00-00 00:00:00",
                    "0000-00-00 00:00:00",
                },
            .column_count = 4U,
            .row_count = 1U,
            .context = "strict without zero modes full zero rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM temporal_modes", 1);

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures += expect_dml_result(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(3, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal_modes",
            .values =
                (const char *const[]){
                    "3",
                    "0000-00-00",
                    "0000-00-00 00:00:00",
                    "0000-00-00 00:00:00",
                },
            .column_count = 4U,
            .row_count = 1U,
            .context = "nonstrict NO_ZERO_DATE full zero rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM temporal_modes", 1);

    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE'");
    failures += execute_error(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(4, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '0000-00-00' for column 'd' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM temporal_modes",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "strict NO_ZERO_DATE rejected row count",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_ALL_TABLES,NO_ZERO_DATE'");
    failures += execute_error(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(11, '0000-00-00', '0000-00-00 00:00:00', '0000-00-00 00:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '0000-00-00' for column 'd' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM temporal_modes",
            .values = zero_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "strict all NO_ZERO_DATE rejected row count",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_dml_ok(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(5, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal_modes",
            .values = partial_zero_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "empty sql_mode partial zero rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM temporal_modes", 1);

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_IN_DATE'");
    failures += expect_dml_result(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(6, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal_modes",
            .values = adjusted_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "nonstrict NO_ZERO_IN_DATE adjusted rows",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM temporal_modes", 1);

    failures +=
        execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE'");
    failures += execute_error(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(7, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '2024-00-01' for column 'd' at row 1",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_dml_result(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(8, '2024-02-31', '2024-02-31 00:00:00', '2024-02-31 00:00:00')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM temporal_modes",
            .values =
                (const char *const[]){
                    "8",
                    "0000-00-00",
                    "0000-00-00 00:00:00",
                    "0000-00-00 00:00:00",
                },
            .column_count = 4U,
            .row_count = 1U,
            .context = "nonstrict invalid canonical rows",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE invalid_dates (id INT, d DATE, dt DATETIME)");
    failures +=
        execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES,ALLOW_INVALID_DATES'");
    failures += expect_dml_ok(
        database,
        "INSERT INTO invalid_dates VALUES (9, '2024-02-31', '2024-02-31 00:00:00')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt FROM invalid_dates",
            .values = invalid_date_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "ALLOW_INVALID_DATES invalid date rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO temporal_modes VALUES "
        "(10, '2024-02-29', '2024-02-29 00:00:00', '2024-02-31 00:00:00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part =
                "Incorrect datetime value: '2024-02-31 00:00:00' for column 'ts' at row 1",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_zero_temporal_defaults_predicates_and_persistence(void) {
    static const char *const zero_default_rows[] = {
        "1",
        "0000-00-00",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    static const char *const partial_default_rows[] = {
        "1",
        "0000-00-00",
        "0000-00-00 00:00:00",
    };
    static const char *const wp_show_columns_rows[] = {
        "d",
        "date",
        "NO",
        "",
        "0000-00-00",
        "",
        "dt",
        "datetime",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
        "ts",
        "timestamp",
        "NO",
        "",
        "0000-00-00 00:00:00",
        "",
    };
    static const char *const wp_show_create_rows[] = {
        "wp_defaults",
        "CREATE TABLE `wp_defaults` (\n"
        "  `d` date NOT NULL DEFAULT '0000-00-00',\n"
        "  `dt` datetime NOT NULL DEFAULT '0000-00-00 00:00:00',\n"
        "  `ts` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const wp_default_rows[] = {
        "0000-00-00",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    static const char *const one_count_rows[] = {"1"};
    static const char *const wp_datetime_default_rows[] = {"0000-00-00 00:00:00"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "defaults_predicates") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open defaults file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE zero_defaults ("
        "id INT, "
        "d DATE DEFAULT '0000-00-00', "
        "dt DATETIME DEFAULT '0000-00-00 00:00:00', "
        "ts TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00')"
    );
    failures += expect_dml_ok(database, "INSERT INTO zero_defaults (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM zero_defaults",
            .values = zero_default_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "zero temporal defaults",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_defaults ("
        "d DATE NOT NULL DEFAULT '0000-00-00', "
        "dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00', "
        "ts TIMESTAMP NOT NULL DEFAULT '0000-00-00 00:00:00')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM wp_defaults",
            .values = wp_show_columns_rows,
            .column_count = show_columns_column_count,
            .row_count = 3U,
            .context = "WP-style NOT NULL zero temporal default metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE wp_defaults",
            .values = wp_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "WP-style NOT NULL zero temporal SHOW CREATE",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_defaults () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT d, dt, ts FROM wp_defaults",
            .values = wp_default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "WP-style NOT NULL zero temporal inserted defaults",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_dt_empty (dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00')"
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_dt_empty () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT dt FROM wp_dt_empty",
            .values = wp_datetime_default_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "empty sql_mode WP DATETIME zero default",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_dt_strict (dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00')"
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_dt_strict () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT dt FROM wp_dt_strict",
            .values = wp_datetime_default_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "strict sql_mode WP DATETIME zero default",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures += expect_statement_result(
        database,
        "CREATE TABLE wp_dt_no_zero_date "
        "(dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00')",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_dt_no_zero_date () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT dt FROM wp_dt_no_zero_date",
            .values = wp_datetime_default_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "NO_ZERO_DATE WP DATETIME zero default",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_IN_DATE'");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_dt_no_zero_in_date "
        "(dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00')"
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_dt_no_zero_in_date () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT dt FROM wp_dt_no_zero_in_date",
            .values = wp_datetime_default_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "NO_ZERO_IN_DATE WP DATETIME zero default",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO'");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_dt_no_auto_value_on_zero "
        "(dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00')"
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_dt_no_auto_value_on_zero () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT dt FROM wp_dt_no_auto_value_on_zero",
            .values = wp_datetime_default_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "NO_AUTO_VALUE_ON_ZERO WP DATETIME zero default",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE wp_dt_no_backslash_escapes "
        "(dt DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00')"
    );
    failures += expect_dml_ok(database, "INSERT INTO wp_dt_no_backslash_escapes () VALUES ()", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT dt FROM wp_dt_no_backslash_escapes",
            .values = wp_datetime_default_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "NO_BACKSLASH_ESCAPES WP DATETIME zero default",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_IN_DATE'");
    failures += expect_statement_result(
        database,
        "CREATE TABLE partial_defaults ("
        "id INT, "
        "d DATE DEFAULT '2024-00-01', "
        "dt DATETIME DEFAULT '2024-01-00 00:00:00')",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_dml_ok(database, "INSERT INTO partial_defaults (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt FROM partial_defaults",
            .values = partial_default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "partial temporal defaults adjusted",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE'");
    failures += execute_error(
        database,
        "CREATE TABLE bad_zero_default (d DATE DEFAULT '0000-00-00')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'd'",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE predicate_modes (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO predicate_modes VALUES "
        "(1, '2024-00-01', '2024-01-00 00:00:00', '0000-00-00 00:00:00')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM predicate_modes WHERE d = '2024-00-01'",
            .values = one_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "partial DATE predicate admitted",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM predicate_modes "
                   "WHERE dt = '2024-01-00 00:00:00'",
            .values = one_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "partial DATETIME predicate admitted",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM predicate_modes "
                   "WHERE ts = '0000-00-00 00:00:00'",
            .values = one_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "zero TIMESTAMP predicate admitted",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_IN_DATE'");
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM predicate_modes WHERE d = '2024-00-01'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_timestamp_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect DATE value: '2024-00-01'",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures += execute_error(
        database,
        "SELECT COUNT(*) FROM predicate_modes WHERE ts = '0000-00-00 00:00:00'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_timestamp_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect TIMESTAMP value: '0000-00-00 00:00:00'",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        MYLITE_FILE_PREAMBLE_SIZE,
        "zero temporal file preamble"
    );

    mylite_close(database);
    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen defaults file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM zero_defaults",
            .values = zero_default_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "reopened zero temporal defaults",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_zero_temporal_alter_defaults(void) {
    static const char *const full_zero_show_rows[] = {
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "d",
        "date",
        "YES",
        "",
        "0000-00-00",
        "",
        "dt",
        "datetime",
        "YES",
        "",
        "0000-00-00 00:00:00",
        "",
        "ts",
        "timestamp",
        "YES",
        "",
        "0000-00-00 00:00:00",
        "",
    };
    static const char *const full_zero_insert_rows[] = {
        "1",
        "0000-00-00",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    static const char *const no_zero_in_date_show_rows[] = {
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "d",
        "date",
        "YES",
        "",
        "0000-00-00",
        "",
        "dt",
        "datetime",
        "YES",
        "",
        "0000-00-00 00:00:00",
        "",
        "t",
        "time",
        "YES",
        "",
        "00:00:00",
        "",
        "y",
        "year",
        "YES",
        "",
        "0000",
        "",
    };
    static const char *const combined_zero_modes_show_rows[] = {
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "d",
        "date",
        "YES",
        "",
        "0000-00-00",
        "",
        "dt",
        "datetime",
        "YES",
        "",
        "0000-00-00 00:00:00",
        "",
    };
    static const char *const modify_change_rows[] = {
        "id",
        "int",
        "YES",
        "",
        NULL,
        "",
        "dt2",
        "datetime",
        "YES",
        "",
        "2024-01-00 00:00:00",
        "",
        "ts3",
        "timestamp",
        "YES",
        "",
        "0000-00-00 00:00:00",
        "",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "alter_defaults") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open alter defaults file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(database, "CREATE TABLE add_empty (id INT)");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE add_empty ADD COLUMN d DATE DEFAULT '0000-00-00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE add_empty ADD COLUMN dt DATETIME DEFAULT '0000-00-00 00:00:00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE add_empty ADD COLUMN ts TIMESTAMP NULL DEFAULT '0000-00-00 00:00:00'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM add_empty",
            .values = full_zero_show_rows,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = "empty sql_mode ALTER ADD zero defaults metadata",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO add_empty (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM add_empty",
            .values = full_zero_insert_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "empty sql_mode ALTER ADD zero defaults materialized",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures += expect_statement_ok(database, "CREATE TABLE add_no_zero_date (id INT)");
    failures += expect_statement_result(
        database,
        "ALTER TABLE add_no_zero_date ADD COLUMN d DATE DEFAULT '0000-00-00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE add_no_zero_date ADD COLUMN dt DATETIME DEFAULT '0000-00-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE add_no_zero_date ADD COLUMN ts TIMESTAMP NULL DEFAULT "
        "'0000-00-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM add_no_zero_date",
            .values = full_zero_show_rows,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = "NO_ZERO_DATE ALTER ADD zero defaults metadata",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_IN_DATE'");
    failures += expect_statement_ok(database, "CREATE TABLE add_no_zero_in_date (id INT)");
    failures += expect_statement_result(
        database,
        "ALTER TABLE add_no_zero_in_date ADD COLUMN d DATE DEFAULT '2024-00-01'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE add_no_zero_in_date ADD COLUMN dt DATETIME DEFAULT "
        "'2024-01-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE add_no_zero_in_date ADD COLUMN t TIME DEFAULT '00:00:00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE add_no_zero_in_date ADD COLUMN y YEAR DEFAULT '0000'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM add_no_zero_in_date",
            .values = no_zero_in_date_show_rows,
            .column_count = show_columns_column_count,
            .row_count = zero_in_date_show_columns_row_count,
            .context = "NO_ZERO_IN_DATE ALTER ADD adjusted defaults",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_empty (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE set_empty ALTER COLUMN d SET DEFAULT '0000-00-00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE set_empty ALTER COLUMN dt SET DEFAULT '0000-00-00 00:00:00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE set_empty ALTER COLUMN ts SET DEFAULT '0000-00-00 00:00:00'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM set_empty",
            .values = full_zero_show_rows,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = "empty sql_mode ALTER SET zero defaults metadata",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO set_empty (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM set_empty",
            .values = full_zero_insert_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "empty sql_mode ALTER SET zero defaults materialized",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_no_zero_date (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE set_no_zero_date ALTER COLUMN d SET DEFAULT '0000-00-00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE set_no_zero_date ALTER COLUMN dt SET DEFAULT '0000-00-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE set_no_zero_date ALTER COLUMN ts SET DEFAULT '0000-00-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM set_no_zero_date",
            .values = full_zero_show_rows,
            .column_count = show_columns_column_count,
            .row_count = 4U,
            .context = "NO_ZERO_DATE ALTER SET zero defaults metadata",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_IN_DATE'");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_no_zero_in_date (id INT, d DATE, dt DATETIME, t TIME, y YEAR)"
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE set_no_zero_in_date ALTER COLUMN d SET DEFAULT '2024-00-01'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE set_no_zero_in_date ALTER COLUMN dt SET DEFAULT '2024-01-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE set_no_zero_in_date ALTER COLUMN t SET DEFAULT '00:00:00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE set_no_zero_in_date ALTER COLUMN y SET DEFAULT '0000'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM set_no_zero_in_date",
            .values = no_zero_in_date_show_rows,
            .column_count = show_columns_column_count,
            .row_count = zero_in_date_show_columns_row_count,
            .context = "NO_ZERO_IN_DATE ALTER SET adjusted defaults",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE,NO_ZERO_IN_DATE'");
    failures += expect_statement_ok(database, "CREATE TABLE add_combined (id INT)");
    failures += expect_statement_result(
        database,
        "ALTER TABLE add_combined ADD COLUMN d DATE DEFAULT '2024-00-01'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE add_combined ADD COLUMN dt DATETIME DEFAULT '2024-01-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM add_combined",
            .values = combined_zero_modes_show_rows,
            .column_count = show_columns_column_count,
            .row_count = combined_zero_modes_show_columns_row_count,
            .context = "combined zero modes ALTER ADD adjusted defaults",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE set_combined (id INT, d DATE, dt DATETIME)");
    failures += expect_statement_result(
        database,
        "ALTER TABLE set_combined ALTER COLUMN d SET DEFAULT '2024-00-01'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE set_combined ALTER COLUMN dt SET DEFAULT '2024-01-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM set_combined",
            .values = combined_zero_modes_show_rows,
            .column_count = show_columns_column_count,
            .row_count = combined_zero_modes_show_columns_row_count,
            .context = "combined zero modes ALTER SET adjusted defaults",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE modify_change (id INT, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE modify_change MODIFY COLUMN dt DATETIME DEFAULT '2024-01-00 00:00:00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE modify_change MODIFY COLUMN ts TIMESTAMP NULL DEFAULT "
        "'0000-00-00 00:00:00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE modify_change CHANGE COLUMN dt dt2 DATETIME DEFAULT "
        "'2024-01-00 00:00:00'"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE modify_change CHANGE COLUMN ts ts2 TIMESTAMP NULL DEFAULT "
        "'0000-00-00 00:00:00'"
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures += expect_statement_result(
        database,
        "ALTER TABLE modify_change MODIFY COLUMN ts2 TIMESTAMP NULL DEFAULT "
        "'0000-00-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "ALTER TABLE modify_change CHANGE COLUMN ts2 ts3 TIMESTAMP NULL DEFAULT "
        "'0000-00-00 00:00:00'",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM modify_change",
            .values = modify_change_rows,
            .column_count = show_columns_column_count,
            .row_count = 3U,
            .context = "MODIFY CHANGE zero temporal defaults",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE'");
    failures += expect_statement_ok(database, "CREATE TABLE bad_add_date (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE bad_add_date ADD COLUMN d DATE DEFAULT '0000-00-00'",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'd'",
        }
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE bad_set_ts (id INT, ts TIMESTAMP NULL)");
    failures += execute_error(
        database,
        "ALTER TABLE bad_set_ts ALTER COLUMN ts SET DEFAULT '0000-00-00 00:00:00'",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'ts'",
        }
    );

    failures +=
        execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE'");
    failures += expect_statement_ok(database, "CREATE TABLE bad_add_dt (id INT)");
    failures += execute_error(
        database,
        "ALTER TABLE bad_add_dt ADD COLUMN dt DATETIME DEFAULT '2024-01-00 00:00:00'",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'dt'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE bad_change_dt (id INT, dt DATETIME)");
    failures += execute_error(
        database,
        "ALTER TABLE bad_change_dt CHANGE COLUMN dt dt2 DATETIME DEFAULT '2024-01-00 00:00:00'",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'dt2'",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        MYLITE_FILE_PREAMBLE_SIZE,
        "alter defaults file preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen alter defaults file"
    );
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM set_empty",
            .values = full_zero_insert_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "reopened ALTER SET zero defaults materialized",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_zero_temporal_update_replace_copy(void) {
    static const char *const updated_rows[] = {
        "1",
        "2024-00-01",
        "2024-01-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    static const char *const copied_rows[] = {
        "1",
        "2024-00-01",
        "2024-01-00 00:00:00",
        "0000-00-00 00:00:00",
        "2",
        "0000-00-00",
        "0000-00-00 00:00:00",
        "0000-00-00 00:00:00",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "update_replace_copy") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open update file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE dml (id INT, d DATE, dt DATETIME, ts TIMESTAMP NULL)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO dml VALUES (1, '2024-02-29', '2024-02-29 00:00:00', "
        "'2024-02-29 00:00:00')",
        1
    );
    failures += expect_dml_ok(database, "UPDATE dml SET d = '2024-00-01' WHERE id = 1", 1);
    failures +=
        expect_dml_ok(database, "UPDATE dml SET dt = '2024-01-00 00:00:00' WHERE id = 1", 1);
    failures +=
        expect_dml_ok(database, "UPDATE dml SET ts = '0000-00-00 00:00:00' WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM dml WHERE id = 1",
            .values = updated_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "mode-aware UPDATE values",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures +=
        expect_dml_ok(database, "UPDATE dml SET ts = '1970-01-01 00:00:01' WHERE id = 1", 1);
    failures += expect_dml_result(
        database,
        "UPDATE dml SET ts = '0000-00-00 00:00:00' WHERE id = 1",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_dml_result(
        database,
        "REPLACE INTO dml VALUES "
        "(2, '2024-02-31', '2024-02-31 00:00:00', '2024-02-31 00:00:00')",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 3U}
    );
    failures += expect_statement_ok(database, "CREATE TABLE copied LIKE dml");
    failures += expect_dml_ok(
        database,
        "INSERT INTO copied (id, d, dt, ts) SELECT id, d, dt, ts FROM dml",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, dt, ts FROM copied ORDER BY id",
            .values = copied_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "mode-aware INSERT SELECT copy",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_zero_temporal_independent_handles(void) {
    static const char *const zero_rows[] = {"0000-00-00"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first_handle") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second_handle") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE dates (d DATE)");
    failures += expect_statement_ok(second, "CREATE TABLE dates (d DATE)");

    failures += execute_statement_ok(first, "SET sql_mode = ''");
    failures += expect_dml_ok(first, "INSERT INTO dates VALUES ('0000-00-00')", 1);
    failures += execute_error(
        second,
        "INSERT INTO dates VALUES ('0000-00-00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '0000-00-00' for column 'd' at row 1",
        }
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT d FROM dates",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle zero row",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM dates",
            .values = (const char *const[]){"0"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle strict row count",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected MYLITE_OK, got %d (%d %s %s)\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);

    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "statement warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = affected_rows,
            .warning_count = 0U,
        }
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        expected.affected_rows,
        "DML affected"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "DML warnings"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return mylite_test_expect_true(actual == NULL, context);
    }

    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
