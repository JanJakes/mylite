#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    show_columns_field_count = 6,
    binary_family_column_count = 9,
    information_schema_binary_projection_count = 9,
    binary_column_blob = 5,
    binary_column_mediumblob = 6,
    binary_column_longblob = 7,
    binary_column_not_null_varbinary = 8,
    mysql_error_parse = 1064,
    mysql_error_column_length_too_big = 1074,
    mysql_error_row_size_too_large = 1118,
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_data_too_long = 1406,
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

struct expected_bytes {
    const unsigned char *bytes;
    size_t size;
    bool is_null;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_binary_success_persistence_and_introspection(void);
static int test_binary_diagnostics(void);
static int test_binary_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_binary_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
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

    failures += test_binary_success_persistence_and_introspection();
    failures += test_binary_diagnostics();
    failures += test_binary_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_binary_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",          "NO",  "", NULL, "", "b",  "binary(3)", "YES", "", NULL, "",
        "vb", "varbinary(3)", "YES", "", NULL, "", "cb", "binary(2)", "YES", "", NULL, "",
        "tb", "tinyblob",     "YES", "", NULL, "", "bl", "blob",      "YES", "", NULL, "",
        "mb", "mediumblob",   "YES", "", NULL, "", "lb", "longblob",  "YES", "", NULL, "",
        "nn", "varbinary(2)", "NO",  "", NULL, "",
    };
    static const char *const show_create_rows[] = {
        "bin_family",
        "CREATE TABLE `bin_family` (\n"
        "  `id` int NOT NULL,\n"
        "  `b` binary(3) DEFAULT NULL,\n"
        "  `vb` varbinary(3) DEFAULT NULL,\n"
        "  `cb` binary(2) DEFAULT NULL,\n"
        "  `tb` tinyblob,\n"
        "  `bl` blob,\n"
        "  `mb` mediumblob,\n"
        "  `lb` longblob,\n"
        "  `nn` varbinary(2) NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "id", "int",          NULL,         NULL,         NULL, NULL, "int",        "NO",  NULL,
        "b",  "binary(3)",    "3",          "3",          NULL, NULL, "binary",     "YES", NULL,
        "vb", "varbinary(3)", "3",          "3",          NULL, NULL, "varbinary",  "YES", NULL,
        "cb", "binary(2)",    "2",          "2",          NULL, NULL, "binary",     "YES", NULL,
        "tb", "tinyblob",     "255",        "255",        NULL, NULL, "tinyblob",   "YES", NULL,
        "bl", "blob",         "65535",      "65535",      NULL, NULL, "blob",       "YES", NULL,
        "mb", "mediumblob",   "16777215",   "16777215",   NULL, NULL, "mediumblob", "YES", NULL,
        "lb", "longblob",     "4294967295", "4294967295", NULL, NULL, "longblob",   "YES", NULL,
        "nn", "varbinary(2)", "2",          "2",          NULL, NULL, "varbinary",  "NO",  NULL,
    };
    static const unsigned char b_initial[] = {0x41U, 0x00U, 0x00U};
    static const unsigned char vb_initial[] = {0x41U, 0x42U, 0x43U};
    static const unsigned char cb_initial[] = {0x41U, 0x00U};
    static const unsigned char bl_initial[] = {0x62U, 0x6cU, 0x6fU, 0x62U};
    static const unsigned char mb_initial[] = {0x00U, 0xffU};
    static const unsigned char nn_initial[] = {0x01U, 0x02U};
    static const unsigned char vb_updated[] = {0x5aU, 0x00U};
    static const unsigned char clone_b[] = {0x41U, 0x00U, 0x00U};
    static const unsigned char added_fixed[] = {0x00U, 0x00U};
    static const unsigned char no_backslash_value[] = {0x5cU, 0x30U};
    static const unsigned char replace_b[] = {0x41U, 0x00U};
    static const unsigned char replace_set_b[] = {0x42U, 0x00U};
    static const unsigned char select_padded[] = {0x51U, 0x00U, 0x00U};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_result *result = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open binary success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE bin_family (id INT NOT NULL, b BINARY(3), vb VARBINARY(3), "
        "cb CHAR(2) BYTE, tb TINYBLOB, bl BLOB, mb MEDIUMBLOB, lb LONGBLOB, "
        "nn VARBINARY(2) NOT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM bin_family",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = binary_family_column_count,
            .context = "binary SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE bin_family",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "binary SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, DATA_TYPE, "
                   "IS_NULLABLE, COLUMN_DEFAULT FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'bin_family' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_binary_projection_count,
            .row_count = binary_family_column_count,
            .context = "binary INFORMATION_SCHEMA.COLUMNS",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO bin_family VALUES "
        "(1, 'A', X'414243', 0x41, X'', 'blob', X'00ff', NULL, X'0102')",
        1
    );
    failures += execute_ok(
        database,
        "SELECT id, b, vb, cb, tb, bl, mb, lb, nn FROM bin_family ORDER BY id",
        &result
    );
    failures += expect_size(mylite_result_row_count(result), 1U, "binary selected row count");
    failures += expect_result_value(result, 0U, 0U, "1", "binary selected id");
    failures += expect_binary_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = b_initial, .size = sizeof(b_initial)},
        "BINARY padding"
    );
    failures += expect_binary_cell(
        result,
        0U,
        2U,
        (struct expected_bytes){.bytes = vb_initial, .size = sizeof(vb_initial)},
        "VARBINARY hex literal"
    );
    failures += expect_binary_cell(
        result,
        0U,
        3U,
        (struct expected_bytes){.bytes = cb_initial, .size = sizeof(cb_initial)},
        "CHAR BYTE alias"
    );
    failures += expect_binary_cell(
        result,
        0U,
        4U,
        (struct expected_bytes){.bytes = (const unsigned char *)"", .size = 0U},
        "empty TINYBLOB"
    );
    failures += expect_binary_cell(
        result,
        0U,
        binary_column_blob,
        (struct expected_bytes){.bytes = bl_initial, .size = sizeof(bl_initial)},
        "BLOB string literal"
    );
    failures += expect_binary_cell(
        result,
        0U,
        binary_column_mediumblob,
        (struct expected_bytes){.bytes = mb_initial, .size = sizeof(mb_initial)},
        "MEDIUMBLOB NUL byte"
    );
    failures += expect_binary_cell(
        result,
        0U,
        binary_column_longblob,
        (struct expected_bytes){.is_null = true},
        "LONGBLOB NULL"
    );
    failures += expect_binary_cell(
        result,
        0U,
        binary_column_not_null_varbinary,
        (struct expected_bytes){.bytes = nn_initial, .size = sizeof(nn_initial)},
        "NOT NULL VARBINARY"
    );
    failures += expect_true(
        mylite_result_value_bytes(NULL, 0U, 0U) == NULL &&
            mylite_result_value_size(result, binary_family_column_count, 0U) == 0U,
        "binary result byte API misuse"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(database, "UPDATE bin_family SET vb = 'Z\\0' WHERE id = 1", 1);
    failures += execute_ok(database, "SELECT vb FROM bin_family WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = vb_updated, .size = sizeof(vb_updated)},
        "UPDATE binary string with escaped NUL"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(
        database,
        "ALTER TABLE bin_family ADD COLUMN added_fixed BINARY(2) NOT NULL"
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE bin_family ADD COLUMN added_var VARBINARY(2) NOT NULL"
    );
    failures +=
        expect_statement_ok(database, "ALTER TABLE bin_family ADD COLUMN added_blob BLOB NOT NULL");
    failures += execute_ok(
        database,
        "SELECT added_fixed, added_var, added_blob FROM bin_family WHERE id = 1",
        &result
    );
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = added_fixed, .size = sizeof(added_fixed)},
        "ALTER ADD BINARY NOT NULL implicit backfill"
    );
    failures += expect_binary_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = (const unsigned char *)"", .size = 0U},
        "ALTER ADD VARBINARY NOT NULL implicit backfill"
    );
    failures += expect_binary_cell(
        result,
        0U,
        2U,
        (struct expected_bytes){.bytes = (const unsigned char *)"", .size = 0U},
        "ALTER ADD BLOB NOT NULL implicit backfill"
    );
    mylite_result_free(result);
    result = NULL;

    failures +=
        expect_statement_ok(database, "CREATE TABLE repl (id INT, b BINARY(2), v VARBINARY(3))");
    failures += expect_dml_ok(database, "REPLACE INTO repl VALUES (1, 'A', X'0102')", 1);
    failures += expect_dml_ok(database, "REPLACE INTO repl SET id = 2, b = 'B', v = X'03'", 1);
    failures += execute_ok(database, "SELECT b, v FROM repl ORDER BY id", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = replace_b, .size = sizeof(replace_b)},
        "REPLACE VALUES BINARY padding"
    );
    failures += expect_binary_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = nn_initial, .size = sizeof(nn_initial)},
        "REPLACE VALUES VARBINARY hex"
    );
    failures += expect_binary_cell(
        result,
        1U,
        0U,
        (struct expected_bytes){.bytes = replace_set_b, .size = sizeof(replace_set_b)},
        "REPLACE SET BINARY padding"
    );
    failures += expect_binary_cell(
        result,
        1U,
        1U,
        (struct expected_bytes){.bytes = (const unsigned char *)"\x03", .size = 1U},
        "REPLACE SET VARBINARY hex"
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_statement_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += expect_statement_ok(database, "CREATE TABLE escapes (id INT, v VARBINARY(2))");
    failures += expect_dml_ok(database, "INSERT INTO escapes VALUES (1, '\\0')", 1);
    failures += execute_ok(database, "SELECT v FROM escapes WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = no_backslash_value, .size = sizeof(no_backslash_value)},
        "NO_BACKSLASH_ESCAPES binary literal decoding"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_statement_ok(database, "SET SESSION sql_mode = DEFAULT");

    failures += expect_statement_ok(
        database,
        "CREATE TABLE copy_source (id INT, v VARBINARY(1), b BINARY(3))"
    );
    failures += expect_dml_ok(database, "INSERT INTO copy_source VALUES (1, X'51', X'515253')", 1);
    failures += expect_statement_ok(database, "CREATE TABLE copy_target (id INT, b BINARY(3))");
    failures += expect_dml_ok(database, "INSERT INTO copy_target SELECT id, v FROM copy_source", 1);
    failures += execute_ok(database, "SELECT b FROM copy_target WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = select_padded, .size = sizeof(select_padded)},
        "INSERT SELECT pads VARBINARY source for BINARY target"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_statement_ok(database, "CREATE TABLE replace_target (id INT, b BINARY(3))");
    failures +=
        expect_dml_ok(database, "REPLACE INTO replace_target SELECT id, v FROM copy_source", 1);
    failures += execute_ok(database, "SELECT b FROM replace_target WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = select_padded, .size = sizeof(select_padded)},
        "REPLACE SELECT pads VARBINARY source for BINARY target"
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        expect_statement_ok(database, "CREATE TABLE narrow_target (id INT, v VARBINARY(1))");
    failures += execute_error(
        database,
        "INSERT INTO narrow_target SELECT id, b FROM copy_source",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long",
        }
    );
    static const char *const no_rows[] = {"0"};
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM narrow_target",
            .values = no_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed binary INSERT SELECT leaves target empty",
        }
    );

    failures += expect_dml_ok(database, "CREATE TABLE clone LIKE bin_family", 0);
    failures += expect_dml_ok(
        database,
        "INSERT INTO clone (id, b, vb, cb, tb, bl, mb, lb, nn, added_fixed, added_var, "
        "added_blob) SELECT id, b, vb, cb, tb, bl, mb, lb, nn, added_fixed, added_var, "
        "added_blob FROM bin_family",
        1
    );
    failures += execute_ok(database, "SELECT b FROM clone WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = clone_b, .size = sizeof(clone_b)},
        "binary CREATE TABLE LIKE and INSERT SELECT copy"
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "binary file preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen binary file");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_ok(database, "SELECT vb FROM bin_family WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = vb_updated, .size = sizeof(vb_updated)},
        "binary reopen persistence"
    );
    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_binary_diagnostics(void) {
    static const unsigned char truncated_b[] = {0x01U, 0x02U, 0x03U};
    static const unsigned char implicit_nn[] = {0x00U, 0x00U};
    char path[test_path_capacity];
    mylite_result *result = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open binary diagnostics");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE too_wide (b BINARY(256))",
        (struct expected_sql_error){
            .code = mysql_error_column_length_too_big,
            .sqlstate = "42000",
            .message_part = "Column length too big",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE too_wide (v VARBINARY(65536))",
        (struct expected_sql_error){
            .code = mysql_error_column_length_too_big,
            .sqlstate = "42000",
            .message_part = "Column length too big",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE row_too_wide (v VARBINARY(65533))",
        (struct expected_sql_error){
            .code = mysql_error_row_size_too_large,
            .sqlstate = "42000",
            .message_part = "Row size too large",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_default (b BINARY(3) DEFAULT 'a')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE d (id INT NOT NULL, b BINARY(3), nn BINARY(2) NOT NULL)"
    );
    failures += execute_error(
        database,
        "INSERT INTO d VALUES (1, X'01020304', X'0102')",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO d VALUES (2, b'1010', X'0102')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "Binary string values support only string, hex, NULL, and DEFAULT values",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO d VALUES (1, X'01020304', NULL)",
        (struct expected_dml_result){.affected_rows = 1, .warning_count = 2U}
    );
    failures += execute_ok(database, "SELECT b, nn FROM d WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = truncated_b, .size = sizeof(truncated_b)},
        "INSERT IGNORE truncated BINARY"
    );
    failures += expect_binary_cell(
        result,
        0U,
        1U,
        (struct expected_bytes){.bytes = implicit_nn, .size = sizeof(implicit_nn)},
        "INSERT IGNORE implicit BINARY"
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE d SET b = X'01020304' WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long",
        }
    );
    failures += execute_error(
        database,
        "UPDATE d SET nn = NULL WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "cannot be null",
        }
    );
    failures += execute_error(
        database,
        "CREATE INDEX d_b ON d (b)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "Secondary indexes do not yet support this column type",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_binary_independent_handles(void) {
    static const unsigned char one_value[] = {0x01U};
    static const unsigned char two_value[] = {0x02U};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_result *result = NULL;
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent-first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent-second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first binary file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second binary file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT NOT NULL, b VARBINARY(1))");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT NOT NULL, b VARBINARY(1))");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, X'01')", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, X'02')", 1);
    failures += execute_ok(first, "SELECT b FROM t WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = one_value, .size = sizeof(one_value)},
        "first independent binary state"
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(second, "SELECT b FROM t WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = two_value, .size = sizeof(two_value)},
        "second independent binary state"
    );
    mylite_result_free(result);

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
        return 1;
    }
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
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), expected.warning_count, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    size_t value_count = query.column_count * query.row_count;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        size_t row = value_index / query.column_count;
        size_t column = value_index % query.column_count;

        failures +=
            expect_result_value(result, row, column, query.values[value_index], query.context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_binary_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
    const char *context
) {
    const unsigned char *actual = mylite_result_value_bytes(result, row, column);
    size_t actual_size = mylite_result_value_size(result, row, column);
    int failures = 0;

    if (expected.is_null) {
        failures += expect_true(actual == NULL, context);
        failures += expect_size(actual_size, 0U, context);
        return failures;
    }
    failures += expect_true(actual != NULL, context);
    failures += expect_size(actual_size, expected.size, context);
    if (actual != NULL && actual_size == expected.size) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }
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
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_binary_string_types_%d_%s.mylite",
        current_process_id(),
        name
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
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

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected condition to be true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected \"%s\", got \"%s\"\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected \"%s\" to contain \"%s\"\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (size == 0U && expected != NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && memcmp(actual, expected, size) == 0) {
        return 0;
    }
    fprintf(stderr, "%s: byte sequence mismatch\n", context);
    return 1;
}
