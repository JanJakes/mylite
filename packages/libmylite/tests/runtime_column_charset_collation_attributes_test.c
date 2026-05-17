#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    test_path_suffix_capacity = 16,
    query_capacity = 512,
    attrs_column_count = 6,
    altered_attrs_column_count = 7,
    ascii_attrs_column_count = 5,
    ascii_attrs_length_projection_count = 3,
    ascii_result_column_count = 4,
    ascii_varchar_display_length = 10,
    ascii_char_display_length = 5,
    ascii_text_display_length = 65535,
    binary_attrs_column_count = 9,
    binary_attrs_projection_count = 7,
    binary_alter_projection_count = 8,
    mysql_error_parse = 1064,
    mysql_error_unknown_character_set = 1115,
    mysql_error_collation_not_valid_for_character_set = 1253,
    mysql_error_unknown_collation = 1273,
    mysql_collation_binary_id = 63,
    mysql_collation_ascii_general_ci_id = 11,
    mysql_collation_ascii_bin_id = 65,
    mysql_collation_utf8mb4_bin_id = 46,
    mysql_collation_utf8mb4_unicode_ci_id = 224,
    mysql_collation_utf8mb4_unicode_520_ci_id = 246,
    mysql_collation_utf8mb4_0900_ai_ci_id = 255,
    mysql_collation_utf8mb4_0900_bin_id = 309,
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

