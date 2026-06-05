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
    mysql_error_duplicate_key = 1062,
    mysql_error_incorrect_column_specifier = 1063,
    mysql_error_parse = 1064,
    mysql_error_wrong_auto_key = 1075,
    mysql_error_table_does_not_exist = 1146,
    show_columns_column_count = 6,
    show_index_field_count = 15,
    show_table_status_column_count = 18,
    shadowed_final_row_count = 7,
    related_file_suffix_capacity = 16,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_temporary_auto_increment_metadata_dml_and_persistence(void);
static int test_temporary_auto_increment_like_update_and_transaction(void);
static int test_temporary_auto_increment_independent_handles(void);
static int test_temporary_auto_increment_diagnostics_and_boundaries(void);
static int seed_app_schema(mylite_db *database);
static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
);
static int expect_error(mylite_db *database, const char *sql, int expected_code);
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

    failures += test_temporary_auto_increment_metadata_dml_and_persistence();
    failures += test_temporary_auto_increment_like_update_and_transaction();
    failures += test_temporary_auto_increment_independent_handles();
    failures += test_temporary_auto_increment_diagnostics_and_boundaries();

    return failures == 0 ? 0 : 1;
}

static int test_temporary_auto_increment_metadata_dml_and_persistence(void) {
    static const char *const columns[] = {
        "id",
        "int",
        "NO",
        "PRI",
        NULL,
        "auto_increment",
        "v",
        "int",
        "YES",
        "",
        NULL,
        NULL,
    };
    static const char *const primary_index[] = {
        "shadowed",
        "0",
        "PRIMARY",
        "1",
        "id",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const persistent_auto_increment[] = {"20"};
    static const char *const empty_count[] = {"0"};
    static const char *const table_pk_state[] = {"1", "0", "1"};
    static const char *const table_pk_row[] = {"1", "10"};
    static const char *const unique_index[] = {
        "unique_ai",
        "0",
        "id_uq",
        "1",
        "id",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const unique_row[] = {"1", "20"};
    static const char *const secondary_index[] = {
        "key_ai",
        "1",
        "id_key",
        "1",
        "id",
        "A",
        "0",
        NULL,
        NULL,
        "",
        "BTREE",
        "",
        "",
        "YES",
        NULL,
    };
    static const char *const secondary_row[] = {"1", "30"};
    static const char *const first_state[] = {"1", "0", "7"};
    static const char *const generated_state[] = {"3", "0", "8"};
    static const char *const zero_state[] = {"1", "0", "8"};
    static const char *const final_state[] = {"1", "0", "21"};
    static const char *const select_first_state[] = {"1", "0", "1"};
    static const char *const select_second_state[] = {"1", "0", "2"};
    static const char *const select_third_state[] = {"1", "0", "3"};
    static const char *const select_rows[] = {"1", "40", "2", "50", "3", "60"};
    static const char *const final_rows[] = {
        "0",
        "50",
        "7",
        "70",
        "8",
        "80",
        "9",
        "90",
        "10",
        "100",
        "20",
        "200",
        "21",
        "210",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "metadata_dml") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open metadata file");
    failures += seed_app_schema(database);
    failures += expect_statement(
        database,
        "CREATE TABLE shadowed (id INT AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=20",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadowed(v) VALUES(200)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE shadowed (id INT AUTO_INCREMENT PRIMARY KEY, v INT) "
        "AUTO_INCREMENT=7",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM shadowed",
            .values = columns,
            .column_count = show_columns_column_count,
            .row_count = 2U,
            .context = "temporary auto increment SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM shadowed",
            .values = primary_index,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "temporary auto increment SHOW INDEX primary key",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE shadowed",
        0U,
        1U,
        "CREATE TEMPORARY TABLE `shadowed`",
        "SHOW CREATE TABLE renders temporary"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE shadowed",
        0U,
        1U,
        "AUTO_INCREMENT=7",
        "SHOW CREATE TABLE renders temporary counter"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT AUTO_INCREMENT FROM information_schema.tables "
                   "WHERE table_schema = 'app' AND table_name = 'shadowed'",
            .values = persistent_auto_increment,
            .column_count = 1U,
            .row_count = 1U,
            .context = "information schema sees persistent counter only",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE temp_only (id INT AUTO_INCREMENT PRIMARY KEY)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS LIKE 'temp_only'",
            .values = NULL,
            .column_count = show_table_status_column_count,
            .row_count = 0U,
            .context = "SHOW TABLE STATUS omits temporary auto increment table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.tables "
                   "WHERE table_schema = 'app' AND table_name = 'temp_only'",
            .values = empty_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "information schema omits temporary-only table",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE table_pk "
        "(id INT AUTO_INCREMENT, v INT, PRIMARY KEY(id)) AUTO_INCREMENT=0",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO table_pk(v) VALUES(10)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = table_pk_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary table-level primary key AUTO_INCREMENT=0 state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM table_pk",
            .values = table_pk_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary table-level primary key AUTO_INCREMENT=0 row",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE table_pk",
        0U,
        1U,
        "AUTO_INCREMENT=2",
        "SHOW CREATE TABLE renders AUTO_INCREMENT=0 normalized counter"
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE unique_ai "
        "(id INT AUTO_INCREMENT, v INT, UNIQUE KEY id_uq (id))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM unique_ai",
            .values = unique_index,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "temporary unique-key backed auto increment index",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO unique_ai(v) VALUES(20)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM unique_ai",
            .values = unique_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary unique-key backed auto increment row",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE key_ai "
        "(id INT AUTO_INCREMENT, v INT, KEY id_key (id))",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM key_ai",
            .values = secondary_index,
            .column_count = show_index_field_count,
            .row_count = 1U,
            .context = "temporary secondary-key backed auto increment index",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO key_ai(v) VALUES(30)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM key_ai",
            .values = secondary_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary secondary-key backed auto increment row",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadowed(v) VALUES(70)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = first_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first temporary generated state",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadowed VALUES(NULL,80), (0,90), (DEFAULT,100)",
        (struct expected_statement){3, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = generated_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multi-row temporary generated state",
        }
    );
    failures += expect_statement(
        database,
        "SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO'",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadowed VALUES(0,50)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = zero_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "NO_AUTO_VALUE_ON_ZERO stores explicit zero",
        }
    );
    failures += expect_statement(database, "SET sql_mode = ''", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "INSERT INTO shadowed SET id = 20, v = 200",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO shadowed SET v = 210",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = final_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary counter advances after explicit high value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM shadowed ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = shadowed_final_row_count,
            .context = "temporary auto increment final rows",
        }
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE shadowed",
        0U,
        1U,
        "AUTO_INCREMENT=22",
        "SHOW CREATE TABLE renders advanced temporary counter"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT AUTO_INCREMENT FROM information_schema.tables "
                   "WHERE table_schema = 'app' AND table_name = 'shadowed'",
            .values = persistent_auto_increment,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary counter leaves persistent metadata unchanged",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE select_ai (id INT AUTO_INCREMENT PRIMARY KEY, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO select_ai(v) SELECT 40 FROM DUAL",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = select_first_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary INSERT SELECT omitted auto increment state",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO select_ai(id, v) SELECT NULL, 50 FROM DUAL",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = select_second_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary INSERT SELECT NULL auto increment state",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO select_ai(id, v) SELECT 0, 60 FROM DUAL",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = select_third_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "temporary INSERT SELECT zero auto increment state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM select_ai ORDER BY id",
            .values = select_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "temporary INSERT SELECT auto increment rows",
        }
    );
    failures += expect_int(
        read_file_at(path, 0, actual_preamble, sizeof(actual_preamble)),
        0,
        "read metadata preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "temporary auto increment preserves preamble"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_auto_increment_like_update_and_transaction(void) {
    static const char *const one_row[] = {"1", "1"};
    static const char *const persistent_clone_row[] = {"1", "2"};
    static const char *const temp_from_temp_row[] = {"1", "3"};
    static const char *const updated_rows[] = {"-1", "10", "6", "20", "7", "30"};
    static const char *const transaction_last_insert[] = {"1"};
    static const char *const transaction_empty_count[] = {"0"};
    static const char *const transaction_row[] = {"2", "20"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "like_update_tx") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open like/update file");
    failures += seed_app_schema(database);
    failures += expect_statement(
        database,
        "CREATE TABLE src (id INT AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=7",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO src(v) VALUES(70)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tmp_like LIKE src",
        (struct expected_statement){0, 0U}
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE tmp_like",
        0U,
        1U,
        "CREATE TEMPORARY TABLE `tmp_like`",
        "temporary LIKE renders temporary"
    );
    failures += expect_query_contains(
        database,
        "SHOW CREATE TABLE tmp_like",
        0U,
        1U,
        "`id` int NOT NULL AUTO_INCREMENT",
        "temporary LIKE clones auto increment column"
    );
    failures += expect_statement(
        database,
        "INSERT INTO tmp_like(v) VALUES(1)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM tmp_like",
            .values = one_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary LIKE resets counter",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tmp_src (id INT AUTO_INCREMENT PRIMARY KEY, v INT) "
        "AUTO_INCREMENT=9",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO tmp_src(v) VALUES(90)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "CREATE TABLE persistent_from_tmp LIKE tmp_src",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO persistent_from_tmp(v) VALUES(2)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM persistent_from_tmp",
            .values = persistent_clone_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "persistent LIKE from temporary source resets counter",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE temp_from_temp LIKE tmp_src",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO temp_from_temp(v) VALUES(3)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM temp_from_temp",
            .values = temp_from_temp_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary LIKE from temporary source resets counter",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE upd (id INT AUTO_INCREMENT PRIMARY KEY, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO upd(v) VALUES(10)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "UPDATE upd SET id = 5 WHERE id = 1",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO upd(v) VALUES(20)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "UPDATE upd SET id = -1 WHERE id = 5",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO upd(v) VALUES(30)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM upd ORDER BY id",
            .values = updated_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "temporary UPDATE advances counter and negative does not lower it",
        }
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tx (id INT AUTO_INCREMENT PRIMARY KEY, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(database, "START TRANSACTION", (struct expected_statement){0, 0U});
    failures += expect_statement(
        database,
        "INSERT INTO tx(v) VALUES(10)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(database, "ROLLBACK", (struct expected_statement){0, 0U});
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = transaction_last_insert,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary rollback preserves LAST_INSERT_ID",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM tx",
            .values = transaction_empty_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary rollback removes row",
        }
    );
    failures += expect_statement(
        database,
        "INSERT INTO tx(v) VALUES(20)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM tx",
            .values = transaction_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "temporary rollback does not reset counter",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_temporary_auto_increment_independent_handles(void) {
    static const char *const first_row[] = {"1", "10"};
    static const char *const second_row[] = {"1", "20"};
    char path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_db *reopened = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "independent") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &first), MYLITE_OK, "open first handle");
    failures += seed_app_schema(first);
    failures += expect_int(mylite_open(path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement(second, "USE app", (struct expected_statement){0, 0U});
    failures += expect_statement(
        first,
        "CREATE TEMPORARY TABLE temp_ai (id INT AUTO_INCREMENT PRIMARY KEY, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        second,
        "CREATE TEMPORARY TABLE temp_ai (id INT AUTO_INCREMENT PRIMARY KEY, v INT)",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        first,
        "INSERT INTO temp_ai(v) VALUES(10)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_statement(
        second,
        "INSERT INTO temp_ai(v) VALUES(20)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM temp_ai",
            .values = first_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first handle temporary counter",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM temp_ai",
            .values = second_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second handle temporary counter",
        }
    );

    mylite_close(first);
    mylite_close(second);
    failures += expect_int(mylite_open(path, &reopened), MYLITE_OK, "reopen independent file");
    failures += expect_statement(reopened, "USE app", (struct expected_statement){0, 0U});
    failures += expect_error(reopened, "SELECT * FROM temp_ai", mysql_error_table_does_not_exist);

    mylite_close(reopened);
    remove_related_files(path);
    return failures;
}

