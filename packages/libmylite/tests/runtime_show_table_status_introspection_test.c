#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    show_table_status_column_count = 18,
    status_name_column = 0,
    status_engine_column = 1,
    status_version_column = 2,
    status_row_format_column = 3,
    status_rows_column = 4,
    status_average_row_length_column = 5,
    status_data_length_column = 6,
    status_max_data_length_column = 7,
    status_index_length_column = 8,
    status_data_free_column = 9,
    status_auto_increment_column = 10,
    status_create_time_column = 11,
    status_update_time_column = 12,
    status_check_time_column = 13,
    status_collation_column = 14,
    status_checksum_column = 15,
    status_create_options_column = 16,
    status_comment_column = 17,
    datetime_text_length = 19,
    datetime_year_month_separator = 4,
    datetime_month_day_separator = 7,
    datetime_date_time_separator = 10,
    datetime_hour_minute_separator = 13,
    datetime_minute_second_separator = 16,
    decimal_base = 10,
    row_count_text_capacity = 32,
    suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_regexp_bracket = 3696,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_status_row {
    const char *name;
    const char *rows;
    const char *average_row_length;
    const char *index_length;
    const char *auto_increment;
};

struct status_cell_query {
    const char *sql;
    const char *table_name;
    size_t column_index;
    const char *context;
};

struct expected_status_cell {
    struct status_cell_query cell;
    const char *expected;
};

struct copied_status_cell {
    struct status_cell_query cell;
    char *buffer;
    size_t buffer_size;
};

struct expected_single_value {
    const char *sql;
    const char *expected;
    const char *context;
};

struct expected_text_difference {
    const char *left;
    const char *right;
    const char *context;
};

static const char *const status_columns[show_table_status_column_count] = {
    "Name",
    "Engine",
    "Version",
    "Row_format",
    "Rows",
    "Avg_row_length",
    "Data_length",
    "Max_data_length",
    "Index_length",
    "Data_free",
    "Auto_increment",
    "Create_time",
    "Update_time",
    "Check_time",
    "Collation",
    "Checksum",
    "Create_options",
    "Comment",
};