static int test_column_charset_collation_metadata_lifecycle(void);
static int test_binary_column_charset_collation_normalization(void);
static int test_ascii_column_charset_collation_metadata(void);
static int test_column_charset_collation_diagnostics(void);
static int expect_column_character_metadata(
    mylite_db *database,
    const char *table_name,
    const char *const *expected_values,
    size_t expected_row_count,
    const char *context
);
static int expect_result_metadata(mylite_db *database);
static int expect_ascii_result_metadata(mylite_db *database);
static int expect_binary_result_metadata(mylite_db *database);
static int expect_query_result(mylite_db *database, struct expected_query expected);
static int expect_show_create_contains(mylite_db *database, const char *sql, const char *needle);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_affected_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_column_charset_collation_metadata_lifecycle();
    failures += test_binary_column_charset_collation_normalization();
    failures += test_ascii_column_charset_collation_metadata();
    failures += test_column_charset_collation_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_column_charset_collation_metadata_lifecycle(void) {
    static const char *const attrs_metadata[] = {
        "id",
        NULL,
        NULL,
        "v",
        "utf8mb4",
        "utf8mb4_bin",
        "t",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "c",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "inherited",
        "utf8mb4",
        "utf8mb4_general_ci",
        "v0900",
        "utf8mb4",
        "utf8mb4_0900_bin",
    };
    static const char *const altered_metadata[] = {
        "id",        NULL,      NULL,
        "renamed",   "utf8mb4", "utf8mb4_0900_bin",
        "t",         "utf8mb4", "utf8mb4_0900_ai_ci",
        "c",         "utf8mb4", "utf8mb4_0900_bin",
        "inherited", "utf8mb4", "utf8mb4_general_ci",
        "v0900",     "utf8mb4", "utf8mb4_0900_bin",
        "added",     "utf8mb4", "utf8mb4_unicode_520_ci",
    };
    static const char *const ctas_metadata[] = {
        "v",
        "utf8mb4",
        "utf8mb4_bin",
        "t",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "c",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "inherited",
        "utf8mb4",
        "utf8mb4_general_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open lifecycle file");
    failures += execute_affected_ok(database, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE attrs("
        "id INT, "
        "v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
        "t TEXT CHARSET 'utf8mb4', "
        "c CHAR(2) COLLATE `utf8mb4_unicode_ci`, "
        "inherited VARCHAR(3), "
        "v0900 VARCHAR(10) COLLATE utf8mb4_0900_bin"
        ") COLLATE=utf8mb4_general_ci"
    );
    failures +=
        execute_affected_ok(database, "INSERT INTO attrs VALUES(1, 'a', 'b', 'c', 'd', 'e')", 1);
    failures += expect_column_character_metadata(
        database,
        "attrs",
        attrs_metadata,
        attrs_column_count,
        "created column charset metadata"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE attrs",
        "`v` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE attrs",
        "`t` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE attrs",
        "`inherited` varchar(3) COLLATE utf8mb4_general_ci"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE attrs",
        "`v0900` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin"
    );
    failures += expect_result_metadata(database);

    failures += execute_statement_ok(database, "CREATE TABLE like_attrs LIKE attrs");
    failures += expect_column_character_metadata(
        database,
        "like_attrs",
        attrs_metadata,
        attrs_column_count,
        "create table like column charset metadata"
    );
    failures += execute_affected_ok(
        database,
        "CREATE TABLE ctas_attrs AS SELECT v, t, c, inherited FROM attrs",
        1
    );
    failures += expect_column_character_metadata(
        database,
        "ctas_attrs",
        ctas_metadata,
        4U,
        "create table select column charset metadata"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE ctas_attrs",
        "`inherited` varchar(3) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE attrs ADD added VARCHAR(5) CHARACTER SET utf8mb4 "
        "COLLATE utf8mb4_unicode_520_ci"
    );
    failures += execute_affected_ok(
        database,
        "ALTER TABLE attrs MODIFY c CHAR(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin",
        1
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE attrs CHANGE v renamed VARCHAR(10) COLLATE utf8mb4_0900_bin"
    );
    failures += expect_column_character_metadata(
        database,
        "attrs",
        altered_metadata,
        altered_attrs_column_count,
        "altered column charset metadata"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE attrs",
        "`renamed` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE attrs",
        "`c` char(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin"
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "column charset file preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen lifecycle file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_column_character_metadata(
        database,
        "attrs",
        altered_metadata,
        altered_attrs_column_count,
        "reopened column charset metadata"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_binary_column_charset_collation_normalization(void) {
    static const char *const binary_column_values[] = {
        "renamed", "varbinary",  "varbinary(10)", NULL, NULL, "10",         "10",
        "c",       "binary",     "binary(4)",     NULL, NULL, "4",          "4",
        "t",       "tinyblob",   "tinyblob",      NULL, NULL, "255",        "255",
        "b",       "blob",       "blob",          NULL, NULL, "65535",      "65535",
        "m",       "mediumblob", "mediumblob",    NULL, NULL, "16777215",   "16777215",
        "l",       "longblob",   "longblob",      NULL, NULL, "4294967295", "4294967295",
        "z",       "varbinary",  "varbinary(0)",  NULL, NULL, "0",          "0",
        "cz",      "binary",     "binary(0)",     NULL, NULL, "0",          "0",
        "added",   "varbinary",  "varbinary(2)",  NULL, NULL, "2",          "2",
    };
    static const char *const updated_hex_values[] = {
        "7A",
        "62000000",
        "74696E79",
        "626C6F62",
        "6D656469756D",
        "6C6F6E67",
        "",
        "",
        "7879",
    };
    static const char *const altered_hex_values[] = {
        "1",
        "6162",
        "2",
        "78790000",
        "4",
        "68656C6C6F",
        "5",
        NULL,
        "2",
        "",
        "0",
        "7A000000",
        "4",
        "",
        "0",
        NULL,
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "binary") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open binary charset file");
    failures += execute_affected_ok(database, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE binary_attrs("
        "v VARCHAR(10) CHARACTER SET binary, "
        "c CHAR(3) COLLATE binary, "
        "t TINYTEXT CHARSET binary, "
        "b TEXT COLLATE binary, "
        "m MEDIUMTEXT CHARACTER SET binary, "
        "l LONGTEXT COLLATE binary, "
        "z VARCHAR(0) CHARACTER SET binary, "
        "cz CHAR(0) COLLATE binary)"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE binary_attrs",
        "`v` varbinary(10)"
    );
    failures +=
        expect_show_create_contains(database, "SHOW CREATE TABLE binary_attrs", "`c` binary(3)");
    failures +=
        expect_show_create_contains(database, "SHOW CREATE TABLE binary_attrs", "`t` tinyblob");
    failures += expect_show_create_contains(database, "SHOW CREATE TABLE binary_attrs", "`b` blob");
    failures +=
        expect_show_create_contains(database, "SHOW CREATE TABLE binary_attrs", "`m` mediumblob");
    failures +=
        expect_show_create_contains(database, "SHOW CREATE TABLE binary_attrs", "`l` longblob");
    failures += execute_statement_ok(
        database,
        "ALTER TABLE binary_attrs ADD added VARCHAR(2) COLLATE binary"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE binary_attrs MODIFY c CHAR(4) CHARACTER SET binary"
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE binary_attrs CHANGE v renamed VARCHAR(10) COLLATE binary"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE binary_attrs",
        "`renamed` varbinary(10)"
    );
    failures +=
        expect_show_create_contains(database, "SHOW CREATE TABLE binary_attrs", "`c` binary(4)");
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE binary_attrs",
        "`added` varbinary(2)"
    );
    failures += expect_query_result(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'binary_attrs' "
                   "ORDER BY ORDINAL_POSITION",
            .values = binary_column_values,
            .column_count = binary_attrs_projection_count,
            .row_count = binary_attrs_column_count,
            .context = "binary charset information_schema columns",
        }
    );
    failures += expect_binary_result_metadata(database);
    failures += execute_affected_ok(
        database,
        "INSERT INTO binary_attrs VALUES('a', 'b', 'tiny', 'blob', 'medium', 'long', '', '', "
        "'xy')",
        1
    );
    failures += execute_affected_ok(database, "UPDATE binary_attrs SET renamed = 'z'", 1);
    failures += execute_statement_ok(
        database,
        "CREATE TABLE binary_alter(id INT, v VARCHAR(10), c CHAR(3), txt TEXT)"
    );
    failures += execute_affected_ok(
        database,
        "INSERT INTO binary_alter VALUES(1, 'ab', 'xy', 'hello'), (2, '', 'z', '')",
        2
    );
    failures += execute_statement_ok(
        database,
        "ALTER TABLE binary_alter ADD added VARCHAR(2) COLLATE binary"
    );
    failures += execute_affected_ok(
        database,
        "ALTER TABLE binary_alter MODIFY c CHAR(4) CHARACTER SET binary",
        2
    );
    failures += execute_affected_ok(
        database,
        "ALTER TABLE binary_alter CHANGE v renamed VARCHAR(10) COLLATE binary",
        2
    );
    failures += execute_affected_ok(
        database,
        "ALTER TABLE binary_alter MODIFY txt TEXT CHARACTER SET binary",
        2
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE binary_alter",
        "`renamed` varbinary(10)"
    );
    failures +=
        expect_show_create_contains(database, "SHOW CREATE TABLE binary_alter", "`c` binary(4)");
    failures +=
        expect_show_create_contains(database, "SHOW CREATE TABLE binary_alter", "`txt` blob");
    failures += expect_query_result(
        database,
        (struct expected_query){
            .sql = "SELECT id, HEX(renamed), LENGTH(renamed), HEX(c), LENGTH(c), HEX(txt), "
                   "LENGTH(txt), HEX(added) FROM binary_alter ORDER BY id",
            .values = altered_hex_values,
            .column_count = binary_alter_projection_count,
            .row_count = 2U,
            .context = "nonbinary to binary ALTER values",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "binary charset file preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen binary charset file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_result(
        database,
        (struct expected_query){
            .sql = "SELECT HEX(renamed), HEX(c), HEX(t), HEX(b), HEX(m), HEX(l), HEX(z), "
                   "HEX(cz), HEX(added) FROM binary_attrs",
            .values = updated_hex_values,
            .column_count = binary_attrs_column_count,
            .row_count = 1U,
            .context = "reopened binary charset DML values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_ascii_column_charset_collation_metadata(void) {
    static const char *const ascii_metadata[] = {
        "id",
        NULL,
        NULL,
        "v",
        "ascii",
        "ascii_general_ci",
        "c",
        "ascii",
        "ascii_bin",
        "t",
        "ascii",
        "ascii_general_ci",
        "vc",
        "ascii",
        "ascii_bin",
    };
    static const char *const ascii_lengths[] = {
        "id",
        NULL,
        NULL,
        "v",
        "10",
        "10",
        "c",
        "5",
        "5",
        "t",
        "65535",
        "65535",
        "vc",
        "10",
        "10",
    };
    static const char *const ascii_values[] = {"A", "B", "text", "bin"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "ascii") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open ascii charset file");
    failures += execute_affected_ok(database, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE ascii_attrs("
        "id INT, "
        "v VARCHAR(10) CHARACTER SET ascii, "
        "c CHAR(5) CHARACTER SET ascii COLLATE ascii_bin, "
        "t TEXT COLLATE ascii_general_ci, "
        "vc VARCHAR(10) COLLATE ascii_bin)"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE ascii_attrs",
        "`v` varchar(10) CHARACTER SET ascii COLLATE ascii_general_ci"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE ascii_attrs",
        "`c` char(5) CHARACTER SET ascii COLLATE ascii_bin"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE ascii_attrs",
        "`t` text CHARACTER SET ascii COLLATE ascii_general_ci"
    );
    failures += expect_show_create_contains(
        database,
        "SHOW CREATE TABLE ascii_attrs",
        "`vc` varchar(10) CHARACTER SET ascii COLLATE ascii_bin"
    );
    failures += expect_column_character_metadata(
        database,
        "ascii_attrs",
        ascii_metadata,
        ascii_attrs_column_count,
        "ascii column charset metadata"
    );
    failures += expect_query_result(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'ascii_attrs' "
                   "ORDER BY ORDINAL_POSITION",
            .values = ascii_lengths,
            .column_count = ascii_attrs_length_projection_count,
            .row_count = ascii_attrs_column_count,
            .context = "ascii column character lengths",
        }
    );
    failures += expect_ascii_result_metadata(database);
    failures += execute_statement_ok(database, "CREATE TABLE like_ascii LIKE ascii_attrs");
    failures += expect_column_character_metadata(
        database,
        "like_ascii",
        ascii_metadata,
        ascii_attrs_column_count,
        "ascii like column charset metadata"
    );
    failures += execute_affected_ok(
        database,
        "INSERT INTO ascii_attrs VALUES(1, 'a', 'B', 'text', 'bin')",
        1
    );
    failures += execute_affected_ok(database, "UPDATE ascii_attrs SET v = 'A' WHERE id = 1", 1);
    failures += expect_query_result(
        database,
        (struct expected_query){
            .sql = "SELECT v, c, t, vc FROM ascii_attrs",
            .values = ascii_values,
            .column_count = ascii_result_column_count,
            .row_count = 1U,
            .context = "ascii charset DML values",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "ascii charset file preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen ascii charset file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_column_character_metadata(
        database,
        "ascii_attrs",
        ascii_metadata,
        ascii_attrs_column_count,
        "reopened ascii column charset metadata"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_column_charset_collation_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");
    failures += execute_affected_ok(database, "CREATE DATABASE app", 1);
    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        "CREATE TABLE bad_equal(v VARCHAR(10) CHARACTER SET=utf8mb4)",
        (struct expected_sql_error){mysql_error_parse, "42000", "syntax"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_int(i INT CHARACTER SET utf8mb4)",
        (struct expected_sql_error){mysql_error_parse, "42000", "CHAR, VARCHAR, and TEXT"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_charset(v VARCHAR(10) CHARACTER SET latin2)",
        (struct expected_sql_error){mysql_error_unknown_character_set, "42000", "latin2"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_collation(v VARCHAR(10) COLLATE imaginary_collation)",
        (struct expected_sql_error){mysql_error_unknown_collation, "HY000", "imaginary_collation"}
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_mismatch(v VARCHAR(10) CHARACTER SET latin1 COLLATE utf8mb4_bin)",
        (struct expected_sql_error){
            mysql_error_collation_not_valid_for_character_set,
            "42000",
            "utf8mb4_bin",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_mismatch_0900(v VARCHAR(10) CHARACTER SET latin1 "
        "COLLATE utf8mb4_0900_bin)",
        (struct expected_sql_error){
            mysql_error_collation_not_valid_for_character_set,
            "42000",
            "utf8mb4_0900_bin",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_utf8_binary(v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE binary)",
        (struct expected_sql_error){
            mysql_error_collation_not_valid_for_character_set,
            "42000",
            "COLLATION 'binary' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_binary_utf8(v VARCHAR(10) CHARACTER SET binary COLLATE utf8mb4_bin)",
        (struct expected_sql_error){
            mysql_error_collation_not_valid_for_character_set,
            "42000",
            "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_ascii_utf8(v VARCHAR(10) CHARACTER SET ascii COLLATE utf8mb4_bin)",
        (struct expected_sql_error){
            mysql_error_collation_not_valid_for_character_set,
            "42000",
            "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'ascii'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_utf8_ascii(v VARCHAR(10) CHARACTER SET utf8mb4 COLLATE ascii_bin)",
        (struct expected_sql_error){
            mysql_error_collation_not_valid_for_character_set,
            "42000",
            "COLLATION 'ascii_bin' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_national(v NCHAR(2) CHARACTER SET utf8mb4)",
        (struct expected_sql_error){mysql_error_parse, "42000", "NATIONAL character columns"}
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_column_character_metadata(
    mylite_db *database,
    const char *table_name,
    const char *const *expected_values,
    size_t expected_row_count,
    const char *context
) {
    char query[query_capacity];
    int written = snprintf(
        query,
        sizeof(query),
        "SELECT COLUMN_NAME, CHARACTER_SET_NAME, COLLATION_NAME "
        "FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = '%s' ORDER BY ORDINAL_POSITION",
        table_name
    );

    if (written < 0 || (size_t)written >= sizeof(query)) {
        fprintf(stderr, "%s: failed to build metadata query\n", context);
        return 1;
    }

    return expect_query_result(
        database,
        (struct expected_query){
            .sql = query,
            .values = expected_values,
            .column_count = 3U,
            .row_count = expected_row_count,
            .context = context,
        }
    );
}

static int expect_result_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT v, c, v0900 FROM attrs LIMIT 0", &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 3U, "metadata column count");
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 0U),
            mysql_collation_utf8mb4_bin_id,
            "binary column charset id"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 0U),
            mysql_collation_utf8mb4_bin_id,
            "binary column collation id"
        );
        failures += expect_true(
            (mylite_result_column_flags(result, 0U) & MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U,
            "binary column metadata flag"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 1U),
            mysql_collation_utf8mb4_unicode_ci_id,
            "unicode column charset id"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 1U),
            mysql_collation_utf8mb4_unicode_ci_id,
            "unicode column collation id"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 2U),
            mysql_collation_utf8mb4_0900_bin_id,
            "0900 binary column charset id"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 2U),
            mysql_collation_utf8mb4_0900_bin_id,
            "0900 binary column collation id"
        );
        failures += expect_true(
            (mylite_result_column_flags(result, 2U) & MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U,
            "0900 binary column metadata flag"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_ascii_result_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT v, c, t, vc FROM ascii_attrs LIMIT 0", &result);

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            ascii_result_column_count,
            "ascii metadata column count"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 0U),
            mysql_collation_ascii_general_ci_id,
            "ascii general charset id"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 0U),
            mysql_collation_ascii_general_ci_id,
            "ascii general collation id"
        );
        failures += expect_int64(
            (int64_t)mylite_result_column_display_length(result, 0U),
            ascii_varchar_display_length,
            "ascii varchar display length"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 1U),
            mysql_collation_ascii_bin_id,
            "ascii binary charset id"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 1U),
            mysql_collation_ascii_bin_id,
            "ascii binary collation id"
        );
        failures += expect_true(
            (mylite_result_column_flags(result, 1U) & MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U,
            "ascii binary metadata flag"
        );
        failures += expect_int64(
            (int64_t)mylite_result_column_display_length(result, 1U),
            ascii_char_display_length,
            "ascii char display length"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 2U),
            mysql_collation_ascii_general_ci_id,
            "ascii text charset id"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 2U),
            mysql_collation_ascii_general_ci_id,
            "ascii text collation id"
        );
        failures += expect_int64(
            (int64_t)mylite_result_column_display_length(result, 2U),
            ascii_text_display_length,
            "ascii text display length"
        );
        failures += expect_uint32(
            mylite_result_column_charset_id(result, 3U),
            mysql_collation_ascii_bin_id,
            "ascii varchar binary charset id"
        );
        failures += expect_uint32(
            mylite_result_column_collation_id(result, 3U),
            mysql_collation_ascii_bin_id,
            "ascii varchar binary collation id"
        );
        failures += expect_true(
            (mylite_result_column_flags(result, 3U) & MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U,
            "ascii varchar binary metadata flag"
        );
        failures += expect_int64(
            (int64_t)mylite_result_column_display_length(result, 3U),
            ascii_varchar_display_length,
            "ascii varchar binary display length"
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_binary_result_metadata(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "SELECT renamed, c, t, b, m, l, z, cz, added FROM binary_attrs LIMIT 0",
        &result
    );

    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            binary_attrs_column_count,
            "binary metadata column count"
        );
        for (size_t column_index = 0U; column_index < binary_attrs_column_count; ++column_index) {
            failures += expect_uint32(
                mylite_result_column_charset_id(result, column_index),
                mysql_collation_binary_id,
                "binary charset result metadata id"
            );
            failures += expect_uint32(
                mylite_result_column_collation_id(result, column_index),
                mysql_collation_binary_id,
                "binary collation result metadata id"
            );
            failures += expect_true(
                (mylite_result_column_flags(result, column_index) &
                 MYLITE_RESULT_COLUMN_FLAG_BINARY) != 0U,
                "binary result metadata flag"
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_query_result(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            const size_t value_index = (row * expected.column_count) + column;
            char cell_context[query_capacity];
            int written = snprintf(
                cell_context,
                sizeof(cell_context),
                "%s row %zu column %zu",
                expected.context,
                row,
                column
            );

            if (written < 0 || (size_t)written >= sizeof(cell_context)) {
                failures += 1;
                continue;
            }
            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                cell_context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_create_contains(mylite_db *database, const char *sql, const char *needle) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 2U, sql);
        failures += expect_size(mylite_result_row_count(result), 1U, sql);
        failures += expect_text_contains(mylite_result_value_text(result, 0U, 1U), needle, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), 0, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_affected_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(*out_result);
        *out_result = NULL;
        return 1;
    }
    if (*out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            sql,
            expected.code,
            expected.sqlstate
        );
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
        "/tmp/mylite_column_charset_collation_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
        return -1;
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        failures = 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        failures = 1;
    }
    fclose(file);
    return failures;
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
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
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

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu32 ", got %" PRIu32 "\n", context, expected, actual);
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected text containing \"%s\", got \"%s\"\n",
            context,
            needle == NULL ? "(null)" : needle,
            actual == NULL ? "(null)" : actual
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
