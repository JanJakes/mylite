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
    sql_capacity = 2048,
    replace_integer_family_column_count = 8,
    replace_success_row_count = 6,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_column_specified_twice = 1110,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_data_out_of_range = 1264,
    mysql_error_field_no_default = 1364,
    mysql_error_bad_null = 1048,
    mysql_error_truncated_wrong_value = 1366,
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

static int test_replace_values_success_persistence_rename_and_drop(void);
static int test_replace_values_schema_resolution_and_diagnostics(void);
static int test_replace_values_independent_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_numbers_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_replace_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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
static int query_physical_row_count(sqlite3 *connection, const char *table_name, int *out_count);
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

    failures += test_replace_values_success_persistence_rename_and_drop();
    failures += test_replace_values_schema_resolution_and_diagnostics();
    failures += test_replace_values_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_replace_values_success_persistence_rename_and_drop(void) {
    static const char *const integer_family_values[] = {
        "-2147483648",
        "2147483647",
        "4294967295",
        "0",
        "-9223372036854775808",
        "9223372036854775807",
        NULL,
        "9",
    };
    static const char *const ordered_rows[] = {
        "1",
        "-3",
        "-4",
        "6",
        "1",
        "0",
        "1",
        "-2147483648",
        "9",
        "3",
        NULL,
        "10",
        "4",
        NULL,
        "11",
        "5",
        NULL,
        "12",
    };
    static const char *const row_count_two[] = {"2"};
    static const char *const persisted_row[] = {"1", "-2147483648", "9"};
    static const char *const value_row_syntax_rows[] = {"8", "80", "9", "90", "10", "100"};
    static const char *const renamed_row[] = {"7", "77"};
    static const char *const last_insert_and_row_count[] = {"0", "1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    uint64_t catalog_generation_before_replace = 0U;
    uint64_t sqlite_generation_before_replace = 0U;
    int physical_rows = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += seed_schema(database, "app");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database, "numbers");

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation_before_replace = catalog->generation;
    }
    if (session != NULL) {
        sqlite_generation_before_replace = session->sqlite_schema_generation;
    }

    failures += expect_replace_ok(
        database,
        "REPLACE INTO numbers VALUES (1, -2147483648, 2147483647, 4294967295, "
        "0, -9223372036854775808, 9223372036854775807, NULL, 9)",
        1
    );
    failures += expect_replace_ok(database, "REPLACE numbers (id, i, nn) VALUES (1, -3, -4)", 1);
    failures += expect_replace_ok(database, "REPLACE INTO numbers (id, nn) VALUES (3, +10)", 1);
    failures +=
        expect_replace_ok(database, "REPLACE INTO numbers (id, nn) VALUES (4, 11), (5, 12)", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_two,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replace row count function",
        }
    );
    failures +=
        expect_replace_ok(database, "REPLACE INTO numbers (id, i, nn) VALUES (6, TRUE, FALSE)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID(), ROW_COUNT()",
            .values = last_insert_and_row_count,
            .column_count = 2U,
            .row_count = 1U,
            .context = "replace leaves last insert id",
        }
    );

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before_replace,
            "replace leaves catalog generation"
        );
    }
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_replace,
            "replace leaves SQLite schema generation"
        );
    }

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT i, ii, iu, integeru, b, bu, n, nn FROM numbers WHERE nn = 9",
            .values = integer_family_values,
            .column_count = replace_integer_family_column_count,
            .row_count = 1U,
            .context = "replace integer family values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, nn FROM numbers ORDER BY nn",
            .values = ordered_rows,
            .column_count = 3U,
            .row_count = replace_success_row_count,
            .context = "replace values rows",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read replace schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "numbers", &table),
        MYLITE_OK,
        "read replace table"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_physical_row_count(sqlite, table.physical_name, &physical_rows);
    }
    failures += expect_int(physical_rows, replace_success_row_count, "physical rows after replace");

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "replace preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, nn FROM numbers WHERE nn = 9",
            .values = persisted_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "persisted replace row",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO numbers (id, nn) VALUE (8, 80)", 1);
    failures += expect_replace_ok(
        database,
        "REPLACE INTO numbers (id, nn) VALUES ROW(9, 90), ROW(10, 100)",
        2
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM numbers WHERE id >= 8 ORDER BY id",
            .values = value_row_syntax_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "replace value row syntax rows",
        }
    );

    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        expect_replace_ok(database, "REPLACE INTO renamed_numbers (id, nn) VALUES (7, 77)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM renamed_numbers WHERE id = 7",
            .values = renamed_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "replace after rename",
        }
    );

    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "REPLACE INTO renamed_numbers (id, nn) VALUES (8, 88)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_values_schema_resolution_and_diagnostics(void) {
    static const char *const qualified_values[] = {"1", "2"};
    static const char *const scalar_expression_values[] = {"1", "1", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_schema(database, "app");
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO missing_schema.numbers (id, nn) VALUES (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE app.qualified_numbers (id INT NOT NULL, nn INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_replace_ok(database, "REPLACE INTO app.qualified_numbers VALUES (1, 2)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM app.qualified_numbers",
            .values = qualified_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "schema-qualified replace",
        }
    );

    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(database, "numbers");

    failures += execute_error(
        database,
        "REPLACE INTO missing_table VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO _mylite_reserved VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (missing, nn) VALUES (1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, id, nn) VALUES (1, 2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_column_specified_twice,
            .sqlstate = "42000",
            .message_part = "Column 'id' specified twice",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (1, 2, 3)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'nn' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (NULL, 1)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'id' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, i, nn) VALUES (1, 2147483648, 1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, iu, nn) VALUES (1, -1, 1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'iu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, bu, nn) VALUES (1, 9223372036854775808, 1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'bu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers VALUE (1)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers VALUES ROW(1)",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers VALUE ROW(1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers VALUES ROW(1), (2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers VALUES (1), ROW(2)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE LOW_PRIORITY DELAYED INTO numbers VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers SELECT 1",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (app.id) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (?, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO numbers (id, nn) VALUES (ABS(1), 1)", 1);
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES ((SELECT 1), 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES ('abc', 1)",
        (struct expected_sql_error){
            .code = mysql_error_truncated_wrong_value,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value: 'abc' for column 'id' at row 1",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO numbers (id, nn) VALUES (1 + 2, 1)", 1);
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (DEFAULT, 1)",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'id' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (1.5, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT supports only integer, boolean, string, hex, NULL, and "
                            "DEFAULT values",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (1e0, 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT supports only integer, boolean, string, hex, NULL, and "
                            "DEFAULT values",
        }
    );
    failures += expect_replace_ok(database, "REPLACE INTO numbers (id, nn) VALUES (0x1, 1)", 1);
    failures += execute_error(
        database,
        "REPLACE INTO numbers (id, nn) VALUES (b'1', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "INSERT supports only integer, boolean, string, hex, NULL, and "
                            "DEFAULT values",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM numbers ORDER BY id",
            .values = scalar_expression_values,
            .column_count = 1U,
            .row_count = 3U,
            .context = "scalar expression replace rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_values_independent_handles(void) {
    static const char *const first_values[] = {"1", "11"};
    static const char *const second_values[] = {"2", "22"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_numbers_table(first, "numbers");
    failures += create_numbers_table(second, "numbers");

    failures += expect_replace_ok(first, "REPLACE INTO numbers (id, nn) VALUES (1, 11)", 1);
    failures += expect_replace_ok(second, "REPLACE INTO numbers (id, nn) VALUES (2, 22)", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, nn FROM numbers",
            .values = first_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent replace rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, nn FROM numbers",
            .values = second_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent replace rows",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

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

static int create_numbers_table(mylite_db *database, const char *table_name) {
    char sql[sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s ("
        "id INT NOT NULL, "
        "i INT, "
        "ii INTEGER, "
        "iu INT UNSIGNED, "
        "integeru INTEGER UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT NULL, "
        "nn INT NOT NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create table SQL is too long for %s\n", table_name);
        return 1;
    }

    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

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

static int expect_replace_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "replace column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "replace row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), affected_rows, "replace affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "replace warning count");
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
        "%s/mylite_replace_values_lifecycle_%d_%s.mylite",
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

static int query_physical_row_count(sqlite3 *connection, const char *table_name, int *out_count) {
    char sql[sql_capacity];
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
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
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
