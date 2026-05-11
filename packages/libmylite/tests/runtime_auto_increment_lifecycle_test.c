#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    related_file_suffix_capacity = 8,
    show_table_status_query_capacity = 128,
    show_columns_field_count = 6,
    show_table_status_field_count = 18,
    show_table_status_name_column = 0,
    show_table_status_auto_increment_column = 10,
    generated_rows_after_explicit_count = 6,
    mysql_error_duplicate_key = 1062,
    mysql_error_incorrect_column_specifier = 1063,
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_wrong_auto_key = 1075,
    mysql_error_primary_key_part_null = 1171,
    mysql_error_failed_read_auto_increment = 1467,
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

struct expected_statement {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_auto_increment_success_metadata_persistence_and_mutation(void);
static int test_auto_increment_type_families_and_diagnostics(void);
static int test_auto_increment_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_show_table_status_auto_increment(
    mylite_db *database,
    const char *table_name,
    const char *expected,
    const char *context
);
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
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_auto_increment_success_metadata_persistence_and_mutation();
    failures += test_auto_increment_type_families_and_diagnostics();
    failures += test_auto_increment_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_auto_increment_success_metadata_persistence_and_mutation(void) {
    static const char *const show_columns_rows[] = {
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
        "",
    };
    static const char *const show_create_initial[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const last_insert_id_zero[] = {"0"};
    static const char *const first_insert_state[] = {"1", "0", "1"};
    static const char *const generated_rows[] = {
        "1",
        "10",
        "2",
        "20",
        "3",
        "30",
        "4",
        "40",
    };
    static const char *const generated_multi_state[] = {"3", "0", "2"};
    static const char *const explicit_state[] = {"1", "0", "2"};
    static const char *const final_insert_state[] = {"1", "0", "11"};
    static const char *const final_rows[] = {
        "1",
        "10",
        "2",
        "20",
        "3",
        "30",
        "4",
        "40",
        "10",
        "100",
        "11",
        "110",
    };
    static const char *const show_create_advanced[] = {
        "t",
        "CREATE TABLE `t` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const insert_set_rows[] = {"1", "10", "5", "50", "6", "60"};
    static const char *const option_show_create[] = {
        "opt",
        "CREATE TABLE `opt` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const option_rows[] = {"7", "70"};
    static const char *const like_show_create[] = {
        "like_opt",
        "CREATE TABLE `like_opt` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const one_row[] = {"1", "1"};
    static const char *const truncated_rows[] = {"1", "90"};
    static const char *const updated_auto_rows[] = {"5", "10", "6", "20"};
    static const char *const hidden_default_columns[] = {
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
        "",
    };
    static const char *const hidden_default_insert_state[] = {"1", "0"};
    static const char *const hidden_default_rows[] = {"7", "70"};
    static const char *const hidden_default_show_create[] = {
        "default_set",
        "CREATE TABLE `default_set` (\n"
        "  `id` int NOT NULL AUTO_INCREMENT,\n"
        "  `v` int DEFAULT NULL,\n"
        "  PRIMARY KEY (`id`)\n"
        ") ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    mylite_db *database = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM t",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "auto increment SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_initial,
            .column_count = 2U,
            .row_count = 1U,
            .context = "initial auto increment SHOW CREATE",
        }
    );
    failures += expect_show_table_status_auto_increment(database, "t", "1", "initial status");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT LAST_INSERT_ID()",
            .values = last_insert_id_zero,
            .column_count = 1U,
            .row_count = 1U,
            .context = "initial last insert id",
        }
    );

    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation = session->catalog_generation;
        sqlite_schema_generation = session->sqlite_schema_generation;
    }
    failures += expect_statement_result(
        database,
        "INSERT INTO t (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = first_insert_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "first generated id state",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO t VALUES (NULL,20), (0,30), (DEFAULT,40)",
        (struct expected_statement){.affected_rows = 3, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = generated_multi_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "multi-row generated id state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = generated_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "generated id rows",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO t VALUES (10,100)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = explicit_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "explicit auto increment insert state",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO t (v) VALUES (110)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID()",
            .values = final_insert_state,
            .column_count = 3U,
            .row_count = 1U,
            .context = "generated id after explicit high value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = generated_rows_after_explicit_count,
            .context = "final generated and explicit rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE t",
            .values = show_create_advanced,
            .column_count = 2U,
            .row_count = 1U,
            .context = "advanced auto increment SHOW CREATE",
        }
    );
    failures += expect_show_table_status_auto_increment(database, "t", "12", "advanced status");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation,
            "auto increment inserts leave catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "auto increment inserts leave SQLite schema generation"
        );
    }

    failures += expect_statement_ok(
        database,
        "CREATE TABLE set_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO set_t SET v = 10",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO set_t SET id = 5, v = 50",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO set_t SET v = 60",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM set_t ORDER BY id",
            .values = insert_set_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "INSERT SET auto increment rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE opt (id INT AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=7"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE opt",
            .values = option_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "table option SHOW CREATE",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO opt (v) VALUES (70)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM opt",
            .values = option_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "table option generated row",
        }
    );
    failures += expect_show_table_status_auto_increment(database, "opt", "8", "option status");
    failures += expect_statement_ok(database, "CREATE TABLE like_opt LIKE opt");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE like_opt",
            .values = like_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE resets auto increment counter",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO like_opt (v) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM like_opt",
            .values = one_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "CREATE TABLE LIKE generated row",
        }
    );
    failures += expect_statement_result(
        database,
        "TRUNCATE TABLE opt",
        (struct expected_statement){.affected_rows = 0, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO opt (v) VALUES (90)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM opt",
            .values = truncated_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "TRUNCATE resets auto increment row",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE upd (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO upd (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "UPDATE upd SET id = 5 WHERE id = 1",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO upd (v) VALUES (20)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM upd ORDER BY id",
            .values = updated_auto_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "UPDATE advances auto increment counter",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE default_set (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(database, "ALTER TABLE default_set ALTER id SET DEFAULT 7");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM default_set",
            .values = hidden_default_columns,
            .column_count = show_columns_field_count,
            .row_count = 2U,
            .context = "ALTER SET DEFAULT auto increment hides metadata default",
        }
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO default_set (v) VALUES (70)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = hidden_default_insert_state,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ALTER SET DEFAULT auto increment insert state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM default_set",
            .values = hidden_default_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ALTER SET DEFAULT auto increment row",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE default_set",
            .values = hidden_default_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "ALTER SET DEFAULT auto increment SHOW CREATE",
        }
    );
    failures += expect_show_table_status_auto_increment(
        database,
        "default_set",
        "8",
        "hidden default status"
    );
    failures += execute_error(
        database,
        "INSERT INTO default_set (v) VALUES (80)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '7'",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "auto increment preserves MyLite preamble"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = generated_rows_after_explicit_count,
            .context = "reopened generated rows",
        }
    );
    failures += expect_show_table_status_auto_increment(database, "t", "12", "reopened status");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_auto_increment_type_families_and_diagnostics(void) {
    static const char *const one[] = {"1"};
    static const char *const negative_rows[] = {"-5", "1"};
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

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_int (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_integer (id INTEGER AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_bigint (id BIGINT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_int_unsigned (id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_integer_unsigned "
        "(id INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ai_bigint_unsigned (id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ai_int (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ai_integer (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ai_bigint (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ai_int_unsigned (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ai_integer_unsigned (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO ai_bigint_unsigned (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ai_int",
            .values = one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INT auto increment value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ai_integer",
            .values = one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INTEGER auto increment value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ai_bigint",
            .values = one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "BIGINT auto increment value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ai_int_unsigned",
            .values = one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INT UNSIGNED auto increment value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ai_integer_unsigned",
            .values = one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "INTEGER UNSIGNED auto increment value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM ai_bigint_unsigned",
            .values = one,
            .column_count = 1U,
            .row_count = 1U,
            .context = "BIGINT UNSIGNED auto increment value",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE neg (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO neg VALUES (-5, 1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO neg (v) VALUES (2)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM neg ORDER BY id",
            .values = negative_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "negative explicit value does not advance counter",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE tiny_t (id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT) "
        "AUTO_INCREMENT=255"
    );
    failures += expect_statement_result(
        database,
        "INSERT INTO tiny_t (v) VALUES (1)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );
    failures += execute_error(
        database,
        "INSERT INTO tiny_t (v) VALUES (2)",
        (struct expected_sql_error){
            .code = mysql_error_duplicate_key,
            .sqlstate = "23000",
            .message_part = "Duplicate entry '255'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE tiny_over (id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT) "
        "AUTO_INCREMENT=256"
    );
    failures += expect_show_table_status_auto_increment(
        database,
        "tiny_over",
        "256",
        "out of range initial status"
    );
    failures += execute_error(
        database,
        "INSERT INTO tiny_over (v) VALUES (1)",
        (struct expected_sql_error){
            .code = mysql_error_failed_read_auto_increment,
            .sqlstate = "HY000",
            .message_part = "Failed to read auto-increment value from storage engine",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE no_key (id INT AUTO_INCREMENT, v INT)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_auto_key,
            .sqlstate = "42000",
            .message_part = "auto column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE varchar_auto (id VARCHAR(3) AUTO_INCREMENT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_specifier,
            .sqlstate = "42000",
            .message_part = "Incorrect column specifier",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE default_auto (id INT AUTO_INCREMENT DEFAULT 7 PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nullable_auto (id INT NULL AUTO_INCREMENT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_primary_key_part_null,
            .sqlstate = "42000",
            .message_part = "PRIMARY KEY",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE two_auto (id INT AUTO_INCREMENT PRIMARY KEY, n INT AUTO_INCREMENT)",
        (struct expected_sql_error){
            .code = mysql_error_wrong_auto_key,
            .sqlstate = "42000",
            .message_part = "auto column",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE negative_option (id INT AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT=-1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "syntax",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE mixed_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += execute_error(
        database,
        "INSERT INTO mixed_t VALUES (1, 10), (NULL, 20)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "mixed explicit and generated",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_auto_increment_independent_handles(void) {
    static const char *const first_rows[] = {"1", "10"};
    static const char *const second_rows[] = {"5", "50"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures +=
        expect_statement_ok(first, "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT)");
    failures += expect_statement_result(
        first,
        "INSERT INTO t (v) VALUES (10)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );

    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(
        second,
        "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=5"
    );
    failures += expect_statement_result(
        second,
        "INSERT INTO t (v) VALUES (50)",
        (struct expected_statement){.affected_rows = 1, .warning_count = 0U}
    );

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = first_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "first handle auto increment rows",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id, v FROM t",
            .values = second_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second handle auto increment rows",
        }
    );
    failures += expect_show_table_status_auto_increment(first, "t", "2", "first handle status");
    failures += expect_show_table_status_auto_increment(second, "t", "6", "second handle status");

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
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
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_statement expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result != NULL) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values[(row * query.column_count) + column],
                    query.context
                );
            }
        }
        failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
        failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_table_status_auto_increment(
    mylite_db *database,
    const char *table_name,
    const char *expected,
    const char *context
) {
    char sql[show_table_status_query_capacity];
    mylite_result *result = NULL;
    int written = snprintf(sql, sizeof(sql), "SHOW TABLE STATUS LIKE '%s'", table_name);
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        (void)fprintf(stderr, "%s: failed to format SHOW TABLE STATUS query\n", context);
        return 1;
    }
    failures += execute_ok(database, sql, &result);
    if (result != NULL) {
        failures +=
            expect_size(mylite_result_column_count(result), show_table_status_field_count, context);
        failures += expect_size(mylite_result_row_count(result), 1U, context);
        failures +=
            expect_result_value(result, 0U, show_table_status_name_column, table_name, context);
        failures += expect_result_value(
            result,
            0U,
            show_table_status_auto_increment_column,
            expected,
            context
        );
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
    return expect_text_or_null(mylite_result_value_text(result, row, column), expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_auto_increment_lifecycle_%ld_%s.mylite",
        (long)current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        (void)fprintf(stderr, "failed to build test path\n");
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
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        (void)fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    if (bytes_read != size) {
        (void)fprintf(stderr, "%s: expected %zu bytes, got %zu\n", path, size, bytes_read);
        fclose(file);
        return 1;
    }
    fclose(file);
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
    (void)fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    (void)fprintf(
        stderr,
        "%s: expected %llu, got %llu\n",
        context,
        (unsigned long long)expected,
        (unsigned long long)actual
    );
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

static int expect_contains(const char *actual, const char *needle, const char *context) {
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
