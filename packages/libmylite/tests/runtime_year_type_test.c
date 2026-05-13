#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_diagnostics.h"
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
    show_columns_field_count = 6,
    information_schema_column_count = 9,
    year_show_columns_row_count = 10,
    year_information_schema_row_count = 9,
    year_dml_row_count = 11,
    year_order_row_count = 6,
    year_replace_row_count = 4,
    year_index_row_count = 5,
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_parse = 1064,
    mysql_error_no_default = 1364,
    mysql_error_data_out_of_range = 1264,
    mysql_error_incorrect_integer_value = 1366,
    mysql_error_invalid_year_display_width = 1818,
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

static int test_year_success_metadata_dml_and_persistence(void);
static int test_year_diagnostics(void);
static int test_year_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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

    failures += test_year_success_metadata_dml_and_persistence();
    failures += test_year_diagnostics();
    failures += test_year_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_year_success_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",  "NO",  "", NULL,   "", "y",   "year", "YES", "", NULL,   "",
        "y4", "year", "YES", "", NULL,   "", "ynn", "year", "NO",  "", NULL,   "",
        "yd", "year", "YES", "", "1970", "", "ys",  "year", "YES", "", "1970", "",
        "yz", "year", "YES", "", "0000", "", "yzs", "year", "YES", "", "2000", "",
        "yt", "year", "YES", "", "2001", "", "yf",  "year", "YES", "", "0000", "",
    };
    static const char *const show_create_rows[] = {
        "years",
        "CREATE TABLE `years` (\n"
        "  `id` int NOT NULL,\n"
        "  `y` year DEFAULT NULL,\n"
        "  `y4` year DEFAULT NULL,\n"
        "  `ynn` year NOT NULL,\n"
        "  `yd` year DEFAULT '1970',\n"
        "  `ys` year DEFAULT '1970',\n"
        "  `yz` year DEFAULT '0000',\n"
        "  `yzs` year DEFAULT '2000',\n"
        "  `yt` year DEFAULT '2001',\n"
        "  `yf` year DEFAULT '0000'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "y",   "year", "year", "YES", NULL,   NULL,   NULL,  NULL,   NULL,   "y4",  "year", "year",
        "YES", NULL,   NULL,   NULL,  NULL,   NULL,   "ynn", "year", "year", "NO",  NULL,   NULL,
        NULL,  NULL,   NULL,   "yd",  "year", "year", "YES", "1970", NULL,   NULL,  NULL,   NULL,
        "ys",  "year", "year", "YES", "1970", NULL,   NULL,  NULL,   NULL,   "yz",  "year", "year",
        "YES", "0000", NULL,   NULL,  NULL,   NULL,   "yzs", "year", "year", "YES", "2000", NULL,
        NULL,  NULL,   NULL,   "yt",  "year", "year", "YES", "2001", NULL,   NULL,  NULL,   NULL,
        "yf",  "year", "year", "YES", "0000", NULL,   NULL,  NULL,   NULL,
    };
    static const char *const dml_rows[] = {
        "1",    "0000", "0000", "2",    "2000", "2000", "3",    "2001", "2001", "4",    "2069",
        "2069", "5",    "1970", "1970", "6",    "1999", "1999", "7",    "1901", "1901", "8",
        "2155", "2155", "9",    NULL,   "1970", "10",   "2001", "0000", "11",   "0000", "2155",
    };
    static const char *const after_updates[] = {
        "1",    "0000", "0000", "2",    "2000", "2000", "3",    "2001", "2001", "4",    NULL,
        "2069", "5",    "1970", "1970", "6",    "1999", "1999", "7",    "1901", "1901", "8",
        "2155", "2155", "9",    NULL,   "1970", "10",   "2001", "0000", "11",   "0000", "2155",
    };
    static const char *const in_numeric_rows[] = {"2"};
    static const char *const in_string_rows[] = {"2", "5", "9"};
    static const char *const in_mixed_rows[] = {"2"};
    static const char *const ordered_rows[] = {
        "1",
        NULL,
        "2",
        "0000",
        "3",
        "1901",
        "4",
        "1970",
        "5",
        "2000",
        "6",
        "2155",
    };
    static const char *const ordered_desc_rows[] = {
        "6",
        "2155",
        "5",
        "2000",
        "4",
        "1970",
        "3",
        "1901",
        "2",
        "0000",
        "1",
        NULL,
    };
    static const char *const ignored_rows[] = {
        "1",
        "0000",
        "2",
        "0000",
        "3",
        "0000",
        "4",
        "0000",
    };
    static const char *const ordered_limit_rows[] = {"1", "1", "2", "0", "3", "0", "4", "1"};
    static const char *const alter_rows[] = {"1", "0000", "2", "0000"};
    static const char *const alter_default_rows[] = {"3", "1970"};
    static const char *const replace_rows[] = {
        "1",
        "1970",
        "1970",
        "2",
        NULL,
        "0000",
        "3",
        "2069",
        "2001",
        "4",
        "2001",
        "2001",
    };
    static const char *const copied_rows[] = {"3", "2001", "2001"};
    static const char *const selected_rows[] = {"3", "2001"};
    static const char *const index_rows[] = {
        "3",
        NULL,
        "4",
        "0000",
        "5",
        "1970",
        "1",
        "2001",
        "2",
        "2002",
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open year success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_result(
        database,
        "CREATE TABLE years (id INT NOT NULL, y YEAR, y4 YEAR(4), ynn YEAR NOT NULL, "
        "yd YEAR DEFAULT 70, ys YEAR DEFAULT '70', yz YEAR DEFAULT 0, "
        "yzs YEAR DEFAULT '0', yt YEAR DEFAULT TRUE, yf YEAR DEFAULT FALSE)",
        (struct expected_dml_result){.affected_rows = 0, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM years",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = year_show_columns_row_count,
            .context = "year SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE years",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "year SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
                   "DATETIME_PRECISION, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, "
                   "NUMERIC_SCALE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'years' "
                   "AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_column_count,
            .row_count = year_information_schema_row_count,
            .context = "year information schema",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE dml_t (id INT NOT NULL, y YEAR, ynn YEAR NOT NULL DEFAULT 1970)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO dml_t VALUES "
        "(1, 0, 0), (2, '0', '0'), (3, 1, 1), (4, '69', '69'), "
        "(5, 70, 70), (6, '99', '99'), (7, 1901, 1901), (8, 2155, 2155), "
        "(9, NULL, DEFAULT), (10, TRUE, FALSE), (11, '0000', '2155')",
        year_dml_row_count
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y, ynn FROM dml_t ORDER BY id",
            .values = dml_rows,
            .column_count = 3U,
            .row_count = year_dml_row_count,
            .context = "year inserted rows",
        }
    );
    failures += expect_dml_ok(database, "UPDATE dml_t SET y = '69' WHERE id = 4", 0);
    failures += expect_dml_ok(database, "UPDATE dml_t SET y = 70 WHERE id = 4", 1);
    failures += expect_dml_ok(database, "UPDATE dml_t SET y = NULL WHERE id = 4", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y, ynn FROM dml_t ORDER BY id",
            .values = after_updates,
            .column_count = 3U,
            .row_count = year_dml_row_count,
            .context = "year updated rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dml_t WHERE ynn IN (70, 2000) ORDER BY id",
            .values = in_numeric_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "year numeric IN list",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dml_t WHERE ynn IN ('70', '2000') ORDER BY id",
            .values = in_string_rows,
            .column_count = 1U,
            .row_count = 3U,
            .context = "year string IN list",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dml_t WHERE ynn IN (70, '2000') ORDER BY id",
            .values = in_mixed_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "year mixed IN list",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE order_t (id INT NOT NULL, y YEAR)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO order_t VALUES "
        "(1, NULL), (2, 0), (3, 1901), (4, 1970), (5, 2000), (6, 2155)",
        year_order_row_count
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM order_t ORDER BY y ASC",
            .values = ordered_rows,
            .column_count = 2U,
            .row_count = year_order_row_count,
            .context = "year ascending order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM order_t ORDER BY y DESC",
            .values = ordered_desc_rows,
            .column_count = 2U,
            .row_count = year_order_row_count,
            .context = "year descending order",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE ignored (id INT, y YEAR NOT NULL)");
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO ignored VALUES (1, 2156), (2, 'abc'), (3, NULL)",
        (struct expected_dml_result){.affected_rows = 3, .warning_count = 3U}
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO ignored(id) VALUES (4)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM ignored ORDER BY id",
            .values = ignored_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "year INSERT IGNORE rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ordered_update (id INT NOT NULL, y YEAR, flag INT NOT NULL DEFAULT 0)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO ordered_update VALUES "
        "(1, NULL, 0), (2, 2001, 0), (3, 2000, 0), (4, NULL, 0)",
        4
    );
    failures += expect_dml_ok(database, "UPDATE ordered_update SET flag = 1 ORDER BY y LIMIT 2", 2);
    failures += expect_dml_ok(
        database,
        "UPDATE ordered_update SET flag = 2 WHERE flag = 0 ORDER BY y DESC LIMIT 0",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM ordered_update ORDER BY id",
            .values = ordered_limit_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "year ordered limit update",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE alter_t (id INT)");
    failures += expect_dml_ok(database, "INSERT INTO alter_t VALUES (1), (2)", 2);
    failures += expect_statement_ok(database, "ALTER TABLE alter_t ADD y YEAR NOT NULL");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM alter_t ORDER BY id",
            .values = alter_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "year alter add not null backfill",
        }
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE alter_t ALTER COLUMN y SET DEFAULT '70'");
    failures += expect_dml_ok(database, "INSERT INTO alter_t(id) VALUES (3)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM alter_t WHERE id = 3",
            .values = alter_default_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "year alter set default",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE replace_t (id INT, y YEAR, ynn YEAR NOT NULL DEFAULT 1970)"
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO replace_t VALUES (1, '70', DEFAULT), (2, NULL, 0)",
        2
    );
    failures +=
        expect_dml_ok(database, "REPLACE INTO replace_t SET id = 3, y = '69', ynn = TRUE", 1);
    failures += expect_statement_ok(
        database,
        "CREATE TABLE replace_src (id INT, y YEAR, ynn YEAR NOT NULL DEFAULT 1970)"
    );
    failures += expect_dml_ok(database, "INSERT INTO replace_src VALUES (4, 1, 1)", 1);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO replace_t (id, y, ynn) SELECT id, y, ynn FROM replace_src",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y, ynn FROM replace_t ORDER BY id",
            .values = replace_rows,
            .column_count = 3U,
            .row_count = year_replace_row_count,
            .context = "year replace rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE copied LIKE dml_t");
    failures += expect_dml_ok(
        database,
        "INSERT INTO copied (id, y, ynn) SELECT id, y, ynn FROM dml_t WHERE id = 3",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y, ynn FROM copied",
            .values = copied_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "year insert select copy",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE selected AS SELECT id, y FROM dml_t");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM selected WHERE id = 3",
            .values = selected_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "year create table select",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE index_t (id INT, y YEAR, KEY ky (y))");
    failures += expect_dml_ok(
        database,
        "INSERT INTO index_t VALUES (1, 2001), (2, '02'), (3, NULL), (4, 0), (5, '70')",
        year_index_row_count
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE unique_year (y YEAR, UNIQUE KEY uy (y))");
    failures += expect_dml_ok(database, "INSERT INTO unique_year VALUES (1901), (1970), (2000)", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM index_t ORDER BY y",
            .values = index_rows,
            .column_count = 2U,
            .row_count = year_index_row_count,
            .context = "year secondary indexes",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        MYLITE_FILE_PREAMBLE_SIZE,
        "year file preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen year success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, y FROM dml_t ORDER BY y DESC LIMIT 1",
            .values = (const char *const[]){"8", "2155"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "year reopened ordered select",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_year_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open year diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE bad_width_zero (y YEAR(0))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_year_display_width,
            .sqlstate = "HY000",
            .message_part = "Invalid display width. Use YEAR instead.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_width_five (y YEAR(5))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_year_display_width,
            .sqlstate = "HY000",
            .message_part = "Invalid display width. Use YEAR instead.",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_current_default (y YEAR DEFAULT CURRENT_TIMESTAMP)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'y'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_string_default (y YEAR DEFAULT 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'y'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_expression_default (y YEAR DEFAULT (2000 + 1))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'y'",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE err_insert (id INT, y YEAR NOT NULL)");
    failures += execute_error(
        database,
        "INSERT INTO err_insert VALUES (1, 1900)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'y' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO err_insert VALUES (1, 2156)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'y' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO err_insert VALUES (1, -1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'y' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO err_insert VALUES (1, 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_integer_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value: 'abc' for column 'y' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO err_insert VALUES (1, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'y' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO err_insert (id) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'y' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_attr (y YEAR UNSIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'UNSIGNED'",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_year_independent_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first year file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second year file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE years (id INT, y YEAR)");
    failures += expect_statement_ok(second, "CREATE TABLE years (id INT, y YEAR)");
    failures += expect_dml_ok(first, "INSERT INTO years VALUES (1, 2001)", 1);
    failures += expect_dml_ok(second, "INSERT INTO years VALUES (1, 1970)", 1);
    failures += expect_dml_ok(first, "UPDATE years SET y = 2155 WHERE id = 1", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT y FROM years",
            .values = (const char *const[]){"2155"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent year state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT y FROM years",
            .values = (const char *const[]){"1970"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent year state",
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
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "expected success for [%s], got %d/%s: %s\n",
            sql,
            mylite_diagnostics_errcode(diagnostics),
            mylite_diagnostics_sqlstate(diagnostics),
            mylite_diagnostics_errmsg(diagnostics)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures += expect_text(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += expect_contains(mylite_diagnostics_errmsg(diagnostics), expected.message_part, sql);
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

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(
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
        (struct expected_dml_result){.affected_rows = affected_rows, .warning_count = 0U}
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "DML affected");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
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
        "%s/mylite_year_type_%d_%s.mylite",
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
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
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
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
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
