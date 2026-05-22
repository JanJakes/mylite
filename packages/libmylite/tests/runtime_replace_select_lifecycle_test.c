#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
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
    copied_query_column_count = 9,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_database = 1049,
    mysql_error_unknown_column = 1054,
    mysql_error_no_tables_used = 1096,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_column_specified_twice = 1110,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_data_out_of_range = 1264,
    mysql_error_field_no_default = 1364,
    mysql_error_row_is_referenced = 1451,
    mysql_error_no_referenced_row = 1452,
    mysql_error_bad_null = 1048,
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

static int test_replace_select_success_persistence_and_visibility(void);
static int test_replace_select_row_scalar_sources(void);
static int test_replace_select_keyed_targets(void);
static int test_replace_select_schema_resolution_and_diagnostics(void);
static int test_replace_select_independent_handles(void);
static int seed_schema(mylite_db *database, const char *name);
static int create_source_table(mylite_db *database, const char *table_name);
static int create_target_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
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

    failures += test_replace_select_success_persistence_and_visibility();
    failures += test_replace_select_row_scalar_sources();
    failures += test_replace_select_keyed_targets();
    failures += test_replace_select_schema_resolution_and_diagnostics();
    failures += test_replace_select_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_replace_select_success_persistence_and_visibility(void) {
    static const char *const copied_rows[] = {
        "1",
        "-2147483648",
        "2147483647",
        "4294967295",
        "-9223372036854775808",
        "9223372036854775807",
        NULL,
        "11",
        "101",
        "2",
        "2147483647",
        "-1",
        "0",
        "9223372036854775807",
        "0",
        "20",
        "12",
        "102",
        "3",
        NULL,
        NULL,
        "42",
        NULL,
        "42",
        NULL,
        "13",
        "44",
    };
    static const char *const visible_copy_rows[] = {
        "1",
        "-2147483648",
        "11",
        "1",
        "-2147483648",
        "11",
        "2",
        "2147483647",
        "12",
    };
    static const char *const row_count_one[] = {"1"};
    static const char *const persisted_rows[] = {"3", "13", "44"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation_before_replace_select = 0U;
    uint64_t sqlite_generation_before_replace_select = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
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
    failures += create_source_table(database, "src");
    failures += create_target_table(database, "dst");

    failures += expect_dml_ok(
        database,
        "INSERT INTO src(id, i, ii, iu, b, bu, n, nn, hidden) VALUES "
        "(1, -2147483648, 2147483647, 4294967295, -9223372036854775808, "
        "9223372036854775807, NULL, 11, 101), "
        "(2, 2147483647, -1, 0, 9223372036854775807, 0, 20, 12, 102), "
        "(3, NULL, NULL, 42, NULL, 42, NULL, 13, 103)",
        3
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        catalog_generation_before_replace_select = catalog->generation;
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        sqlite_generation_before_replace_select = session->sqlite_schema_generation;
    }
    failures += expect_dml_ok(
        database,
        "REPLACE INTO dst(id, i, ii, iu, b, bu, n, nn, hidden) "
        "SELECT id, i, ii, iu, b, bu, n, nn, hidden "
        "FROM src WHERE id >= 1 ORDER BY id LIMIT 2",
        2
    );
    catalog = mylite_connection_catalog_for_test(database);
    if (catalog != NULL) {
        failures += expect_uint64(
            catalog->generation,
            catalog_generation_before_replace_select,
            "replace select leaves catalog generation unchanged"
        );
    }
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_generation_before_replace_select,
            "replace select leaves SQLite schema generation unchanged"
        );
    }
    failures += expect_dml_ok(
        database,
        "REPLACE dst(id, i, ii, iu, b, bu, n, nn) "
        "SELECT id, i, ii, iu, b, bu, n, nn FROM src WHERE id = 3",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = row_count_one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replace select row count function",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, ii, iu, b, bu, n, nn, hidden FROM dst ORDER BY id",
            .values = copied_rows,
            .column_count = copied_query_column_count,
            .row_count = 3U,
            .context = "replace select copied rows",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE visible_copy (id INT NOT NULL, i INT, ii INTEGER, iu INT UNSIGNED, "
        "b BIGINT, bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_dml_ok(
        database,
        "REPLACE INTO visible_copy SELECT * FROM src ORDER BY id LIMIT 2",
        2
    );
    failures += expect_dml_ok(database, "REPLACE INTO visible_copy SELECT * FROM src LIMIT 0", 0);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO visible_copy SELECT * FROM visible_copy WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, nn FROM visible_copy ORDER BY id",
            .values = visible_copy_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "replace select visible star and no-key repeat rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "replace select preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn, hidden FROM dst WHERE id = 3",
            .values = persisted_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "persisted replace select row",
        }
    );
    failures += execute_ok(database, "RENAME TABLE dst TO renamed_dst", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_dml_ok(
        database,
        "REPLACE INTO renamed_dst(id, nn) SELECT id, nn FROM src WHERE id = 1",
        1
    );
    failures += execute_ok(database, "DROP TABLE renamed_dst", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "REPLACE INTO renamed_dst(id, nn) SELECT id, nn FROM src WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_dst' doesn't exist",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_select_row_scalar_sources(void) {
    static const char *const no_key_rows[] = {
        "1",
        "7",
        "10",
        "1",
        "20",
        "30",
    };
    static const char *const filter_rows[] = {
        "1",
        "10",
        "2",
        "20",
    };
    static const char *const zero_rows[] = {"0"};
    static const char *const pk_rows[] = {
        "1",
        "30",
        "2",
        "20",
    };
    static const char *const multi_unique_rows[] = {
        "1",
        "20",
        "300",
    };
    static const char *const nullable_unique_rows[] = {
        NULL,
        "10",
        NULL,
        "20",
    };
    static const char *const auto_increment_first_last_id[] = {"1"};
    static const char *const auto_increment_second_last_id[] = {"2"};
    static const char *const auto_increment_rows[] = {
        "2",
        "a",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "row_scalar") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open row scalar file");
    failures += seed_schema(database, "app");
    failures += expect_statement_ok(database, "USE app");

    failures += expect_statement_ok(
        database,
        "CREATE TABLE no_key(id INT, v INT NOT NULL DEFAULT 7, req INT NOT NULL)"
    );
    failures += expect_dml_ok(database, "REPLACE INTO no_key(id, req) SELECT 1, 10", 1);
    failures +=
        expect_dml_ok(database, "REPLACE INTO no_key(id, v, req) SELECT 1, 20, 30 FROM DUAL", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, req FROM no_key ORDER BY req",
            .values = no_key_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "row-scalar no-key replace select rows",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE filter_target(id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(
        database,
        "REPLACE INTO filter_target(id, v) SELECT 1, 10 FROM DUAL "
        "WHERE EXISTS(SELECT * FROM filter_target WHERE id = 99)",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM filter_target",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "false exists replace select source",
        }
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO filter_target(id, v) SELECT 1, 10 FROM DUAL "
        "WHERE NOT EXISTS(SELECT * FROM filter_target WHERE id = 99)",
        1
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO filter_target(id, v) SELECT 2, 20 FROM DUAL "
        "WHERE EXISTS(SELECT * FROM filter_target WHERE id = 1)",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM filter_target ORDER BY id",
            .values = filter_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "dual exists replace select rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE required_target(id INT PRIMARY KEY, must INT NOT NULL)"
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO required_target(id) SELECT 10 FROM DUAL "
        "WHERE EXISTS(SELECT * FROM required_target WHERE id = 99)",
        0
    );
    failures += execute_error(
        database,
        "REPLACE INTO required_target(id) SELECT 11 FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'must' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO required_target(id, must) SELECT 12, NULL FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'must' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO required_target SELECT * FROM DUAL",
        (struct expected_sql_error){
            .code = mysql_error_no_tables_used,
            .sqlstate = "HY000",
            .message_part = "No tables used",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE pk(id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO pk VALUES (1, 10)", 1);
    failures += expect_dml_ok(database, "REPLACE INTO pk SELECT 2, 20", 1);
    failures += expect_dml_ok(database, "REPLACE INTO pk SELECT 1, 30 FROM DUAL", 2);
    failures += expect_dml_ok(database, "REPLACE INTO pk SELECT 1, 30 FROM DUAL", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM pk ORDER BY id",
            .values = pk_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "primary-key row-scalar replace select rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE multi_unique(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures += expect_dml_ok(database, "INSERT INTO multi_unique VALUES (1, 10, 100)", 1);
    failures += expect_dml_ok(database, "INSERT INTO multi_unique VALUES (2, 20, 200)", 1);
    failures += expect_dml_ok(database, "REPLACE INTO multi_unique SELECT 1, 20, 300 FROM DUAL", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM multi_unique",
            .values = multi_unique_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multi-unique row-scalar replace select rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE nullable_unique(a INT UNIQUE, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO nullable_unique VALUES (NULL, 10)", 1);
    failures +=
        expect_dml_ok(database, "REPLACE INTO nullable_unique SELECT NULL, 20 FROM DUAL", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, v FROM nullable_unique ORDER BY v",
            .values = nullable_unique_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "nullable unique row-scalar replace select rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_inc(id INT AUTO_INCREMENT PRIMARY KEY, v VARCHAR(10) UNIQUE)"
    );
    failures += expect_dml_ok(database, "REPLACE INTO auto_inc(v) SELECT CONCAT('a') FROM DUAL", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = auto_increment_first_last_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row-scalar replace select generated first id",
        }
    );
    failures +=
        expect_dml_ok(database, "REPLACE INTO auto_inc(id, v) SELECT NULL, 'a' FROM DUAL", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = auto_increment_second_last_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "row-scalar replace select generated replacement id",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_inc",
            .values = auto_increment_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "row-scalar replace select auto-increment rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_select_keyed_targets(void) {
    static const char *const pk_rows[] = {"1", "20", "2", "30"};
    static const char *const same_table_rows[] = {"1", "10", "2", "20"};
    static const char *const unique_rows[] = {"1", "20", "200", "2", "30", "300"};
    static const char *const multi_unique_rows[] = {"1", "20", "300"};
    static const char *const composite_unique_rows[] = {
        "1",
        "1",
        "100",
        "1",
        "2",
        "20",
        "2",
        "2",
        "200",
    };
    static const char *const nullable_unique_rows[] = {
        NULL,
        "10",
        NULL,
        "20",
    };
    static const char *const prefix_unique_rows[] = {
        "abczzz",
        "20",
        "xyz000",
        "30",
    };
    static const char *const auto_increment_last_id[] = {"3"};
    static const char *const auto_increment_next_last_id[] = {"8"};
    static const char *const auto_increment_rows[] = {
        "1",
        "10",
        "3",
        "20",
        "7",
        "30",
    };
    static const char *const auto_increment_rows_after_next[] = {
        "1",
        "10",
        "3",
        "20",
        "7",
        "30",
        "8",
        "40",
    };
    static const char *const parent_rows[] = {"1", "10"};
    static const char *const child_rows[] = {"1", "1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "keyed_targets") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open keyed file");
    failures += seed_schema(database, "app");
    failures += expect_statement_ok(database, "USE app");

    failures += expect_statement_ok(database, "CREATE TABLE pk(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE pk_src(id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO pk VALUES (1, 10)", 1);
    failures += expect_dml_ok(database, "INSERT INTO pk_src VALUES (1, 20), (2, 30)", 2);
    failures += expect_dml_ok(database, "REPLACE INTO pk SELECT id, v FROM pk_src ORDER BY id", 3);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM pk ORDER BY id",
            .values = pk_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "primary key replace select rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE pk_same(id INT PRIMARY KEY, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO pk_same VALUES (1, 10), (2, 20)", 2);
    failures +=
        expect_dml_ok(database, "REPLACE INTO pk_same SELECT id, v FROM pk_same ORDER BY id", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM pk_same ORDER BY id",
            .values = same_table_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "same table exact replace select rows",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE unique_target(a INT UNIQUE, b INT, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE unique_source(a INT, b INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO unique_target VALUES (1, 10, 100)", 1);
    failures +=
        expect_dml_ok(database, "INSERT INTO unique_source VALUES (1, 20, 200), (2, 30, 300)", 2);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO unique_target SELECT a, b, v FROM unique_source ORDER BY a",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM unique_target ORDER BY a",
            .values = unique_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "single unique replace select rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE multi_unique(a INT UNIQUE, b INT UNIQUE, v INT)"
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE multi_unique_source(a INT, b INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO multi_unique VALUES (1, 10, 100)", 1);
    failures += expect_dml_ok(database, "INSERT INTO multi_unique VALUES (2, 20, 200)", 1);
    failures += expect_dml_ok(database, "INSERT INTO multi_unique_source VALUES (1, 20, 300)", 1);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO multi_unique SELECT a, b, v FROM multi_unique_source",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM multi_unique",
            .values = multi_unique_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multiple unique conflict replace select rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE composite_unique(a INT, b INT, v INT, UNIQUE KEY uq_ab(a, b))"
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE composite_unique_source(a INT, b INT, v INT)");
    failures +=
        expect_dml_ok(database, "INSERT INTO composite_unique VALUES (1, 1, 10), (1, 2, 20)", 2);
    failures += expect_dml_ok(
        database,
        "INSERT INTO composite_unique_source VALUES (1, 1, 100), (2, 2, 200)",
        2
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO composite_unique SELECT a, b, v FROM composite_unique_source ORDER BY a",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, v FROM composite_unique ORDER BY a, b",
            .values = composite_unique_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "composite unique replace select rows",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE nullable_unique(a INT UNIQUE, v INT)");
    failures += expect_statement_ok(database, "CREATE TABLE nullable_unique_source(a INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO nullable_unique VALUES (NULL, 10)", 1);
    failures += expect_dml_ok(database, "INSERT INTO nullable_unique_source VALUES (NULL, 20)", 1);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO nullable_unique SELECT a, v FROM nullable_unique_source",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, v FROM nullable_unique ORDER BY v",
            .values = nullable_unique_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "nullable unique null replace select rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE prefix_unique(s VARCHAR(10), v INT, UNIQUE KEY uq_s(s(3)))"
    );
    failures +=
        expect_statement_ok(database, "CREATE TABLE prefix_unique_source(s VARCHAR(10), v INT)");
    failures += expect_dml_ok(database, "INSERT INTO prefix_unique VALUES ('abcdef', 10)", 1);
    failures +=
        expect_dml_ok(database, "INSERT INTO prefix_unique_source VALUES ('abczzz', 20)", 1);
    failures +=
        expect_dml_ok(database, "INSERT INTO prefix_unique_source VALUES ('xyz000', 30)", 1);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO prefix_unique SELECT s, v FROM prefix_unique_source ORDER BY s",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT s, v FROM prefix_unique ORDER BY s",
            .values = prefix_unique_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "prefix unique replace select rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE auto_inc(id INT AUTO_INCREMENT PRIMARY KEY, v INT UNIQUE)"
    );
    failures += expect_statement_ok(database, "CREATE TABLE auto_inc_source(id INT, v INT)");
    failures += expect_dml_ok(database, "INSERT INTO auto_inc(v) VALUES (10), (20)", 2);
    failures += expect_dml_ok(database, "INSERT INTO auto_inc_source VALUES (NULL, 20)", 1);
    failures += expect_dml_ok(database, "INSERT INTO auto_inc_source VALUES (7, 30)", 1);
    failures += expect_dml_ok(
        database,
        "REPLACE INTO auto_inc(id, v) SELECT id, v FROM auto_inc_source ORDER BY v",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = auto_increment_last_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replace select first generated auto increment id",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_inc ORDER BY id",
            .values = auto_increment_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "replace select auto increment rows",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO auto_inc(v) VALUES (40)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = auto_increment_next_last_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replace select advances auto increment counter",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM auto_inc ORDER BY id",
            .values = auto_increment_rows_after_next,
            .column_count = 2U,
            .row_count = 4U,
            .context = "replace select auto increment next generated row",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE fk_parent(id INT PRIMARY KEY, v INT)");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE fk_child("
        "id INT PRIMARY KEY, "
        "parent_id INT, "
        "FOREIGN KEY(parent_id) REFERENCES fk_parent(id))"
    );
    failures += expect_statement_ok(database, "CREATE TABLE fk_parent_source(id INT, v INT)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE fk_child_source(id INT, parent_id INT)");
    failures += expect_dml_ok(database, "INSERT INTO fk_parent VALUES (1, 10)", 1);
    failures += expect_dml_ok(database, "INSERT INTO fk_child VALUES (1, 1)", 1);
    failures += expect_dml_ok(database, "INSERT INTO fk_parent_source VALUES (1, 20)", 1);
    failures += execute_error(
        database,
        "REPLACE INTO fk_parent SELECT id, v FROM fk_parent_source",
        (struct expected_sql_error){
            .code = mysql_error_row_is_referenced,
            .sqlstate = "23000",
            .message_part = "parent row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM fk_parent",
            .values = parent_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "parent replace select rolls back after FK error",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO fk_child_source VALUES (2, 999)", 1);
    failures += execute_error(
        database,
        "REPLACE INTO fk_child SELECT id, parent_id FROM fk_child_source",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "child row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, parent_id FROM fk_child",
            .values = child_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "child replace select rolls back after FK error",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "keyed replace select preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen keyed file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM pk ORDER BY id",
            .values = pk_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "persisted primary key replace select rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_select_schema_resolution_and_diagnostics(void) {
    static const char *const qualified_rows[] = {"1", "11"};
    static const char *const zero_rows[] = {"0"};
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
    failures += create_source_table(database, "app.src");
    failures += create_target_table(database, "app.dst");
    failures += expect_dml_ok(
        database,
        "INSERT INTO app.src(id, i, ii, iu, b, bu, n, nn, hidden) VALUES "
        "(1, 1, 1, 1, 1, 1, 11, 11, 11), "
        "(2, 2, 2, 2, 2147483648, 2, NULL, 12, 12)",
        2
    );
    failures += expect_dml_ok(
        database,
        "REPLACE INTO app.dst(id, nn) SELECT id, nn FROM app.src WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM app.dst",
            .values = qualified_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "qualified replace select",
        }
    );

    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO app.dst(id) SELECT id FROM src",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "REPLACE INTO missing_schema.dst(id) SELECT id FROM missing_source.src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO app.missing_dst(id) SELECT id FROM app.missing_src",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_dst' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO _mylite_reserved(id) SELECT id FROM app.src",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO app.dst(id) SELECT id FROM _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );

    failures += execute_ok(
        database,
        "CREATE TABLE required_target(id INT NOT NULL, must INT NOT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_dml_ok(
        database,
        "REPLACE INTO required_target(id) SELECT id FROM src WHERE id > 100",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM required_target",
            .values = zero_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "zero row source with omitted required column",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO required_target(id) SELECT id FROM src WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_field_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'must' doesn't have a default value",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO required_target(id, must) SELECT id, n FROM src WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'must' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id, i) SELECT id, b FROM src ORDER BY id",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 2",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id, i) SELECT id FROM src",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT id, i FROM src WHERE id > 100",
        (struct expected_sql_error){
            .code = mysql_error_column_count_mismatch,
            .sqlstate = "21S01",
            .message_part = "Column count doesn't match value count at row 1",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id, id) SELECT id, i FROM src",
        (struct expected_sql_error){
            .code = mysql_error_column_specified_twice,
            .sqlstate = "42000",
            .message_part = "Column 'id' specified twice",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(missing) SELECT id FROM src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT missing FROM src",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT id FROM src WHERE missing = 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT id FROM src ORDER BY missing LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );

    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT id + 1 FROM src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT '1' FROM src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor table columns",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT src.id FROM src JOIN src other",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REPLACE ... SELECT does not support joined SELECT",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(id) SELECT 1 UNION SELECT 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE DELAYED LOW_PRIORITY INTO dst(id) SELECT id FROM src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst PARTITION (p0) (id) SELECT id FROM src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst(app.id) SELECT id FROM src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "REPLACE INTO dst TABLE src",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_replace_select_independent_handles(void) {
    static const char *const first_values[] = {"1", "10"};
    static const char *const second_values[] = {"2", "20"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += seed_schema(first, "app");
    failures += seed_schema(second, "app");
    failures += execute_ok(first, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += create_source_table(first, "src");
    failures += create_source_table(second, "src");
    failures += execute_ok(first, "CREATE TABLE dst(id INT NOT NULL, nn INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "CREATE TABLE dst(id INT NOT NULL, nn INT NOT NULL)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_dml_ok(
        first,
        "INSERT INTO src(id, i, ii, iu, b, bu, n, nn, hidden) VALUES "
        "(1, 1, 1, 1, 1, 1, 1, 10, 1)",
        1
    );
    failures += expect_dml_ok(
        second,
        "INSERT INTO src(id, i, ii, iu, b, bu, n, nn, hidden) VALUES "
        "(2, 2, 2, 2, 2, 2, 2, 20, 2)",
        1
    );
    failures += expect_dml_ok(first, "REPLACE INTO dst SELECT id, nn FROM src", 1);
    failures += expect_dml_ok(second, "REPLACE INTO dst SELECT id, nn FROM src", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, nn FROM dst",
            .values = first_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first independent replace select rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, nn FROM dst",
            .values = second_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second independent replace select rows",
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

static int create_source_table(mylite_db *database, const char *table_name) {
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
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT NULL, "
        "nn INT NOT NULL, "
        "hidden INT DEFAULT 7)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create source table SQL is too long for %s\n", table_name);
        return 1;
    }

    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;
    written = snprintf(sql, sizeof(sql), "ALTER TABLE %s ALTER hidden SET INVISIBLE", table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "alter source table SQL is too long for %s\n", table_name);
        return failures + 1;
    }
    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);

    return failures;
}

static int create_target_table(mylite_db *database, const char *table_name) {
    char sql[sql_capacity];
    mylite_result *result = NULL;
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s ("
        "id INT NOT NULL, "
        "i INTEGER, "
        "ii INTEGER, "
        "iu INTEGER UNSIGNED, "
        "b BIGINT, "
        "bu BIGINT UNSIGNED, "
        "n INT NULL, "
        "nn INT NOT NULL DEFAULT 5, "
        "hidden INT DEFAULT 44)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "create target table SQL is too long for %s\n", table_name);
        return 1;
    }

    failures += execute_ok(database, sql, &result);
    mylite_result_free(result);
    result = NULL;
    written = snprintf(sql, sizeof(sql), "ALTER TABLE %s ALTER hidden SET INVISIBLE", table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "alter target table SQL is too long for %s\n", table_name);
        return failures + 1;
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

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), affected_rows, "DML affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "DML warning count");
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
        "%s/mylite_replace_select_lifecycle_%d_%s.mylite",
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text [%s], got [%s]\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle
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
