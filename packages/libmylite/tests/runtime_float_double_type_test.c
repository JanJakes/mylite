#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <locale.h>
#include <stdbool.h>
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
    locale_name_capacity = 128,
    show_columns_field_count = 6,
    approx_type_column_count = 17,
    approx_values_column_count = 7,
    approx_values_after_underflow_row_count = 5,
    approx_information_schema_column_count = 10,
    approx_information_schema_row_count = 16,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_bad_null = 1048,
    mysql_error_data_out_of_range = 1264,
    mysql_error_incorrect_column_specifier = 1063,
    mysql_error_decimal_scale_too_big = 1425,
    mysql_error_decimal_must_be_greater_or_equal_to_d = 1427,
    mysql_error_display_width_out_of_range = 1439,
};

static const int64_t affected_rows_not_checked = INT64_MIN;

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

struct expected_statement_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_float_double_success_persistence_and_introspection(void);
static int test_float_double_diagnostics(void);
static int test_float_double_locale_independent_numeric_io(void);
static int test_float_double_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement_result expected
);
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
static int expect_true(int condition, const char *context);
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

    failures += test_float_double_success_persistence_and_introspection();
    failures += test_float_double_diagnostics();
    failures += test_float_double_locale_independent_numeric_io();
    failures += test_float_double_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_float_double_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id",    "int",
        "NO",    "",
        NULL,    "",
        "f",     "float",
        "YES",   "",
        NULL,    "",
        "f0",    "float",
        "YES",   "",
        NULL,    "",
        "f24",   "float",
        "YES",   "",
        NULL,    "",
        "f25",   "double",
        "YES",   "",
        NULL,    "",
        "f53",   "double",
        "YES",   "",
        NULL,    "",
        "d",     "double",
        "YES",   "",
        NULL,    "",
        "dp",    "double",
        "YES",   "",
        NULL,    "",
        "r",     "double",
        "YES",   "",
        NULL,    "",
        "f4",    "float",
        "YES",   "",
        NULL,    "",
        "f8",    "double",
        "YES",   "",
        NULL,    "",
        "fmd",   "float(10,2)",
        "YES",   "",
        NULL,    "",
        "dmd",   "double(10,2)",
        "YES",   "",
        NULL,    "",
        "fu",    "float unsigned",
        "YES",   "",
        NULL,    "",
        "du",    "double unsigned",
        "YES",   "",
        NULL,    "",
        "nn",    "float",
        "NO",    "",
        "1.25",  "",
        "dn",    "double",
        "NO",    "",
        "-2.25", "",
    };
    static const char *const show_create_rows[] = {
        "approx_types",
        "CREATE TABLE `approx_types` (\n"
        "  `id` int NOT NULL,\n"
        "  `f` float DEFAULT NULL,\n"
        "  `f0` float DEFAULT NULL,\n"
        "  `f24` float DEFAULT NULL,\n"
        "  `f25` double DEFAULT NULL,\n"
        "  `f53` double DEFAULT NULL,\n"
        "  `d` double DEFAULT NULL,\n"
        "  `dp` double DEFAULT NULL,\n"
        "  `r` double DEFAULT NULL,\n"
        "  `f4` float DEFAULT NULL,\n"
        "  `f8` double DEFAULT NULL,\n"
        "  `fmd` float(10,2) DEFAULT NULL,\n"
        "  `dmd` double(10,2) DEFAULT NULL,\n"
        "  `fu` float unsigned DEFAULT NULL,\n"
        "  `du` double unsigned DEFAULT NULL,\n"
        "  `nn` float NOT NULL DEFAULT '1.25',\n"
        "  `dn` double NOT NULL DEFAULT '-2.25'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "f",   "float",  "float",           "12", NULL, "YES", NULL,    NULL, NULL, NULL,
        "f0",  "float",  "float",           "12", NULL, "YES", NULL,    NULL, NULL, NULL,
        "f24", "float",  "float",           "12", NULL, "YES", NULL,    NULL, NULL, NULL,
        "f25", "double", "double",          "22", NULL, "YES", NULL,    NULL, NULL, NULL,
        "f53", "double", "double",          "22", NULL, "YES", NULL,    NULL, NULL, NULL,
        "d",   "double", "double",          "22", NULL, "YES", NULL,    NULL, NULL, NULL,
        "dp",  "double", "double",          "22", NULL, "YES", NULL,    NULL, NULL, NULL,
        "r",   "double", "double",          "22", NULL, "YES", NULL,    NULL, NULL, NULL,
        "f4",  "float",  "float",           "12", NULL, "YES", NULL,    NULL, NULL, NULL,
        "f8",  "double", "double",          "22", NULL, "YES", NULL,    NULL, NULL, NULL,
        "fmd", "float",  "float(10,2)",     "10", "2",  "YES", NULL,    NULL, NULL, NULL,
        "dmd", "double", "double(10,2)",    "10", "2",  "YES", NULL,    NULL, NULL, NULL,
        "fu",  "float",  "float unsigned",  "12", NULL, "YES", NULL,    NULL, NULL, NULL,
        "du",  "double", "double unsigned", "22", NULL, "YES", NULL,    NULL, NULL, NULL,
        "nn",  "float",  "float",           "12", NULL, "NO",  "1.25",  NULL, NULL, NULL,
        "dn",  "double", "double",          "22", NULL, "NO",  "-2.25", NULL, NULL, NULL,
    };
    static const char *const scaled_default_show_columns_rows[] = {
        "id",   "int",
        "YES",  "",
        NULL,   "",
        "f",    "float(10,2)",
        "NO",   "",
        "0.00", "",
        "d",    "double(10,2)",
        "NO",   "",
        "-2.50", "",
    };
    static const char *const scaled_default_row[] = {"1", "0.00", "-2.50"};
    static const char *const initial_rows[] = {
        "1",          "42",
        "42",         "42",
        "42",         "1.5",
        "-2.5",       "2",
        "3.14159",    "3.1415926535",
        "1.25",       "2.5",
        "3.40282e38", "1.7976931348623157e308",
        "3",          "0",
        "0",          "0",
        "0",          "1.5",
        "-2.5",       "4",
        NULL,         NULL,
        NULL,         NULL,
        "1.5",        "-2.5",
    };
    static const char *const updated_row[] = {"1", "9.75", "4.25"};
    static const char *const underflow_row[] = {"5", "0", "0"};
    static const char *const null_predicate_row[] = {"4"};
    static const char *const altered_rows[] = {
        "1",
        "0",
        "2.5",
        "2",
        "0",
        "2.5",
        "3",
        "0",
        "2.5",
        "4",
        "0",
        "2.5",
        "5",
        "0",
        "2.5",
    };
    static const char *const set_rows[] = {"2", "2.5", "3.75", "4.5"};
    static const char *const insert_ignore_rows[] = {
        "3",
        "6.5",
        "1.5",
        NULL,
        "4",
        "0",
        "4.25",
        NULL,
        "5",
        "0",
        "5.5",
        NULL,
    };
    static const char *const like_rows[] = {"10", "10.25", "1.5"};
    static const char *const copied_rows[] = {
        "1",
        "1.25",
        "1.5",
        "2",
        "2.5",
        "3.75",
    };
    static const char *const renamed_rows[] = {"2.5"};
    static const char *const reopened_rows[] = {"1", "9.75", "4.25", "2.5"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open approximate file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_result(
        database,
        "CREATE TABLE approx_types (id INT NOT NULL, f FLOAT, f0 FLOAT(0), f24 FLOAT(24), "
        "f25 FLOAT(25), f53 FLOAT(53), d DOUBLE, dp DOUBLE PRECISION, r REAL, f4 FLOAT4, "
        "f8 FLOAT8, fmd FLOAT(10,2), dmd DOUBLE(10,2), fu FLOAT UNSIGNED, "
        "du DOUBLE UNSIGNED, nn FLOAT NOT NULL DEFAULT 1.25, dn DOUBLE NOT NULL DEFAULT -2.25)",
        (struct expected_statement_result){
            .affected_rows = affected_rows_not_checked,
            .warning_count = 4U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM approx_types",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = approx_type_column_count,
            .context = "approximate SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE approx_types",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "approximate SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, NUMERIC_PRECISION, "
                   "NUMERIC_SCALE, IS_NULLABLE, COLUMN_DEFAULT, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_SET_NAME, COLLATION_NAME FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'approx_types' "
                   "AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = approx_information_schema_column_count,
            .row_count = approx_information_schema_row_count,
            .context = "approximate INFORMATION_SCHEMA.COLUMNS",
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE approx_scaled_defaults (id INT, f FLOAT(10,2) NOT NULL DEFAULT 0, "
        "d DOUBLE(10,2) NOT NULL DEFAULT -2.5)",
        (struct expected_statement_result){
            .affected_rows = affected_rows_not_checked,
            .warning_count = 2U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM approx_scaled_defaults",
            .values = scaled_default_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "scaled approximate default SHOW COLUMNS",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO approx_scaled_defaults (id) VALUES (1)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d FROM approx_scaled_defaults",
            .values = scaled_default_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "scaled approximate defaults",
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE approx_values (id INT, f FLOAT, d DOUBLE, fu FLOAT UNSIGNED, "
        "du DOUBLE UNSIGNED, nn FLOAT NOT NULL DEFAULT 1.5, dn DOUBLE NOT NULL DEFAULT -2.5)",
        (struct expected_statement_result){
            .affected_rows = affected_rows_not_checked,
            .warning_count = 2U,
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO approx_values VALUES "
        "(1, 42, 42, 42, 42, DEFAULT, DEFAULT), "
        "(2, 3.1415926535, 3.1415926535, 1.25, 2.5, 3.402823466E+38, "
        "1.7976931348623157E+308), "
        "(3, +0, -0, FALSE, FALSE, DEFAULT, DEFAULT), "
        "(4, NULL, NULL, NULL, NULL, DEFAULT, DEFAULT)",
        (struct expected_statement_result){.affected_rows = 4, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d, fu, du, nn, dn FROM approx_values ORDER BY id",
            .values = initial_rows,
            .column_count = approx_values_column_count,
            .row_count = 4U,
            .context = "approximate readback",
        }
    );
    failures += expect_statement_result(
        database,
        "UPDATE approx_values SET f = 9.75 WHERE id = 1",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE approx_values SET d = 4.25 WHERE id = 1",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d FROM approx_values WHERE id = 1",
            .values = updated_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "approximate update readback",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO approx_values (id, f, d) VALUES (5, 1e-46, 1e-325)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d FROM approx_values WHERE id = 5",
            .values = underflow_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "approximate underflow readback",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM approx_values WHERE f IS NULL ORDER BY id",
            .values = null_predicate_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "approximate IS NULL predicate",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE approx_values ADD COLUMN added FLOAT NOT NULL");
    failures += expect_statement_ok(
        database,
        "ALTER TABLE approx_values ADD COLUMN with_default DOUBLE NOT NULL DEFAULT 2.5"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added, with_default FROM approx_values ORDER BY id",
            .values = altered_rows,
            .column_count = 3U,
            .row_count = approx_values_after_underflow_row_count,
            .context = "approximate alter add defaults",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE approx_paths (id INT, f FLOAT NOT NULL, d DOUBLE DEFAULT 1.5, "
        "nullable DOUBLE)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO approx_paths SET id = 1, f = 1.25, d = DEFAULT, nullable = NULL",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "REPLACE INTO approx_paths SET id = 2, f = 2.5, d = 3.75, nullable = 4.5",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d, nullable FROM approx_paths WHERE nullable IS NOT NULL "
                   "ORDER BY id",
            .values = set_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "approximate SET form and IS NOT NULL predicate",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE approx_paths ALTER COLUMN f SET DEFAULT 6.5");
    failures += expect_statement_result(
        database,
        "INSERT INTO approx_paths (id, d) VALUES (3, DEFAULT)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE approx_paths ALTER COLUMN f DROP DEFAULT");
    failures += expect_statement_result(
        database,
        "INSERT IGNORE INTO approx_paths (id, d) VALUES (4, 4.25)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_statement_result(
        database,
        "INSERT IGNORE INTO approx_paths (id, f, d) VALUES (5, NULL, 5.5)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d, nullable FROM approx_paths WHERE id >= 3 ORDER BY id",
            .values = insert_ignore_rows,
            .column_count = 4U,
            .row_count = 3U,
            .context = "approximate altered defaults and INSERT IGNORE adjustments",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE approx_like LIKE approx_paths");
    failures += expect_statement_result(
        database,
        "INSERT INTO approx_like (id, f) VALUES (10, 10.25)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d FROM approx_like ORDER BY id",
            .values = like_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "approximate CREATE TABLE LIKE defaults",
        }
    );
    failures += expect_statement_result(
        database,
        "CREATE TABLE approx_ctas AS SELECT id, f, d FROM approx_paths WHERE id IN (1, 2)",
        (struct expected_statement_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d FROM approx_ctas ORDER BY id",
            .values = copied_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "approximate CREATE TABLE SELECT copy",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE approx_insert_select (id INT, f FLOAT NOT NULL, d DOUBLE)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO approx_insert_select SELECT id, f, d FROM approx_ctas",
        (struct expected_statement_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE approx_replace_select LIKE approx_insert_select"
    );
    failures += expect_statement_result(
        database,
        "REPLACE INTO approx_replace_select SELECT id, f, d FROM approx_insert_select",
        (struct expected_statement_result){.affected_rows = 2, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d FROM approx_replace_select ORDER BY id",
            .values = copied_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "approximate INSERT SELECT and REPLACE SELECT copy",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE approx_paths TO approx_paths_renamed");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT f FROM approx_paths_renamed WHERE id = 2",
            .values = renamed_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "approximate rename persistence",
        }
    );
    failures += expect_statement_ok(database, "DROP TABLE approx_paths_renamed");
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures +=
        expect_bytes(actual_preamble, expected_preamble, sizeof(actual_preamble), "preamble");

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen approximate file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f, d, with_default FROM approx_values WHERE id = 1",
            .values = reopened_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "approximate reopened rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_float_double_diagnostics(void) {
    static const char *const whitespace_string_row[] = {"4", "1.5"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_result(
        database,
        "CREATE TABLE values_t (id INT, f FLOAT, d DOUBLE, fu FLOAT UNSIGNED, "
        "nn FLOAT NOT NULL DEFAULT 1.5)",
        (struct expected_statement_result){
            .affected_rows = affected_rows_not_checked,
            .warning_count = 1U,
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO values_t (id, f) VALUES (1, 3.5e38)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'f' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO values_t (id, fu) VALUES (2, -1.5)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'fu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO values_t (id, nn) VALUES (3, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO values_t (id, f) VALUES (4, ' 1.5')",
        (struct expected_statement_result){
            .affected_rows = 1,
            .warning_count = 0U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, f FROM values_t WHERE id = 4",
            .values = whitespace_string_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "strict approximate whitespace string",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE quoted_float_default (f FLOAT DEFAULT '1.5')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'f'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE quoted_double_default (d DOUBLE DEFAULT '2.5')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'd'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM values_t WHERE f = 1.0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM values_t ORDER BY f",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "ORDER BY supports only integer, BIT, YEAR, DATE, TIME, DATETIME, TIMESTAMP, "
                "or nonbinary string descriptor columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_precision (x FLOAT(54))",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_specifier,
            .sqlstate = "42000",
            .message_part = "Incorrect column specifier for column 'x'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_display_width (x FLOAT(256,1))",
        (struct expected_sql_error){
            .code = mysql_error_display_width_out_of_range,
            .sqlstate = "42000",
            .message_part = "Display width out of range for column 'x' (max = 255)",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_float_scale (x FLOAT(10,31))",
        (struct expected_sql_error){
            .code = mysql_error_decimal_scale_too_big,
            .sqlstate = "42000",
            .message_part = "Too big scale 31 specified for column 'x'. Maximum is 30.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_float_m_d (x FLOAT(1,2))",
        (struct expected_sql_error){
            .code = mysql_error_decimal_must_be_greater_or_equal_to_d,
            .sqlstate = "42000",
            .message_part =
                "For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column 'x').",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_primary (f FLOAT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "PRIMARY KEY supports only integer columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_inline_index (f FLOAT, KEY k_f (f))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Secondary indexes do not yet support this column type",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_inline_unique (f DOUBLE, UNIQUE KEY u_f (f))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Secondary indexes do not yet support this column type",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX k_f ON values_t (f)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Secondary indexes do not yet support this column type",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_auto_increment (f FLOAT AUTO_INCREMENT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_specifier,
            .sqlstate = "42000",
            .message_part = "Incorrect column specifier for column 'f'",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_float_double_locale_independent_numeric_io(void) {
    static const char *const locale_names[] = {
        "de_DE.UTF-8",
        "fr_FR.UTF-8",
        "cs_CZ.UTF-8",
        "de_DE.utf8",
        "fr_FR.utf8",
        "cs_CZ.utf8",
    };
    static const char *const locale_rows[] = {"1.25", "3.1415926535", "2.5"};
    char original_locale[locale_name_capacity];
    char path[test_path_capacity] = {0};
    const char *current_locale = setlocale(LC_NUMERIC, NULL);
    bool changed_locale = false;
    mylite_db *database = NULL;
    int written = 0;
    int failures = 0;

    if (current_locale == NULL) {
        current_locale = "C";
    }
    written = snprintf(original_locale, sizeof(original_locale), "%s", current_locale);
    if (written < 0 || (size_t)written >= sizeof(original_locale)) {
        fprintf(stderr, "failed to copy LC_NUMERIC locale\n");
        return 1;
    }
    for (size_t index = 0U; index < sizeof(locale_names) / sizeof(locale_names[0]); ++index) {
        if (setlocale(LC_NUMERIC, locale_names[index]) != NULL) {
            changed_locale = true;
            break;
        }
    }
    if (!changed_locale) {
        return 0;
    }

    if (make_test_path(path, sizeof(path), "locale") != 0) {
        failures += 1;
        goto cleanup;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open locale file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE locale_values (id INT, f FLOAT, d DOUBLE, df DOUBLE DEFAULT 2.5)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO locale_values VALUES (1, 1.25, 3.1415926535, DEFAULT)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT f, d, df FROM locale_values WHERE id = 1",
            .values = locale_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "approximate C-locale numeric IO",
        }
    );

cleanup:
    if (database != NULL) {
        mylite_close(database);
    }
    if (setlocale(LC_NUMERIC, original_locale) == NULL) {
        fprintf(stderr, "failed to restore LC_NUMERIC locale\n");
        failures += 1;
    }
    if (path[0] != '\0') {
        remove_related_files(path);
    }

    return failures;
}

static int test_float_double_independent_handles(void) {
    static const char *const first_expected[] = {"1.25"};
    static const char *const second_expected[] = {"2.5"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, f FLOAT)");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, f FLOAT)");
    failures += expect_statement_result(
        first,
        "INSERT INTO t VALUES (1, 1.25)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t VALUES (1, 2.5)",
        (struct expected_statement_result){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT f FROM t WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent approximate state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT f FROM t WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent approximate state",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_statement_result){
            .affected_rows = affected_rows_not_checked,
            .warning_count = 0U,
        }
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    if (expected.affected_rows != affected_rows_not_checked) {
        failures += expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            "statement affected rows"
        );
    }
    if (mylite_result_warning_count(result) != expected.warning_count) {
        fprintf(stderr, "warning-count SQL: %s\n", sql);
    }
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "statement warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
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
        "%s/mylite_float_double_type_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
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

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