static int test_show_table_status_values_persistence_rename_and_drop(void);
static int test_show_table_status_where_filters(void);
static int test_show_table_status_diagnostics_and_unsupported_forms(void);
static int test_independent_show_table_status_handles(void);
static int create_status_schema(mylite_db *database);
static int expect_show_table_status_result(
    mylite_db *database,
    const char *sql,
    const struct expected_status_row *expected_rows,
    size_t expected_row_count,
    const char *context
);
static int expect_status_row(
    const mylite_result *result,
    const struct expected_status_row *expected,
    const char *context
);
static int expect_status_cell(mylite_db *database, struct expected_status_cell expected);
static int copy_status_cell(mylite_db *database, struct copied_status_cell request);
static int expect_single_value(mylite_db *database, struct expected_single_value expected);
static int find_result_row_by_name(
    const mylite_result *result,
    const char *name,
    size_t *out_row_index
);
static void wait_for_next_status_second(void);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_text_not_equal(struct expected_text_difference expected);
static int expect_datetime_text(const char *actual, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_show_table_status_values_persistence_rename_and_drop();
    failures += test_show_table_status_where_filters();
    failures += test_show_table_status_diagnostics_and_unsupported_forms();
    failures += test_independent_show_table_status_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_table_status_values_persistence_rename_and_drop(void) {
    static const struct expected_status_row initial_rows[] = {
        {.name = "a%b", .rows = "0", .average_row_length = "0"},
        {.name = "a_b", .rows = "0", .average_row_length = "0"},
        {.name = "empty_numbers", .rows = "0", .average_row_length = "0"},
        {.name = "numbers", .rows = "3", .average_row_length = "5461"},
        {.name = "primary_only", .rows = "0", .average_row_length = "0"},
        {.name = "rename_me", .rows = "1", .average_row_length = "16384"},
        {.name = "secondary_keyed",
         .rows = "0",
         .average_row_length = "0",
         .index_length = "16384"},
        {.name = "to_truncate", .rows = "2", .average_row_length = "8192"},
    };
    static const struct expected_status_row numbers_after_delete[] = {
        {.name = "numbers", .rows = "2", .average_row_length = "8192"},
    };
    static const struct expected_status_row truncated_rows[] = {
        {.name = "to_truncate", .rows = "0", .average_row_length = "0"},
    };
    static const struct expected_status_row renamed_rows[] = {
        {.name = "renamed_numbers", .rows = "1", .average_row_length = "16384"},
    };
    static const struct expected_status_row a_underscore_rows[] = {
        {.name = "a_b", .rows = "0", .average_row_length = "0"},
    };
    static const struct expected_status_row a_percent_rows[] = {
        {.name = "a%b", .rows = "0", .average_row_length = "0"},
    };
    static const struct expected_status_row other_rows[] = {
        {.name = "only_other", .rows = "0", .average_row_length = "0"},
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    const struct mylite_session_state *session = NULL;
    char numbers_update_time_before[row_count_text_capacity];
    char numbers_update_time_after[row_count_text_capacity];
    char numbers_update_time_after_zero[row_count_text_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += create_status_schema(database);
    failures += expect_status_cell(
        database,
        (struct expected_status_cell){
            .cell =
                {
                    .sql = "SHOW TABLE STATUS FROM app LIKE 'empty\\_numbers'",
                    .table_name = "empty_numbers",
                    .column_index = status_create_time_column,
                    .context = "create time UTC status",
                },
            .expected = "2023-11-14 22:13:20",
        }
    );
    failures += expect_status_cell(
        database,
        (struct expected_status_cell){
            .cell =
                {
                    .sql = "SHOW TABLE STATUS FROM app LIKE 'empty\\_numbers'",
                    .table_name = "empty_numbers",
                    .column_index = status_update_time_column,
                    .context = "initial update time UTC status",
                },
            .expected = NULL,
        }
    );
    failures += execute_statement_ok(database, "SET time_zone = '+02:00'");
    failures += expect_status_cell(
        database,
        (struct expected_status_cell){
            .cell =
                {
                    .sql = "SHOW TABLE STATUS FROM app LIKE 'empty\\_numbers'",
                    .table_name = "empty_numbers",
                    .column_index = status_create_time_column,
                    .context = "create time session time zone status",
                },
            .expected = "2023-11-15 00:13:20",
        }
    );
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT CREATE_TIME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = "
                   "'app' AND TABLE_NAME = 'empty_numbers'",
            .expected = "2023-11-15 00:13:20",
            .context = "information schema create time session time zone",
        }
    );
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT UPDATE_TIME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = "
                   "'app' AND TABLE_NAME = 'empty_numbers'",
            .expected = NULL,
            .context = "information schema initial update time",
        }
    );
    failures += expect_single_value(
        database,
        (struct expected_single_value){
            .sql = "SELECT INDEX_LENGTH FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = "
                   "'app' AND TABLE_NAME = 'secondary_keyed'",
            .expected = "16384",
            .context = "information schema secondary index length",
        }
    );
    failures += execute_statement_ok(database, "SET time_zone = '+00:00'");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS FROM app",
        initial_rows,
        sizeof(initial_rows) / sizeof(initial_rows[0]),
        "qualified initial status"
    );

    failures += execute_statement_ok(database, "USE app");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS",
        initial_rows,
        sizeof(initial_rows) / sizeof(initial_rows[0]),
        "selected schema status"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'a\\_%'",
        a_underscore_rows,
        sizeof(a_underscore_rows) / sizeof(a_underscore_rows[0]),
        "escaped underscore table status"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'a\\%%'",
        a_percent_rows,
        sizeof(a_percent_rows) / sizeof(a_percent_rows[0]),
        "escaped percent table status"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS IN other LIKE 'only\\_%'",
        other_rows,
        sizeof(other_rows) / sizeof(other_rows[0]),
        "explicit schema filtered status"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'missing%'",
        NULL,
        0U,
        "no-match table status"
    );
    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation after show table status"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation after show table status"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show table status"
    );

    failures += copy_status_cell(
        database,
        (struct copied_status_cell){
            .cell =
                {
                    .sql = "SHOW TABLE STATUS LIKE 'numbers'",
                    .table_name = "numbers",
                    .column_index = status_update_time_column,
                    .context = "numbers update time before update",
                },
            .buffer = numbers_update_time_before,
            .buffer_size = sizeof(numbers_update_time_before),
        }
    );
    wait_for_next_status_second();
    failures += execute_statement_ok(database, "UPDATE numbers SET i = 99 WHERE id = 2");
    failures += copy_status_cell(
        database,
        (struct copied_status_cell){
            .cell =
                {
                    .sql = "SHOW TABLE STATUS LIKE 'numbers'",
                    .table_name = "numbers",
                    .column_index = status_update_time_column,
                    .context = "numbers update time after update",
                },
            .buffer = numbers_update_time_after,
            .buffer_size = sizeof(numbers_update_time_after),
        }
    );
    failures += expect_text_not_equal((struct expected_text_difference){
        .left = numbers_update_time_after,
        .right = numbers_update_time_before,
        .context = "positive update changes table update time",
    });
    wait_for_next_status_second();
    failures += execute_statement_ok(database, "UPDATE numbers SET i = 99 WHERE id = 999");
    failures += copy_status_cell(
        database,
        (struct copied_status_cell){
            .cell =
                {
                    .sql = "SHOW TABLE STATUS LIKE 'numbers'",
                    .table_name = "numbers",
                    .column_index = status_update_time_column,
                    .context = "numbers update time after zero update",
                },
            .buffer = numbers_update_time_after_zero,
            .buffer_size = sizeof(numbers_update_time_after_zero),
        }
    );
    failures += expect_text_or_null(
        numbers_update_time_after_zero,
        numbers_update_time_after,
        "zero-row update preserves table update time"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'numbers'",
        (const struct expected_status_row[]){
            {.name = "numbers", .rows = "3", .average_row_length = "5461"},
        },
        1U,
        "status after update"
    );
    failures += execute_statement_ok(database, "DELETE FROM numbers WHERE id = 1");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'numbers'",
        numbers_after_delete,
        sizeof(numbers_after_delete) / sizeof(numbers_after_delete[0]),
        "status after delete"
    );
    failures += execute_statement_ok(database, "TRUNCATE TABLE to_truncate");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'to\\_truncate'",
        truncated_rows,
        sizeof(truncated_rows) / sizeof(truncated_rows[0]),
        "status after truncate"
    );
    failures += execute_statement_ok(database, "RENAME TABLE rename_me TO renamed_numbers");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'renamed%'",
        renamed_rows,
        sizeof(renamed_rows) / sizeof(renamed_rows[0]),
        "status after rename"
    );
    failures += execute_statement_ok(database, "DROP TABLE renamed_numbers");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'renamed%'",
        NULL,
        0U,
        "status after drop"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen values file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'numbers'",
        numbers_after_delete,
        sizeof(numbers_after_delete) / sizeof(numbers_after_delete[0]),
        "reopened numbers status"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'to\\_truncate'",
        truncated_rows,
        sizeof(truncated_rows) / sizeof(truncated_rows[0]),
        "reopened truncated status"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_table_status_where_filters(void) {
    static const struct expected_status_row numbers_row[] = {
        {.name = "numbers", .rows = "3", .average_row_length = "5461"},
    };
    static const struct expected_status_row auto_and_numbers_rows[] = {
        {.name = "auto_numbers",
         .rows = "2",
         .average_row_length = "8192",
         .auto_increment = "3"},
        {.name = "numbers", .rows = "3", .average_row_length = "5461"},
    };
    static const struct expected_status_row auto_numbers_row[] = {
        {.name = "auto_numbers",
         .rows = "2",
         .average_row_length = "8192",
         .auto_increment = "3"},
    };
    static const struct expected_status_row name_case_row[] = {
        {.name = "NameCase", .rows = "0", .average_row_length = "0"},
    };
    static const struct expected_status_row tmp_table1_row[] = {
        {.name = "_tmp_table1", .rows = "0", .average_row_length = "0"},
    };
    static const struct expected_status_row tmp_table2_row[] = {
        {.name = "_tmp_table2", .rows = "0", .average_row_length = "0"},
    };
    static const struct expected_status_row substring_null_rows[] = {
        {.name = "NameCase", .rows = "0", .average_row_length = "0"},
        {.name = "_tmp_table1", .rows = "0", .average_row_length = "0"},
        {.name = "_tmp_table2", .rows = "0", .average_row_length = "0"},
    };
    static const struct expected_status_row other_rows[] = {
        {.name = "only_other", .rows = "0", .average_row_length = "0"},
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "where") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open where file");
    failures += create_status_schema(database);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE auto_numbers (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, i INT NULL)"
    );
    failures += execute_statement_ok(database, "INSERT INTO auto_numbers(i) VALUES (10), (20)");
    failures += execute_statement_ok(database, "CREATE TABLE _tmp_table1 (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE _tmp_table2 (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE NameCase (id INT NOT NULL)");

    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name = 'numbers'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where name equality"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name = 'NUMBERS'",
        NULL,
        0U,
        "where name equality case-sensitive"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE `Name` LIKE 'num%'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where name like"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name REGEXP '^num'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where name regexp"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name REGEXP '^NUM'",
        NULL,
        0U,
        "where name regexp case-sensitive"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name RLIKE '^NUM'",
        NULL,
        0U,
        "where name rlike case-sensitive"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name NOT REGEXP '^NUM' AND Name = 'numbers'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where name not regexp case-sensitive"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name NOT RLIKE '^NUM' AND Name = 'numbers'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where name not rlike case-sensitive"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Engine RLIKE '^innodb$' AND Name = 'numbers'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where engine rlike"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Engine = 'INNODB' AND Name IN ('numbers','auto_numbers')",
        auto_and_numbers_rows,
        sizeof(auto_and_numbers_rows) / sizeof(auto_and_numbers_rows[0]),
        "where engine and in"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR(Name, 11, 1) = '1'",
        tmp_table1_row,
        sizeof(tmp_table1_row) / sizeof(tmp_table1_row[0]),
        "where substring name comma comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE SUBSTRING(Name FROM 11 FOR 1) = '2'",
        tmp_table2_row,
        sizeof(tmp_table2_row) / sizeof(tmp_table2_row[0]),
        "where substring name from-for comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE MID(Name, 1, 1) = 'N'",
        name_case_row,
        sizeof(name_case_row) / sizeof(name_case_row[0]),
        "where mid name comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE MID(Name, 1, 1) = 'n' AND Name = 'NameCase'",
        NULL,
        0U,
        "where substring name comparison case-sensitive"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR(Engine, 1, 1) = 'i' AND Name = 'numbers'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where substring engine comparison case-insensitive"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR(Name, NULL, 1) <=> NULL AND Name IN "
        "('_tmp_table1','_tmp_table2','NameCase')",
        substring_null_rows,
        sizeof(substring_null_rows) / sizeof(substring_null_rows[0]),
        "where substring null position null safe"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR(Name, 1, 0) = '' AND Name IN "
        "('_tmp_table1','_tmp_table2','NameCase')",
        substring_null_rows,
        sizeof(substring_null_rows) / sizeof(substring_null_rows[0]),
        "where substring zero length comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE `Rows` = '3'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where rows numeric string comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE `Rows` = 3",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where rows numeric integer comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE `Rows` = '03'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where rows leading zero numeric string comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE `Rows` > '10' AND Name = 'numbers'",
        NULL,
        0U,
        "where rows numeric ordering comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Data_length = '016384' AND Name = 'numbers'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where data length leading zero numeric string comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment IS NULL AND Name IN ('numbers','auto_numbers')",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where auto increment is null"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment IS NOT NULL AND Name IN "
        "('numbers','auto_numbers')",
        auto_numbers_row,
        sizeof(auto_numbers_row) / sizeof(auto_numbers_row[0]),
        "where auto increment is not null"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment >= 3 AND Name IN ('numbers','auto_numbers')",
        auto_numbers_row,
        sizeof(auto_numbers_row) / sizeof(auto_numbers_row[0]),
        "where auto increment numeric integer comparison"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment REGEXP '^3$' AND Name IN "
        "('numbers','auto_numbers')",
        auto_numbers_row,
        sizeof(auto_numbers_row) / sizeof(auto_numbers_row[0]),
        "where auto increment regexp"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment REGEXP '.*' AND Name = 'numbers'",
        NULL,
        0U,
        "where regexp skips null auto increment"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment <=> NULL AND Name IN ('numbers','auto_numbers')",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where auto increment null safe"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment IN (NULL, '03') AND Name IN "
        "('numbers','auto_numbers')",
        auto_numbers_row,
        sizeof(auto_numbers_row) / sizeof(auto_numbers_row[0]),
        "where auto increment numeric in leading zero"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Auto_increment IN (NULL, 3) AND Name IN "
        "('numbers','auto_numbers')",
        auto_numbers_row,
        sizeof(auto_numbers_row) / sizeof(auto_numbers_row[0]),
        "where auto increment numeric integer in"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name NOT IN (NULL, 'numbers') AND Name IN "
        "('numbers','auto_numbers')",
        NULL,
        0U,
        "where not in null"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE (Name = 'numbers' OR Name = 'missing') AND NOT Engine = "
        "'memory'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "where or and not"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS IN other WHERE Name = 'only_other'",
        other_rows,
        sizeof(other_rows) / sizeof(other_rows[0]),
        "where explicit schema"
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name = 'missing'",
        NULL,
        0U,
        "where no match"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after show table status where"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen where file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE Name = 'numbers'",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "reopened where name equality"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_table_status_diagnostics_and_unsupported_forms(void) {
    static const struct expected_status_row numbers_row[] = {
        {.name = "numbers", .rows = "3", .average_row_length = "5461"},
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "SHOW TABLE STATUS",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += create_status_schema(database);
    failures += execute_statement_ok(database, "CREATE DATABASE empty_schema");
    failures += execute_error(
        database,
        "SHOW TABLE STATUS FROM missing_schema",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'missing_schema'",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name '_mylite_reserved'",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS FROM empty_schema WHERE missing = 'x'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS FROM empty_schema WHERE `Rows` = 3",
        NULL,
        0U,
        "empty schema numeric integer where"
    );

    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS LIKE 'missing%'",
        NULL,
        0U,
        "diagnostic no-match status"
    );
    failures += execute_error(
        database,
        "SHOW FULL TABLE STATUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW EXTENDED TABLE STATUS",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE missing = 'x'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE tables.Name = 'numbers'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'tables.Name' in 'where clause'",
        }
    );
    failures += expect_show_table_status_result(
        database,
        "SHOW TABLE STATUS WHERE `Rows` = 3",
        numbers_row,
        sizeof(numbers_row) / sizeof(numbers_row[0]),
        "diagnostic numeric integer where"
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE Name = 3",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SHOW TABLE STATUS WHERE integer literal predicates support only numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR('abc', 1, 1) = 'a'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW TABLE STATUS WHERE supports only output columns",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR(Name, '1', 1) = 'n'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice functions support only integer, boolean, and NULL position literals",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR(Auto_increment, '1', 1) IS NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "string slice functions support only integer, boolean, and NULL position literals",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE SUBSTR(Name, 1, 1) = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SHOW TABLE STATUS WHERE integer literal predicates support only numeric columns",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE `Rows` = 'abc'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "SHOW TABLE STATUS WHERE numeric columns support only unsigned decimal string "
                "predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE LOWER(Name) = 'numbers'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE Name REGEXP '['",
        (struct expected_sql_error){
            .code = mysql_error_regexp_bracket,
            .sqlstate = "HY000",
            .message_part = "unclosed bracket expression",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE Name = 'numbers' XOR Engine = 'InnoDB'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW TABLE STATUS WHERE does not support XOR predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS LIKE 'numbers' WHERE Name = 'numbers'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS WHERE Name = 'numbers' ORDER BY Name",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS LIKE NULL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS LIKE N'a%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS LIKE 'a%' FROM app",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW TABLE STATUS LIKE 'a\\0%'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW LIKE does not support NUL bytes in patterns",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_show_table_status_handles(void) {
    static const struct expected_status_row first_rows[] = {
        {.name = "alpha", .rows = "1", .average_row_length = "16384"},
    };
    static const struct expected_status_row second_rows[] = {
        {.name = "beta", .rows = "2", .average_row_length = "8192"},
    };
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE alpha (id INT NOT NULL)");
    failures += execute_statement_ok(first, "INSERT INTO alpha VALUES (1)");
    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(second, "CREATE TABLE beta (id INT NOT NULL)");
    failures += execute_statement_ok(second, "INSERT INTO beta VALUES (1), (2)");

    failures += expect_show_table_status_result(
        first,
        "SHOW TABLE STATUS",
        first_rows,
        sizeof(first_rows) / sizeof(first_rows[0]),
        "first handle status"
    );
    failures += expect_show_table_status_result(
        first,
        "SHOW TABLE STATUS WHERE Name = 'alpha'",
        first_rows,
        sizeof(first_rows) / sizeof(first_rows[0]),
        "first handle where status"
    );
    failures += expect_show_table_status_result(
        second,
        "SHOW TABLE STATUS",
        second_rows,
        sizeof(second_rows) / sizeof(second_rows[0]),
        "second handle status"
    );
    failures += expect_show_table_status_result(
        second,
        "SHOW TABLE STATUS WHERE Name = 'beta'",
        second_rows,
        sizeof(second_rows) / sizeof(second_rows[0]),
        "second handle where status"
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int create_status_schema(mylite_db *database) {
    int failures = 0;

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "CREATE DATABASE other");
    failures += execute_statement_ok(database, "SET time_zone = '+00:00'");
    failures += execute_statement_ok(database, "SET timestamp = 1700000000");
    failures += execute_statement_ok(database, "CREATE TABLE other.only_other (id INT NOT NULL)");
    failures += execute_statement_ok(database, "USE app");
    failures +=
        execute_statement_ok(database, "CREATE TABLE empty_numbers (id INT NOT NULL, i INT NULL)");
    failures +=
        execute_statement_ok(database, "CREATE TABLE numbers (id INT NOT NULL, i INT NULL)");
    failures +=
        execute_statement_ok(database, "INSERT INTO numbers VALUES (1, NULL), (2, 20), (3, 30)");
    failures += execute_statement_ok(database, "CREATE TABLE primary_only (id INT PRIMARY KEY)");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE secondary_keyed (id INT PRIMARY KEY, i INT, KEY i_key (i))"
    );
    failures += execute_statement_ok(database, "CREATE TABLE `a%b` (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE `a_b` (id INT NOT NULL)");
    failures += execute_statement_ok(database, "CREATE TABLE rename_me (id INT NOT NULL)");
    failures += execute_statement_ok(database, "INSERT INTO rename_me VALUES (1)");
    failures += execute_statement_ok(database, "CREATE TABLE to_truncate (id INT NOT NULL)");
    failures += execute_statement_ok(database, "INSERT INTO to_truncate VALUES (1), (2)");

    return failures;
}

static int expect_show_table_status_result(
    mylite_db *database,
    const char *sql,
    const struct expected_status_row *expected_rows,
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), show_table_status_column_count, context);
    for (size_t column_index = 0U; column_index < show_table_status_column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            status_columns[column_index],
            context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row_index = 0U; row_index < expected_row_count; ++row_index) {
        failures += expect_status_row(result, &expected_rows[row_index], context);
    }
    failures += expect_row_count(database, -1, context);

    mylite_result_free(result);
    return failures;
}

