#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
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
    sqlite_sql_capacity = 512,
    expected_generation_after_schema = 2,
    expected_generation_after_create = 3,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_column_specified_twice = 1110,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_data_out_of_range = 1264,
    mysql_error_field_no_default = 1364,
    mysql_error_bad_null = 1048,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_insert_select_persistence_rename_and_drop(void);
static int test_failure_diagnostics_and_unwinding(void);
static int test_integer_ranges_and_independent_handles(void);
static int create_numbers_table(mylite_db *database);
static int seed_schema(mylite_db *database, const char *name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_result(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
);
static int expect_select_value(
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
static int execute_sql(sqlite3 *connection, const char *sql);
static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists);
static int query_physical_row_count(sqlite3 *connection, const char *table_name, int *out_count);
static int create_blocking_insert_trigger(sqlite3 *connection, const char *table_name);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
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

    failures += test_insert_select_persistence_rename_and_drop();
    failures += test_failure_diagnostics_and_unwinding();
    failures += test_integer_ranges_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_insert_select_persistence_rename_and_drop(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t generation_before_insert = 0U;
    uint64_t sqlite_generation_before_insert = 0U;
    int has_physical_table = 0;
    int physical_rows = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "persistence") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open persistence file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    failures += expect_dml_result(result, 0, "USE result");
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database);

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read numbers table"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_insert = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_insert = session->sqlite_schema_generation;
    }

    failures += execute_ok(database, "INSERT INTO numbers VALUES (1, 2, 3, 4, NULL, 5)", &result);
    failures += expect_dml_result(result, 1, "full-row insert result");
    mylite_result_free(result);
    result = NULL;

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_insert,
            "insert leaves catalog generation unchanged"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_insert,
            "insert leaves SQLite schema generation unchanged"
        );
    }

    failures += execute_ok(database, "SELECT * FROM numbers", &result);
    failures += expect_size(mylite_result_column_count(result), 6U, "SELECT * column count");
    failures += expect_text(mylite_result_column_name(result, 0U), "i", "SELECT * column i");
    failures += expect_text(mylite_result_column_name(result, 1U), "iu", "SELECT * column iu");
    failures += expect_text(mylite_result_column_name(result, 2U), "b", "SELECT * column b");
    failures += expect_text(mylite_result_column_name(result, 3U), "bu", "SELECT * column bu");
    failures += expect_text(mylite_result_column_name(result, 4U), "n", "SELECT * column n");
    failures += expect_text(mylite_result_column_name(result, 5U), "nn", "SELECT * column nn");
    failures += expect_size(mylite_result_row_count(result), 1U, "SELECT * row count");
    failures += expect_select_value(result, 0U, 0U, "1", "SELECT * i");
    failures += expect_select_value(result, 0U, 1U, "2", "SELECT * iu");
    failures += expect_select_value(result, 0U, 2U, "3", "SELECT * b");
    failures += expect_select_value(result, 0U, 3U, "4", "SELECT * bu");
    failures += expect_select_value(result, 0U, 4U, NULL, "SELECT * n");
    failures += expect_select_value(result, 0U, 5U, "5", "SELECT * nn");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "SELECT affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "SELECT warning count");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "INSERT INTO numbers (nn, i) VALUES (6, +7)", &result);
    failures += expect_dml_result(result, 1, "explicit column insert result");
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO numbers (i, nn) VALUES (8, 9), (10, 11)", &result);
    failures += expect_dml_result(result, 2, "multi-row insert result");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT i, nn FROM numbers", &result);
    failures += expect_size(mylite_result_column_count(result), 2U, "projection column count");
    failures += expect_text(mylite_result_column_name(result, 0U), "i", "projection column i");
    failures += expect_text(mylite_result_column_name(result, 1U), "nn", "projection column nn");
    failures += expect_size(mylite_result_row_count(result), 4U, "projection row count");
    failures += expect_select_value(result, 0U, 0U, "1", "projection row 1 i");
    failures += expect_select_value(result, 0U, 1U, "5", "projection row 1 nn");
    failures += expect_select_value(result, 1U, 0U, "7", "projection row 2 i");
    failures += expect_select_value(result, 1U, 1U, "6", "projection row 2 nn");
    failures += expect_select_value(result, 2U, 0U, "8", "projection row 3 i");
    failures += expect_select_value(result, 2U, 1U, "9", "projection row 3 nn");
    failures += expect_select_value(result, 3U, 0U, "10", "projection row 4 i");
    failures += expect_select_value(result, 3U, 1U, "11", "projection row 4 nn");
    mylite_result_free(result);
    result = NULL;

    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += table_exists(sqlite, table.physical_name, &has_physical_table);
        failures += query_physical_row_count(sqlite, table.physical_name, &physical_rows);
    }
    failures += expect_int(has_physical_table, 1, "physical table exists after insert");
    failures += expect_int(physical_rows, 4, "physical row count after insert");

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "row lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen row file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT i, nn FROM numbers", &result);
    failures += expect_size(mylite_result_row_count(result), 4U, "reopened row count");
    failures += expect_select_value(result, 3U, 0U, "10", "reopened last i");
    failures += expect_select_value(result, 3U, 1U, "11", "reopened last nn");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    failures += expect_dml_result(result, 0, "rename with rows result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT * FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += execute_ok(database, "SELECT i, nn FROM renamed_numbers", &result);
    failures += expect_size(mylite_result_row_count(result), 4U, "renamed row count");
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    failures += expect_dml_result(result, 0, "drop with rows result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "SELECT * FROM renamed_numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    has_physical_table = 1;
    if (sqlite != NULL) {
        failures += table_exists(sqlite, table.physical_name, &has_physical_table);
    }
    failures += expect_int(has_physical_table, 0, "drop removes physical table");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_failure_diagnostics_and_unwinding(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_catalog *catalog = NULL;
    uint64_t generation_before_failures = 0U;
    int physical_rows = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "failures") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open failures file");
    failures += seed_schema(database, "app");
    failures += execute_error(
        database,
        "INSERT INTO numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO missing_schema.numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database);

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read failure schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read failure table"
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        generation_before_failures = catalog->generation;
    }

    failures += execute_ok(database, "INSERT INTO numbers (i, nn) VALUES (1, 2)", &result);
    failures += expect_dml_result(result, 1, "seed insert result");
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "INSERT INTO missing_table VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM missing_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO _mylite_reserved VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "SELECT * FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (missing) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (i, i) VALUES (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_column_specified_twice,
            .sqlstate = "42000",
            .message_part = "Column 'i' specified twice",
        }
    );
    failures += execute_error(
        database,
        "SELECT missing FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT numbers.i FROM numbers",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only unqualified table columns",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (i, nn) VALUES (1, 2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (i, nn) VALUES (3, 4), (5)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 2",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (nn) VALUES (NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (i) VALUES (6)",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'nn' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (i, nn) VALUES ('7', 8)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (i, nn) VALUES (1 + 2, 8)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_physical_row_count(sqlite, table.physical_name, &physical_rows);
    }
    failures += expect_int(physical_rows, 1, "failed semantic inserts leave row count");

    failures += execute_error(
        database,
        "INSERT INTO numbers (i, nn) VALUES (9, 10), (2147483648, 11)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 2",
        }
    );
    physical_rows = 0;
    if (sqlite != NULL) {
        failures += query_physical_row_count(sqlite, table.physical_name, &physical_rows);
    }
    failures += expect_int(physical_rows, 1, "conversion failure is atomic");

    if (sqlite != NULL) {
        failures += create_blocking_insert_trigger(sqlite, table.physical_name);
    }
    failures += execute_error(
        database,
        "INSERT INTO numbers (i, nn) VALUES (40, 41), (41, 42)",
        (struct expected_sql_error){
            .code = mysql_error_unknown,
            .sqlstate = "HY000",
            .message_part = "internal SQLite row operation failed",
        }
    );
    physical_rows = 0;
    if (sqlite != NULL) {
        failures += query_physical_row_count(sqlite, table.physical_name, &physical_rows);
    }
    failures += expect_int(physical_rows, 1, "physical failure rolls back inserted rows");

    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            generation_before_failures,
            "row failures do not advance catalog generation"
        );
    }

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_integer_ranges_and_independent_handles(void) {
    char ranges_path[test_path_capacity];
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(ranges_path, sizeof(ranges_path), "ranges") != 0 ||
        make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(ranges_path);
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(ranges_path, &database), MYLITE_OK, "open ranges file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database);
    failures += execute_ok(
        database,
        "INSERT INTO numbers (i, iu, b, bu, nn) VALUES "
        "(-2147483648, 0, -9223372036854775808, 0, 12), "
        "(2147483647, 4294967295, 9223372036854775807, 9223372036854775807, 13), "
        "(NULL, NULL, NULL, NULL, 14), "
        "(+1, +2, +3, +4, 15)",
        &result
    );
    failures += expect_dml_result(result, 4, "range insert result");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "SELECT i, iu, b, bu, nn FROM numbers", &result);
    failures += expect_size(mylite_result_row_count(result), 4U, "range row count");
    failures += expect_select_value(result, 0U, 0U, "-2147483648", "signed int min");
    failures += expect_select_value(result, 0U, 2U, "-9223372036854775808", "bigint min");
    failures += expect_select_value(result, 1U, 1U, "4294967295", "unsigned int max");
    failures += expect_select_value(result, 1U, 3U, "9223372036854775807", "bigint unsigned max");
    failures += expect_select_value(result, 2U, 0U, NULL, "nullable int omitted null");
    failures += expect_select_value(result, 3U, 0U, "1", "unary plus int");
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "INSERT INTO numbers (i, nn) VALUES (-2147483649, 20)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (iu, nn) VALUES (-1, 21)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (iu, nn) VALUES (4294967296, 22)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (b, nn) VALUES (9223372036854775808, 23)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'b' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO numbers (bu, nn) VALUES (9223372036854775808, 24)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'bu' at row 1",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first row file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second row file");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(first);
    failures += create_numbers_table(second);
    failures += execute_ok(first, "INSERT INTO numbers (i, nn) VALUES (101, 102)", &result);
    failures += expect_dml_result(result, 1, "first handle insert");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "INSERT INTO numbers (i, nn) VALUES (201, 202)", &result);
    failures += expect_dml_result(result, 1, "second handle insert");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(first, "SELECT i, nn FROM numbers", &result);
    failures += expect_select_value(result, 0U, 0U, "101", "first handle row");
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "SELECT i, nn FROM numbers", &result);
    failures += expect_select_value(result, 0U, 0U, "201", "second handle row");
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    remove_related_files(ranges_path);

    return failures;
}