static int test_temporary_auto_increment_diagnostics_and_boundaries(void) {
    static const char *const boundary_row[] = {"255"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += seed_app_schema(database);
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE no_key (id INT AUTO_INCREMENT, v INT)",
        mysql_error_wrong_auto_key
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE varchar_auto (id VARCHAR(3) AUTO_INCREMENT PRIMARY KEY)",
        mysql_error_incorrect_column_specifier
    );
    failures += expect_error(
        database,
        "CREATE TEMPORARY TABLE negative_option "
        "(id INT AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT=-1",
        mysql_error_parse
    );
    failures += expect_statement(
        database,
        "CREATE TEMPORARY TABLE tiny_t "
        "(id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=255",
        (struct expected_statement){0, 0U}
    );
    failures += expect_statement(
        database,
        "INSERT INTO tiny_t(v) VALUES(1)",
        (struct expected_statement){1, 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM tiny_t",
            .values = boundary_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary tinyint unsigned maximum row",
        }
    );
    failures +=
        expect_error(database, "INSERT INTO tiny_t(v) VALUES(2)", mysql_error_duplicate_key);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_app_schema(mylite_db *database) {
    int failures = 0;

    failures +=
        expect_statement(database, "CREATE DATABASE app", (struct expected_statement){1, 0U});
    failures += expect_statement(database, "USE app", (struct expected_statement){0, 0U});
    return failures;
}

static int expect_statement(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, "statement columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "statement rows");
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                size_t index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values == NULL ? NULL : query.values[index],
                    query.context
                );
            }
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", query.sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(
    mylite_db *database,
    const char *sql,
    size_t row,
    size_t column,
    const char *needle,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, context);
    if (rc == MYLITE_OK) {
        const char *value = mylite_result_value_text(result, row, column);

        failures += expect_contains(value, needle, context);
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, int expected_code) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected_code, sql);
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
        if (actual == NULL) {
            return 0;
        }
        fprintf(stderr, "%s: expected NULL, got '%s'\n", context, actual);
        return 1;
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_temporary_auto_increment_%d_%s.mylite",
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
    remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : -1;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected '%s', got '%s'\n",
        context,
        expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected '%s' to contain '%s'\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle
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
    fprintf(stderr, "%s: byte comparison failed\n", context);
    return 1;
}