static int expect_status_row(
    const mylite_result *result,
    const struct expected_status_row *expected,
    const char *context
) {
    size_t row_index = 0U;
    int failures = find_result_row_by_name(result, expected->name, &row_index);

    if (failures != 0) {
        (void)fprintf(stderr, "%s: missing status row for %s\n", context, expected->name);
        return failures;
    }

    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_name_column),
        expected->name,
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_engine_column),
        "InnoDB",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_version_column),
        "10",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_row_format_column),
        "Dynamic",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_rows_column),
        expected->rows,
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_average_row_length_column),
        expected->average_row_length,
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_data_length_column),
        "16384",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_max_data_length_column),
        "0",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_index_length_column),
        expected->index_length == NULL ? "0" : expected->index_length,
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_data_free_column),
        "0",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_auto_increment_column),
        expected->auto_increment,
        context
    );
    failures += expect_datetime_text(
        mylite_result_value_text(result, row_index, status_create_time_column),
        context
    );
    if (mylite_result_value_text(result, row_index, status_update_time_column) != NULL) {
        failures += expect_datetime_text(
            mylite_result_value_text(result, row_index, status_update_time_column),
            context
        );
    }
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_check_time_column),
        NULL,
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_collation_column),
        "utf8mb4_0900_ai_ci",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_checksum_column),
        NULL,
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_create_options_column),
        "",
        context
    );
    failures += expect_text_or_null(
        mylite_result_value_text(result, row_index, status_comment_column),
        "",
        context
    );

    return failures;
}