static int create_numbers_table(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "CREATE TABLE numbers ("
        "i INT, "
        "iu INTEGER UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT NULL, "
        "nn INT NOT NULL)",
        &result
    );

    failures += expect_dml_result(result, 0, "CREATE TABLE numbers result");
    mylite_result_free(result);

    return failures;
}

static int seed_schema(mylite_db *database, const char *name) {
    struct mylite_catalog_schema_descriptor schema = {0};

    return expect_int(
        mylite_catalog_create_schema(database, name, &schema),
        MYLITE_OK,
        "seed schema"
    );
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

static int expect_dml_result(
    const mylite_result *result,
    int64_t affected_rows,
    const char *context
) {
    int failures = 0;

    failures += expect_size(mylite_result_column_count(result), 0U, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    return failures;
}

static int expect_select_value(
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
        "%s/mylite_row_values_lifecycle_%d_%s.mylite",
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

static int execute_sql(sqlite3 *connection, const char *sql) {
    int rc = sqlite3_exec(connection, sql, NULL, NULL, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite exec failed for '%s': %d\n", sql, rc);
        return 1;
    }

    return 0;
}

static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(
        connection,
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = ?1",
        -1,
        &statement,
        NULL
    );

    *out_exists = 0;
    if (rc == SQLITE_OK) {
        rc = sqlite3_bind_text(statement, 1, table_name, -1, SQLITE_TRANSIENT);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW) {
            *out_exists = sqlite3_column_int(statement, 0) == 0 ? 0 : 1;
            rc = SQLITE_OK;
        }
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to query table existence for %s: %d\n", table_name, rc);
        return 1;
    }

    return 0;
}

static int query_physical_row_count(sqlite3 *connection, const char *table_name, int *out_count) {
    char sql[sqlite_sql_capacity];
    sqlite3_stmt *statement = NULL;
    int written = snprintf(sql, sizeof(sql), "SELECT count(*) FROM \"%s\"", table_name);
    int rc = SQLITE_OK;

    *out_count = 0;
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "physical count SQL is too long\n");
        return 1;
    }

    rc = sqlite3_prepare_v2(connection, sql, -1, &statement, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW) {
            *out_count = sqlite3_column_int(statement, 0);
            rc = SQLITE_OK;
        }
    }
    if (statement != NULL && sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "failed to query physical row count for %s: %d\n", table_name, rc);
        return 1;
    }

    return 0;
}

static int create_blocking_insert_trigger(sqlite3 *connection, const char *table_name) {
    char sql[sqlite_sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TRIGGER block_row_insert "
        "BEFORE INSERT ON \"%s\" "
        "WHEN NEW.\"i\" = 41 "
        "BEGIN SELECT RAISE(ABORT, 'blocked row insert'); END",
        table_name
    );

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "blocking trigger SQL is too long\n");
        return 1;
    }

    return execute_sql(connection, sql);
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

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
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
        fprintf(stderr, "%s: condition failed\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected '%s', got '%s'\n",
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
            "%s: expected '%s' to contain '%s'\n",
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