static int expect_status_cell(mylite_db *database, struct expected_status_cell expected) {
    char actual[row_count_text_capacity];
    mylite_result *result = NULL;
    size_t row_index = 0U;
    int failures = 0;

    if (expected.expected == NULL) {
        failures = execute_ok(database, expected.cell.sql, &result);
        if (result == NULL) {
            return failures + 1;
        }
        failures += find_result_row_by_name(result, expected.cell.table_name, &row_index);
        if (failures == 0) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, expected.cell.column_index),
                NULL,
                expected.cell.context
            );
        }
        mylite_result_free(result);
        return failures;
    }

    failures = copy_status_cell(
        database,
        (struct copied_status_cell){
            .cell = expected.cell,
            .buffer = actual,
            .buffer_size = sizeof(actual),
        }
    );
    if (failures == 0) {
        failures += expect_text_or_null(actual, expected.expected, expected.cell.context);
    }
    return failures;
}

static int copy_status_cell(mylite_db *database, struct copied_status_cell request) {
    mylite_result *result = NULL;
    size_t row_index = 0U;
    const char *value = NULL;
    int failures = execute_ok(database, request.cell.sql, &result);

    if (request.buffer == NULL || request.buffer_size == 0U) {
        mylite_result_free(result);
        return failures + 1;
    }
    request.buffer[0] = '\0';
    if (result == NULL) {
        return failures + 1;
    }
    failures += find_result_row_by_name(result, request.cell.table_name, &row_index);
    if (failures == 0) {
        value = mylite_result_value_text(result, row_index, request.cell.column_index);
        if (value == NULL) {
            (void)fprintf(stderr, "%s: expected non-NULL status cell\n", request.cell.context);
            ++failures;
        } else if (
            snprintf(request.buffer, request.buffer_size, "%s", value) < 0 ||
            strlen(value) >= request.buffer_size
        ) {
            (void)fprintf(stderr, "%s: status cell buffer too small\n", request.cell.context);
            ++failures;
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_single_value(mylite_db *database, struct expected_single_value expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }
    failures += expect_size(mylite_result_column_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    if (mylite_result_column_count(result) == 1U && mylite_result_row_count(result) == 1U) {
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            expected.expected,
            expected.context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int find_result_row_by_name(
    const mylite_result *result,
    const char *name,
    size_t *out_row_index
) {
    size_t row_count = mylite_result_row_count(result);

    for (size_t row_index = 0U; row_index < row_count; ++row_index) {
        const char *value = mylite_result_value_text(result, row_index, status_name_column);

        if (value != NULL && strcmp(value, name) == 0) {
            *out_row_index = row_index;
            return 0;
        }
    }

    return 1;
}

static void wait_for_next_status_second(void) {
#ifdef _WIN32
    Sleep(1100U);
#else
    sleep(1U);
#endif
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_row_count(result) == 1U && mylite_result_column_count(result) == 1U) {
        failures += expect_int64(
            strtoll(mylite_result_value_text(result, 0U, 0U), NULL, decimal_base),
            expected,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        (void)fprintf(stderr, "%s: missing result\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_show_table_status_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-journal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        (void)fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        (void)fprintf(stderr, "failed to read %s\n", path);
        (void)fclose(file);
        return 1;
    }
    (void)fclose(file);

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    (void)fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    (void)fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    (void)fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    (void)fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    (void)fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}

static int expect_text_not_equal(struct expected_text_difference expected) {
    if (expected.left != NULL && expected.right != NULL &&
        strcmp(expected.left, expected.right) != 0) {
        return 0;
    }

    (void)fprintf(
        stderr,
        "%s: expected different values, both were [%s]\n",
        expected.context,
        expected.left == NULL ? "NULL" : expected.left
    );
    return 1;
}

static int expect_datetime_text(const char *actual, const char *context) {
    if (actual != NULL && strlen(actual) == datetime_text_length &&
        actual[datetime_year_month_separator] == '-' &&
        actual[datetime_month_day_separator] == '-' &&
        actual[datetime_date_time_separator] == ' ' &&
        actual[datetime_hour_minute_separator] == ':' &&
        actual[datetime_minute_second_separator] == ':') {
        return 0;
    }

    (void)fprintf(
        stderr,
        "%s: expected DATETIME text, got [%s]\n",
        context,
        actual == NULL ? "NULL" : actual
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    (void)fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
