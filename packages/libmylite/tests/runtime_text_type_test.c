#include <mylite/mylite.h>

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
    mysql_error_parse = 1064,
    mysql_error_invalid_default = 1067,
    mysql_error_blob_text_cant_have_default = 1101,
    mysql_error_bad_null = 1048,
    mysql_error_no_default = 1364,
    mysql_error_data_too_long = 1406,
    mysql_error_display_width_out_of_range = 1439,
    mysql_error_incorrect_column_specifier = 1063,
    text_family_row_count = 3,
    text_family_column_count = 6,
    text_expression_default_column_count = 8,
    information_schema_text_column_count = 11,
    text_expression_default_metadata_column_count = 3,
    text_length_metadata_column_count = 7,
    text_length_utf8_variant_count = 8,
    tinytext_overlength_byte_count = 256,
    tinytext_utf8_euro_repeat_count = 85,
    text_overlength_byte_count = 65536,
    text_truncation_nonstrict_insert_column_count = 6,
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

static int test_text_success_persistence_and_introspection(void);
static int test_text_length_arguments(void);
static int test_text_parenthesized_defaults(void);
static int test_text_diagnostics(void);
static int test_text_dml_truncation(void);
static int test_text_literal_dml_truncation(void);
static int test_text_insert_select_dml_truncation(void);
static int test_text_independent_handles(void);
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
static char *make_repeated_chunk_sql(
    const char *prefix,
    const char *chunk,
    size_t chunk_length,
    size_t repeat_count,
    const char *suffix
);

int main(void) {
    int failures = 0;

    failures += test_text_success_persistence_and_introspection();
    failures += test_text_length_arguments();
    failures += test_text_parenthesized_defaults();
    failures += test_text_diagnostics();
    failures += test_text_dml_truncation();
    failures += test_text_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_text_success_persistence_and_introspection(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",      "NO",  "", NULL, "", "tt", "tinytext",   "YES", "", NULL, "",
        "t",  "text",     "YES", "", NULL, "", "mt", "mediumtext", "YES", "", NULL, "",
        "lt", "longtext", "YES", "", NULL, "", "nn", "text",       "NO",  "", NULL, "",
    };
    static const char *const show_create_rows[] = {
        "text_family",
        "CREATE TABLE `text_family` (\n"
        "  `id` int NOT NULL,\n"
        "  `tt` tinytext,\n"
        "  `t` text,\n"
        "  `mt` mediumtext,\n"
        "  `lt` longtext,\n"
        "  `nn` text NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "id",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int",
        "NO",
        NULL,
        "tt",
        "tinytext",
        "255",
        "255",
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "tinytext",
        "YES",
        NULL,
        "t",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "text",
        "YES",
        NULL,
        "mt",
        "mediumtext",
        "16777215",
        "16777215",
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "mediumtext",
        "YES",
        NULL,
        "lt",
        "longtext",
        "4294967295",
        "4294967295",
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "longtext",
        "YES",
        NULL,
        "nn",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "text",
        "NO",
        NULL,
    };
    static const char *const initial_rows[] = {
        "1",
        "tiny",
        "alpha  ",
        "medium",
        "long",
        "nn1",
        "2",
        NULL,
        NULL,
        NULL,
        NULL,
        "nn2",
        "3",
        "",
        "",
        "",
        "",
        "",
    };
    static const char *const updated_row[] = {"1", NULL, "updated", "nn1"};
    static const char *const null_tt_ids[] = {"1", "2"};
    static const char *const order_limited_rows[] = {"1", "nn1", "2", "nn2", "3", "last"};
    static const char *const clone_row[] = {"1", NULL, "updated", "medium", "long", "nn1"};
    static const char *const copied_row[] = {"3", ""};
    static const char *const add_column_rows[] = {
        "1",
        NULL,
        "",
        "2",
        NULL,
        "",
        "3",
        NULL,
        "",
    };
    static const char *const renamed_row[] = {"3", "", "last"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open text success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE text_family (id INT NOT NULL, tt TINYTEXT, t TEXT, mt MEDIUMTEXT, "
        "lt LONGTEXT, nn TEXT NOT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM text_family",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = text_family_column_count,
            .context = "text SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE text_family",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "text SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, IS_NULLABLE, "
                   "COLUMN_DEFAULT FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'text_family' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_text_column_count,
            .row_count = text_family_column_count,
            .context = "text information schema columns",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO text_family VALUES "
        "(1, 'tiny', 'alpha  ', 'medium', 'long', 'nn1'), "
        "(2, NULL, NULL, NULL, NULL, 'nn2'), "
        "(3, '', '', '', '', '')",
        3
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, tt, t, mt, lt, nn FROM text_family ORDER BY id",
            .values = initial_rows,
            .column_count = text_family_column_count,
            .row_count = text_family_row_count,
            .context = "text inserted values",
        }
    );
    failures += expect_dml_ok(database, "UPDATE text_family SET t = 'updated' WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE text_family SET t = 'updated' WHERE id = 1", 0);
    failures += expect_dml_ok(database, "UPDATE text_family SET tt = NULL WHERE id = 1", 1);
    failures += expect_dml_ok(database, "UPDATE text_family SET tt = NULL WHERE id = 2", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM text_family WHERE tt IS NULL ORDER BY id",
            .values = null_tt_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "text IS NULL predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, tt, t, nn FROM text_family WHERE id = 1",
            .values = updated_row,
            .column_count = 4U,
            .row_count = 1U,
            .context = "text update readback",
        }
    );
    failures +=
        expect_dml_ok(database, "UPDATE text_family SET nn = 'last' ORDER BY id DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM text_family ORDER BY id",
            .values = order_limited_rows,
            .column_count = 2U,
            .row_count = text_family_row_count,
            .context = "text ordered limited update",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE clone LIKE text_family");
    failures += expect_dml_ok(
        database,
        "INSERT INTO clone SELECT id, tt, t, mt, lt, nn FROM text_family WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, tt, t, mt, lt, nn FROM clone ORDER BY id",
            .values = clone_row,
            .column_count = text_family_column_count,
            .row_count = 1U,
            .context = "text create table like insert select",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE copied AS SELECT id, t FROM text_family WHERE id = 3"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t FROM copied ORDER BY id",
            .values = copied_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "text create table select",
        }
    );

    failures += expect_statement_ok(database, "ALTER TABLE text_family ADD COLUMN extra TEXT");
    failures +=
        expect_statement_ok(database, "ALTER TABLE text_family ADD COLUMN req TINYTEXT NOT NULL");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, extra, req FROM text_family ORDER BY id",
            .values = add_column_rows,
            .column_count = 3U,
            .row_count = text_family_row_count,
            .context = "text alter add column values",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE text_family TO text_renamed");
    failures += expect_statement_ok(database, "DROP TABLE clone");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, req, nn FROM text_renamed WHERE id = 3",
            .values = renamed_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "text after rename",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "text preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen text success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, req, nn FROM text_renamed WHERE id = 3",
            .values = renamed_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "text persisted after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_text_length_arguments(void) {
    static const char *const utf8_information_schema_rows[] = {
        "u0",
        "tinytext",
        "255",
        "255",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "tinytext",
        "u63",
        "tinytext",
        "255",
        "255",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "tinytext",
        "u64",
        "text",
        "65535",
        "65535",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "text",
        "u16383",
        "text",
        "65535",
        "65535",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "text",
        "u16384",
        "mediumtext",
        "16777215",
        "16777215",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "mediumtext",
        "u4194303",
        "mediumtext",
        "16777215",
        "16777215",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "mediumtext",
        "u4194304",
        "longtext",
        "4294967295",
        "4294967295",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "longtext",
        "u4294967295",
        "longtext",
        "4294967295",
        "4294967295",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "longtext",
    };
    static const char *const show_create_rows[] = {
        "utf8_len",
        "CREATE TABLE `utf8_len` (\n"
        "  `u0` tinytext,\n"
        "  `u63` tinytext,\n"
        "  `u64` text,\n"
        "  `u16383` text,\n"
        "  `u16384` mediumtext,\n"
        "  `u4194303` mediumtext,\n"
        "  `u4194304` longtext,\n"
        "  `u4294967295` longtext\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const ascii_information_schema_rows[] = {
        "a255",   "tinytext",   "255",      "255",      "ascii", "ascii_general_ci", "tinytext",
        "a256",   "text",       "65535",    "65535",    "ascii", "ascii_general_ci", "text",
        "a65535", "text",       "65535",    "65535",    "ascii", "ascii_general_ci", "text",
        "a65536", "mediumtext", "16777215", "16777215", "ascii", "ascii_general_ci", "mediumtext",
    };
    static const char *const binary_information_schema_rows[] = {
        "b255",   "tinyblob",   "255",      "255",      NULL, NULL, "tinyblob",
        "b256",   "blob",       "65535",    "65535",    NULL, NULL, "blob",
        "b65535", "blob",       "65535",    "65535",    NULL, NULL, "blob",
        "b65536", "mediumblob", "16777215", "16777215", NULL, NULL, "mediumblob",
    };
    static const char *const collate_binary_information_schema_rows[] = {
        "c255",   "tinyblob",   "255",      "255",      NULL, NULL, "tinyblob",
        "c256",   "blob",       "65535",    "65535",    NULL, NULL, "blob",
        "c65536", "mediumblob", "16777215", "16777215", NULL, NULL, "mediumblob",
    };
    static const char *const table_binary_information_schema_rows[] = {
        "t255",   "tinyblob",   "255",      "255",      NULL, NULL, "tinyblob",
        "t256",   "blob",       "65535",    "65535",    NULL, NULL, "blob",
        "t65536", "mediumblob", "16777215", "16777215", NULL, NULL, "mediumblob",
    };
    static const char *const alter_rows[] = {
        "id",
        "int",
        "changed",
        "mediumtext",
        "added",
        "text",
    };
    static const char *const clone_rows[] = {
        "u0",
        "tinytext",
        "u63",
        "tinytext",
        "u64",
        "text",
        "u16383",
        "text",
        "u16384",
        "mediumtext",
        "u4194303",
        "mediumtext",
        "u4194304",
        "longtext",
        "u4294967295",
        "longtext",
    };
    static const char *const row_values[] = {"1", "alpha", "beta"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "text-length") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open text length file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE utf8_len (u0 TEXT(0), u63 TEXT(63), u64 TEXT(64), "
        "u16383 TEXT(16383), u16384 TEXT(16384), u4194303 TEXT(4194303), "
        "u4194304 TEXT(4194304), u4294967295 TEXT(4294967295))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'utf8_len' ORDER BY ORDINAL_POSITION",
            .values = utf8_information_schema_rows,
            .column_count = text_length_metadata_column_count,
            .row_count = text_length_utf8_variant_count,
            .context = "TEXT(M) utf8mb4 information schema",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE utf8_len",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "TEXT(M) normalized SHOW CREATE",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ascii_len (a255 TEXT(255) CHARACTER SET ascii, "
        "a256 TEXT(256) CHARACTER SET ascii, a65535 TEXT(65535) CHARACTER SET ascii, "
        "a65536 TEXT(65536) CHARACTER SET ascii)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'ascii_len' ORDER BY ORDINAL_POSITION",
            .values = ascii_information_schema_rows,
            .column_count = text_length_metadata_column_count,
            .row_count = 4U,
            .context = "TEXT(M) ascii information schema",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE binary_len (b255 TEXT(255) CHARACTER SET binary, "
        "b256 TEXT(256) CHARACTER SET binary, b65535 TEXT(65535) CHARACTER SET binary, "
        "b65536 TEXT(65536) CHARACTER SET binary)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'binary_len' ORDER BY ORDINAL_POSITION",
            .values = binary_information_schema_rows,
            .column_count = text_length_metadata_column_count,
            .row_count = 4U,
            .context = "TEXT(M) binary information schema",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE collate_binary_len (c255 TEXT(255) COLLATE binary, "
        "c256 TEXT(256) COLLATE binary, c65536 TEXT(65536) COLLATE binary)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'collate_binary_len' ORDER BY ORDINAL_POSITION",
            .values = collate_binary_information_schema_rows,
            .column_count = text_length_metadata_column_count,
            .row_count = 3U,
            .context = "TEXT(M) collate binary information schema",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE table_binary_len (t255 TEXT(255), t256 TEXT(256), "
        "t65536 TEXT(65536)) DEFAULT CHARSET=binary"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'app' "
                   "AND TABLE_NAME = 'table_binary_len' ORDER BY ORDINAL_POSITION",
            .values = table_binary_information_schema_rows,
            .column_count = text_length_metadata_column_count,
            .row_count = 3U,
            .context = "TEXT(M) table default binary information schema",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE alter_len (id INT, body TEXT(63)) DEFAULT CHARSET=ascii"
    );
    failures += expect_statement_ok(database, "ALTER TABLE alter_len ADD COLUMN added TEXT(256)");
    failures += expect_statement_ok(database, "ALTER TABLE alter_len MODIFY body TEXT(65536)");
    failures +=
        expect_statement_ok(database, "ALTER TABLE alter_len CHANGE body changed TEXT(65536)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'alter_len' "
                   "ORDER BY ORDINAL_POSITION",
            .values = alter_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "TEXT(M) ALTER normalization",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE clone_len LIKE utf8_len");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'clone_len' "
                   "ORDER BY ORDINAL_POSITION",
            .values = clone_rows,
            .column_count = 2U,
            .row_count = text_length_utf8_variant_count,
            .context = "TEXT(M) CREATE TABLE LIKE descriptors",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE row_len (id INT, a TEXT(1), b TEXT(255) CHARACTER SET ascii)"
    );
    failures += expect_dml_ok(database, "INSERT INTO row_len VALUES (1, 'alpha', 'beta')", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM row_len",
            .values = row_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "TEXT(M) row readback",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE too_large (too_big TEXT(4294967296))",
        (struct expected_sql_error){
            .code = mysql_error_display_width_out_of_range,
            .sqlstate = "42000",
            .message_part = "Display width out of range for column 'too_big' (max = 4294967295)",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "TEXT(M) preserves preamble"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen text length file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM row_len",
            .values = row_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "TEXT(M) persisted row readback",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_text_parenthesized_defaults(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "tt",
        "tinytext",
        "YES",
        "",
        "_utf8mb4'tiny'",
        "DEFAULT_GENERATED",
        "t",
        "text",
        "YES",
        "",
        "_utf8mb4'abc'",
        "DEFAULT_GENERATED",
        "empty_text",
        "text",
        "YES",
        "",
        "_utf8mb4''",
        "DEFAULT_GENERATED",
        "mt",
        "mediumtext",
        "YES",
        "",
        "_utf8mb4'medium'",
        "DEFAULT_GENERATED",
        "lt",
        "longtext",
        "YES",
        "",
        "_utf8mb4'long'",
        "DEFAULT_GENERATED",
        "nullable",
        "text",
        "YES",
        "",
        "NULL",
        "DEFAULT_GENERATED",
        "nn",
        "text",
        "NO",
        "",
        "_utf8mb4'required'",
        "DEFAULT_GENERATED",
    };
    static const char *const show_create_rows[] = {
        "text_expr",
        "CREATE TABLE `text_expr` (\n"
        "  `id` int NOT NULL,\n"
        "  `tt` tinytext DEFAULT (_utf8mb4'tiny'),\n"
        "  `t` text DEFAULT (_utf8mb4'abc'),\n"
        "  `empty_text` text DEFAULT (_utf8mb4''),\n"
        "  `mt` mediumtext DEFAULT (_utf8mb4'medium'),\n"
        "  `lt` longtext DEFAULT (_utf8mb4'long'),\n"
        "  `nullable` text DEFAULT (NULL),\n"
        "  `nn` text NOT NULL DEFAULT (_utf8mb4'required')\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "id",
        NULL,
        "",
        "tt",
        "_utf8mb4'tiny'",
        "DEFAULT_GENERATED",
        "t",
        "_utf8mb4'abc'",
        "DEFAULT_GENERATED",
        "empty_text",
        "_utf8mb4''",
        "DEFAULT_GENERATED",
        "mt",
        "_utf8mb4'medium'",
        "DEFAULT_GENERATED",
        "lt",
        "_utf8mb4'long'",
        "DEFAULT_GENERATED",
        "nullable",
        "NULL",
        "DEFAULT_GENERATED",
        "nn",
        "_utf8mb4'required'",
        "DEFAULT_GENERATED",
    };
    static const char *const default_rows[] = {
        "1",
        "tiny",
        "abc",
        "",
        "medium",
        "long",
        NULL,
        "required",
        "2",
        "tiny",
        "abc",
        "",
        "medium",
        "long",
        NULL,
        "required",
    };
    static const char *const update_default_rows[] = {"1", "abc", NULL};
    static const char *const replace_default_rows[] = {"1", "rep", NULL, "required"};
    static const char *const added_rows[] = {"1", "add", "2", "add"};
    static const char *const altered_rows[] = {
        "1",
        NULL,
        "add",
        "2",
        "mod",
        "add",
        "3",
        "changed",
        "add",
    };
    static const char *const altered_show_create_rows[] = {
        "alter_text",
        "CREATE TABLE `alter_text` (\n"
        "  `id` int NOT NULL,\n"
        "  `a` text DEFAULT (_utf8mb4'changed'),\n"
        "  `b` text DEFAULT (_utf8mb4'add')\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const moved_rows[] = {
        "1",
        "2",
        "kept",
        "3",
        "4",
        "right",
    };
    static const char *const moved_show_create_rows[] = {
        "move_text",
        "CREATE TABLE `move_text` (\n"
        "  `id` int NOT NULL,\n"
        "  `b` int NOT NULL,\n"
        "  `a` text DEFAULT (_utf8mb4'right')\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "text-defaults") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open text defaults file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE text_expr (id INT NOT NULL, tt TINYTEXT DEFAULT ('tiny'), "
        "t TEXT DEFAULT ('abc'), empty_text TEXT DEFAULT (''), "
        "mt MEDIUMTEXT DEFAULT ('medium'), lt LONGTEXT DEFAULT ('long'), "
        "nullable TEXT DEFAULT (NULL), nn TEXT NOT NULL DEFAULT ('required'))"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM text_expr",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = text_expression_default_column_count,
            .context = "text expression defaults SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE text_expr",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = text_expression_default_column_count,
            .context = "text expression defaults DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN text_expr",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = text_expression_default_column_count,
            .context = "text expression defaults EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE text_expr",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "text expression defaults SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'text_expr' "
                   "ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = text_expression_default_metadata_column_count,
            .row_count = text_expression_default_column_count,
            .context = "text expression defaults information schema",
        }
    );

    failures += expect_dml_ok(database, "INSERT INTO text_expr (id) VALUES (1)", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO text_expr VALUES "
        "(2, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT)",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, tt, t, empty_text, mt, lt, nullable, nn "
                   "FROM text_expr ORDER BY id",
            .values = default_rows,
            .column_count = text_expression_default_column_count,
            .row_count = 2U,
            .context = "text expression defaults materialization",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE text_expr SET t = DEFAULT, nullable = DEFAULT WHERE id = 1",
        0
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t, nullable FROM text_expr WHERE id = 1",
            .values = update_default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "text expression defaults update default",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE text_replace (id INT NOT NULL, t TEXT DEFAULT ('rep'), "
        "nullable TEXT DEFAULT (NULL), nn TEXT NOT NULL DEFAULT ('required'))"
    );
    failures += expect_dml_ok(database, "REPLACE INTO text_replace (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, t, nullable, nn FROM text_replace",
            .values = replace_default_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "text expression defaults REPLACE materialization",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE text_like LIKE text_expr");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE text_like",
            .values =
                (const char *const[]){
                    "text_like",
                    "CREATE TABLE `text_like` (\n"
                    "  `id` int NOT NULL,\n"
                    "  `tt` tinytext DEFAULT (_utf8mb4'tiny'),\n"
                    "  `t` text DEFAULT (_utf8mb4'abc'),\n"
                    "  `empty_text` text DEFAULT (_utf8mb4''),\n"
                    "  `mt` mediumtext DEFAULT (_utf8mb4'medium'),\n"
                    "  `lt` longtext DEFAULT (_utf8mb4'long'),\n"
                    "  `nullable` text DEFAULT (NULL),\n"
                    "  `nn` text NOT NULL DEFAULT (_utf8mb4'required')\n"
                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
                },
            .column_count = 2U,
            .row_count = 1U,
            .context = "text expression defaults CREATE LIKE",
        }
    );

    failures += expect_statement_ok(
        database,
        "ALTER TABLE text_expr ADD COLUMN added TEXT DEFAULT ('add')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM text_expr ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "text expression defaults ALTER ADD",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE alter_text (id INT NOT NULL, a TEXT)");
    failures +=
        expect_statement_ok(database, "ALTER TABLE alter_text ADD COLUMN b TEXT DEFAULT ('add')");
    failures += expect_dml_ok(database, "INSERT INTO alter_text (id) VALUES (1)", 1);
    failures += expect_statement_ok(
        database,
        "ALTER TABLE alter_text MODIFY COLUMN a TEXT DEFAULT ('mod')"
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_text (id) VALUES (2)", 1);
    failures += expect_statement_ok(
        database,
        "ALTER TABLE alter_text CHANGE COLUMN a a TEXT DEFAULT ('changed')"
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_text (id) VALUES (3)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM alter_text ORDER BY id",
            .values = altered_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "text expression defaults ALTER MODIFY CHANGE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE alter_text",
            .values = altered_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "text expression defaults altered SHOW CREATE",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE move_text (id INT NOT NULL, a TEXT DEFAULT ('left'), b INT NOT NULL)"
    );
    failures += expect_dml_ok(database, "INSERT INTO move_text VALUES (1, 'kept', 2)", 1);
    failures += expect_statement_ok(
        database,
        "ALTER TABLE move_text MODIFY a TEXT DEFAULT ('right') AFTER b"
    );
    failures += expect_dml_ok(database, "INSERT INTO move_text (id, b) VALUES (3, 4)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, b, a FROM move_text ORDER BY id",
            .values = moved_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "text expression defaults ALTER MODIFY rebuild",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE move_text",
            .values = moved_show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "text expression defaults moved SHOW CREATE",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read text defaults preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "text defaults preamble preserved"
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen text defaults file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, a, b FROM alter_text ORDER BY id",
            .values = altered_rows,
            .column_count = 3U,
            .row_count = 3U,
            .context = "text expression defaults persisted after reopen",
        }
    );
    failures += expect_statement_ok(database, "RENAME TABLE text_expr TO text_expr_renamed");
    failures += expect_statement_ok(database, "DROP TABLE text_expr_renamed");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_text_diagnostics(void) {
    static const char *const ignore_null_row[] = {"10", ""};
    static const char *const ignore_default_row[] = {"11", ""};
    static const char *const text_predicate_row[] = {"1"};
    static const char *const numeric_literal_row[] = {"2", "2", "x"};
    static const char *const empty_text_default_warnings[] = {
        "Warning",
        "1101",
        "BLOB, TEXT, GEOMETRY or JSON column 'v' can't have a default value",
        "Warning",
        "1101",
        "BLOB, TEXT, GEOMETRY or JSON column 'nullable' can't have a default value",
    };
    static const char *const empty_text_default_show_create[] = {
        "empty_text_default",
        "CREATE TABLE `empty_text_default` (\n"
        "  `id` int NOT NULL,\n"
        "  `v` text NOT NULL,\n"
        "  `nullable` text\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const empty_text_default_rows[] = {"1", "", NULL};
    static const unsigned char nul_text_value[] = {'a', '\0', 'b'};
    char path[test_path_capacity];
    char too_long_sql
        [sizeof("INSERT INTO diag VALUES (1, '', 'x')") + tinytext_overlength_byte_count];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;
    const char too_long_prefix[] = "INSERT INTO diag VALUES (1, '";
    const char too_long_suffix[] = "', 'x')";
    size_t too_long_prefix_length = strlen(too_long_prefix);

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open text diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE diag (id INT NOT NULL, tt TINYTEXT, nn TEXT NOT NULL)"
    );

    memcpy(too_long_sql, too_long_prefix, too_long_prefix_length + 1U);
    memset(too_long_sql + too_long_prefix_length, 'x', tinytext_overlength_byte_count);
    memcpy(
        too_long_sql + too_long_prefix_length + tinytext_overlength_byte_count,
        too_long_suffix,
        sizeof(too_long_suffix)
    );
    failures += execute_error(
        database,
        too_long_sql,
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'tt' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO diag VALUES (1, 'a', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO diag (id, tt) VALUES (2, 'a')",
        (struct expected_sql_error){
            .code = mysql_error_no_default,
            .sqlstate = "HY000",
            .message_part = "Field 'nn' doesn't have a default value",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO diag VALUES (1, 'ok', 'x')", 1);
    failures += execute_error(
        database,
        "UPDATE diag SET nn = NULL",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO diag VALUES (2, 1, 'x')", 1);
    failures += expect_dml_ok(database, "UPDATE diag SET tt = 0x32 WHERE id = 2", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, tt, nn FROM diag WHERE id = 2",
            .values = numeric_literal_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "text numeric and hex literal DML",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO diag VALUES (3, 'zz', 'a\\0b')", 1);
    failures += execute_ok(database, "SELECT nn FROM diag WHERE id = 3", &result);
    if (result != NULL) {
        const unsigned char *actual =
            (const unsigned char *)mylite_result_value_bytes(result, 0U, 0U);

        failures += expect_size(
            mylite_result_value_size(result, 0U, 0U),
            sizeof(nul_text_value),
            "TEXT NUL byte result size"
        );
        failures += expect_true(actual != NULL, "TEXT NUL byte result value");
        if (actual != NULL) {
            failures +=
                expect_bytes(actual, nul_text_value, sizeof(nul_text_value), "TEXT NUL bytes");
        }
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_statement_ok(database, "CREATE TABLE nul_text_copy (value TEXT)");
    failures +=
        expect_dml_ok(database, "INSERT INTO nul_text_copy SELECT nn FROM diag WHERE id = 3", 1);
    failures += execute_ok(database, "SELECT value FROM nul_text_copy", &result);
    if (result != NULL) {
        const unsigned char *actual =
            (const unsigned char *)mylite_result_value_bytes(result, 0U, 0U);

        failures += expect_size(
            mylite_result_value_size(result, 0U, 0U),
            sizeof(nul_text_value),
            "TEXT INSERT SELECT NUL byte result size"
        );
        failures += expect_true(actual != NULL, "TEXT INSERT SELECT NUL byte result value");
        if (actual != NULL) {
            failures += expect_bytes(
                actual,
                nul_text_value,
                sizeof(nul_text_value),
                "TEXT INSERT SELECT NUL bytes"
            );
        }
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "CREATE TABLE bad_default (v TEXT DEFAULT 1)",
        (struct expected_sql_error){
            .code = mysql_error_blob_text_cant_have_default,
            .sqlstate = "42000",
            .message_part = "can't have a default value",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_string_default (v TEXT DEFAULT 'x')",
        (struct expected_sql_error){
            .code = mysql_error_blob_text_cant_have_default,
            .sqlstate = "42000",
            .message_part = "can't have a default value",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE empty_text_default (id INT NOT NULL, v TEXT NOT NULL DEFAULT '', "
        "nullable TEXT DEFAULT '')",
        &result
    );
    failures += expect_size(mylite_result_column_count(result), 0U, "empty text default columns");
    failures += expect_size(mylite_result_row_count(result), 0U, "empty text default rows");
    failures += expect_size(mylite_result_warning_count(result), 2U, "empty text default warnings");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = empty_text_default_warnings,
            .column_count = 3U,
            .row_count = 2U,
            .context = "empty text default warning rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE empty_text_default",
            .values = empty_text_default_show_create,
            .column_count = 2U,
            .row_count = 1U,
            .context = "empty text default SHOW CREATE",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO empty_text_default (id) VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, nullable FROM empty_text_default",
            .values = empty_text_default_rows,
            .column_count = 3U,
            .row_count = 1U,
            .context = "empty text default materialization",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_numeric_expression_default (v TEXT DEFAULT (123))",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'v'",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE diag ALTER COLUMN nn SET DEFAULT ('x')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'nn'",
        }
    );
    failures += expect_statement_ok(
        database,
        "CREATE TABLE bad_null_default (v TEXT NOT NULL DEFAULT (NULL))"
    );
    failures += execute_error(
        database,
        "INSERT INTO bad_null_default () VALUES ()",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'v' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_pk (v TEXT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "PRIMARY KEY supports only integer columns",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_auto (v TEXT AUTO_INCREMENT PRIMARY KEY)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_column_specifier,
            .sqlstate = "42000",
            .message_part = "Incorrect column specifier for column 'v'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM diag WHERE tt = 'ok'",
            .values = text_predicate_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "TEXT equality predicate",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO diag (id, nn) VALUES (10, NULL)",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM diag WHERE id = 10",
            .values = ignore_null_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "text insert ignore null adjustment",
        }
    );
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO diag (id, tt) VALUES (11, 'a')",
        (struct expected_dml_result){
            .affected_rows = 1,
            .warning_count = 1U,
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, nn FROM diag WHERE id = 11",
            .values = ignore_default_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "text insert ignore no-default adjustment",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_text_dml_truncation(void) {
    int failures = 0;

    failures += test_text_literal_dml_truncation();
    failures += test_text_insert_select_dml_truncation();

    return failures;
}

static int test_text_literal_dml_truncation(void) {
    static const char euro[] = "\xE2\x82\xAC";
    static const char *const strict_trailing_warning[] = {
        "Note",
        "1265",
        "Data truncated for column 'tt' at row 1",
    };
    static const char *const nonstrict_warning[] = {
        "Warning",
        "1265",
        "Data truncated for column 'tt' at row 1",
    };
    static const char *const tinytext_x_row[] = {"255", "x"};
    static const char *const tinytext_n_row[] = {"255", "n"};
    static const char *const nonstrict_insert_row[] = {"255", "a", "65535", "b", "65535", "c"};
    static const char *const utf8_row[] = {"255", "85"};
    static const char *const ignore_row[] = {"255", "i"};
    static const char *const ignore_trailing_row[] = {"255", "h"};
    static const char *const replace_rows[] = {"10", "255", "r", "11", "255", "z"};
    static const char *const update_rows[] = {"1", "255", "q", "2", "255", "q"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    char *sql = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "dml_truncation") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open text truncation file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE text_family (id INT NOT NULL, tt TINYTEXT, t TEXT, nn TEXT NOT NULL)"
    );

    sql = make_repeated_chunk_sql(
        "INSERT INTO text_family (id, tt, nn) VALUES (1, '",
        "x",
        1U,
        tinytext_overlength_byte_count,
        "', 'ok')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += execute_error(
            database,
            sql,
            (struct expected_sql_error){
                .code = mysql_error_data_too_long,
                .sqlstate = "22001",
                .message_part = "Data too long for column 'tt' at row 1",
            }
        );
    }
    free(sql);
    sql = NULL;

    sql = make_repeated_chunk_sql(
        "INSERT INTO text_family (id, tt, nn) VALUES (1, '",
        "x",
        1U,
        tinytext_overlength_byte_count - 1U,
        " ', 'ok')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW WARNINGS",
                .values = strict_trailing_warning,
                .column_count = 3U,
                .row_count = 1U,
                .context = "strict trailing text warning",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), RIGHT(tt, 1) FROM text_family WHERE id = 1",
                .values = tinytext_x_row,
                .column_count = 2U,
                .row_count = 1U,
                .context = "strict trailing text row",
            }
        );
    }
    free(sql);
    sql = NULL;

    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(database, "TRUNCATE text_family");
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_family (id, tt, nn) VALUES (1, '",
        "n",
        1U,
        tinytext_overlength_byte_count - 1U,
        " ', 'ok')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW WARNINGS",
                .values = strict_trailing_warning,
                .column_count = 3U,
                .row_count = 1U,
                .context = "nonstrict trailing text note",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), RIGHT(tt, 1) FROM text_family WHERE id = 1",
                .values = tinytext_n_row,
                .column_count = 2U,
                .row_count = 1U,
                .context = "nonstrict trailing text row",
            }
        );
    }
    free(sql);
    sql = NULL;

    failures += expect_statement_ok(database, "TRUNCATE text_family");
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_family (id, tt, t, nn) VALUES (1, '",
        "a",
        1U,
        tinytext_overlength_byte_count,
        "', '"
    );
    if (sql != NULL) {
        char *with_text = make_repeated_chunk_sql(sql, "b", 1U, text_overlength_byte_count, "', '");

        free(sql);
        sql = with_text;
    }
    if (sql != NULL) {
        char *with_nn = make_repeated_chunk_sql(sql, "c", 1U, text_overlength_byte_count, "')");

        free(sql);
        sql = with_nn;
    }
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 3U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), RIGHT(tt, 1), LENGTH(t), RIGHT(t, 1), "
                       "LENGTH(nn), RIGHT(nn, 1) FROM text_family WHERE id = 1",
                .values = nonstrict_insert_row,
                .column_count = text_truncation_nonstrict_insert_column_count,
                .row_count = 1U,
                .context = "nonstrict inserted text truncation row",
            }
        );
    }
    free(sql);
    sql = NULL;

    failures += expect_statement_ok(database, "TRUNCATE text_family");
    sql = make_repeated_chunk_sql(
        "REPLACE INTO text_family (id, tt, nn) VALUES (10, '",
        "r",
        1U,
        tinytext_overlength_byte_count,
        "', 'ok')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
    }
    free(sql);
    sql = make_repeated_chunk_sql(
        "REPLACE INTO text_family SET id = 11, tt = '",
        "z",
        1U,
        tinytext_overlength_byte_count,
        "', nn = 'ok'"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT id, LENGTH(tt), RIGHT(tt, 1) FROM text_family ORDER BY id",
                .values = replace_rows,
                .column_count = 3U,
                .row_count = 2U,
                .context = "nonstrict replace text truncation rows",
            }
        );
    }
    free(sql);
    sql = NULL;

    failures += expect_statement_ok(database, "TRUNCATE text_family");
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_family (id, tt, nn) VALUES (1, '",
        euro,
        sizeof(euro) - 1U,
        tinytext_utf8_euro_repeat_count,
        "x', 'ok')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), CHAR_LENGTH(tt) FROM text_family WHERE id = 1",
                .values = utf8_row,
                .column_count = 2U,
                .row_count = 1U,
                .context = "text truncation UTF-8 boundary",
            }
        );
    }
    free(sql);
    sql = NULL;

    {
        mylite_result *set_result = NULL;

        failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", &set_result);
        mylite_result_free(set_result);
    }
    failures += expect_statement_ok(database, "TRUNCATE text_family");
    sql = make_repeated_chunk_sql(
        "INSERT IGNORE INTO text_family (id, tt, nn) VALUES (1, '",
        "i",
        1U,
        tinytext_overlength_byte_count,
        "', 'ok')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW WARNINGS",
                .values = nonstrict_warning,
                .column_count = 3U,
                .row_count = 1U,
                .context = "insert ignore text truncation warning",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), RIGHT(tt, 1) FROM text_family WHERE id = 1",
                .values = ignore_row,
                .column_count = 2U,
                .row_count = 1U,
                .context = "insert ignore text truncation row",
            }
        );
    }
    free(sql);
    sql = NULL;

    failures += expect_statement_ok(database, "TRUNCATE text_family");
    sql = make_repeated_chunk_sql(
        "INSERT IGNORE INTO text_family (id, tt, nn) VALUES (1, '",
        "h",
        1U,
        tinytext_overlength_byte_count - 1U,
        " ', 'ok')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW WARNINGS",
                .values = strict_trailing_warning,
                .column_count = 3U,
                .row_count = 1U,
                .context = "insert ignore trailing text note",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), RIGHT(tt, 1) FROM text_family WHERE id = 1",
                .values = ignore_trailing_row,
                .column_count = 2U,
                .row_count = 1U,
                .context = "insert ignore trailing text row",
            }
        );
    }
    free(sql);
    sql = NULL;

    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(database, "TRUNCATE text_family");
    failures += expect_dml_ok(
        database,
        "INSERT INTO text_family (id, tt, nn) VALUES (1, 'a', 'ok'), (2, 'b', 'ok')",
        2
    );
    sql = make_repeated_chunk_sql(
        "UPDATE text_family SET tt = '",
        "q",
        1U,
        tinytext_overlength_byte_count,
        "' ORDER BY id"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 2, .warning_count = 2U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT id, LENGTH(tt), RIGHT(tt, 1) FROM text_family ORDER BY id",
                .values = update_rows,
                .column_count = 3U,
                .row_count = 2U,
                .context = "nonstrict update text truncation rows",
            }
        );
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 0, .warning_count = 2U}
        );
    }
    free(sql);
    sql = NULL;

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_text_insert_select_dml_truncation(void) {
    static const char *const strict_trailing_warning[] = {
        "Note",
        "1265",
        "Data truncated for column 'tt' at row 1",
    };
    static const char *const insert_select_rows[] = {"1", "255", "s", "2", "255", "t"};
    static const char *const scalar_insert_select_row[] = {"255", "v"};
    static const char *const scalar_insert_select_nonstrict_row[] = {"255", "y"};
    static const char *const empty_count[] = {"0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    char *sql = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "dml_insert_select_truncation") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open text insert select file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(database, "SET sql_mode = ''");
    failures += expect_statement_ok(database, "CREATE TABLE text_src (id INT, t TEXT)");
    failures += expect_statement_ok(database, "CREATE TABLE text_dst (id INT, tt TINYTEXT)");
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_src VALUES (1, '",
        "s",
        1U,
        tinytext_overlength_byte_count,
        "')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_ok(database, sql, 1);
    }
    free(sql);
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_src VALUES (2, '",
        "t",
        1U,
        tinytext_overlength_byte_count - 1U,
        " ')"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_ok(database, sql, 1);
    }
    free(sql);
    sql = NULL;
    failures += expect_dml_result(
        database,
        "INSERT INTO text_dst SELECT id, t FROM text_src ORDER BY id",
        (struct expected_dml_result){.affected_rows = 2, .warning_count = 2U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, LENGTH(tt), RIGHT(tt, 1) FROM text_dst ORDER BY id",
            .values = insert_select_rows,
            .column_count = 3U,
            .row_count = 2U,
            .context = "nonstrict insert select text truncation rows",
        }
    );

    {
        mylite_result *set_result = NULL;

        failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", &set_result);
        mylite_result_free(set_result);
    }
    failures += expect_statement_ok(database, "CREATE TABLE text_scalar (tt TINYTEXT)");
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_scalar SELECT '",
        "v",
        1U,
        tinytext_overlength_byte_count - 1U,
        " '"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SHOW WARNINGS",
                .values = strict_trailing_warning,
                .column_count = 3U,
                .row_count = 1U,
                .context = "strict scalar insert select trailing text warning",
            }
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), RIGHT(tt, 1) FROM text_scalar",
                .values = scalar_insert_select_row,
                .column_count = 2U,
                .row_count = 1U,
                .context = "strict scalar insert select trailing text row",
            }
        );
    }
    free(sql);
    sql = NULL;
    failures += expect_statement_ok(database, "TRUNCATE text_scalar");
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_scalar SELECT '",
        "u",
        1U,
        tinytext_overlength_byte_count,
        "'"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += execute_error(
            database,
            sql,
            (struct expected_sql_error){
                .code = mysql_error_data_too_long,
                .sqlstate = "22001",
                .message_part = "Data too long for column 'tt' at row 1",
            }
        );
    }
    free(sql);
    sql = NULL;
    failures += expect_statement_ok(database, "SET sql_mode = ''");
    sql = make_repeated_chunk_sql(
        "INSERT INTO text_scalar SELECT '",
        "y",
        1U,
        tinytext_overlength_byte_count,
        "'"
    );
    if (sql == NULL) {
        failures += 1;
    } else {
        failures += expect_dml_result(
            database,
            sql,
            (struct expected_dml_result){.affected_rows = 1, .warning_count = 1U}
        );
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = "SELECT LENGTH(tt), RIGHT(tt, 1) FROM text_scalar",
                .values = scalar_insert_select_nonstrict_row,
                .column_count = 2U,
                .row_count = 1U,
                .context = "nonstrict scalar insert select text row",
            }
        );
    }
    free(sql);
    sql = NULL;

    {
        mylite_result *set_result = NULL;

        failures += execute_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'", &set_result);
        mylite_result_free(set_result);
    }
    failures += expect_statement_ok(database, "TRUNCATE text_dst");
    failures += execute_error(
        database,
        "INSERT INTO text_dst SELECT id, t FROM text_src WHERE id = 2",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long for column 'tt' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM text_dst",
            .values = empty_count,
            .column_count = 1U,
            .row_count = 1U,
            .context = "strict failed insert select leaves destination empty",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_text_independent_handles(void) {
    static const char *const first_expected[] = {"one"};
    static const char *const second_expected[] = {"two"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first text file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second text file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE t (id INT, body TEXT)");
    failures += expect_statement_ok(second, "CREATE TABLE t (id INT, body TEXT)");
    failures += expect_dml_ok(first, "INSERT INTO t VALUES (1, 'one')", 1);
    failures += expect_dml_ok(second, "INSERT INTO t VALUES (1, 'two')", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT body FROM t WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent text state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT body FROM t WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent text state",
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
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = affected_rows,
            .warning_count = 0U,
        }
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

static char *make_repeated_chunk_sql(
    const char *prefix,
    const char *chunk,
    size_t chunk_length,
    size_t repeat_count,
    const char *suffix
) {
    size_t prefix_length = strlen(prefix);
    size_t suffix_length = strlen(suffix);
    size_t repeated_length = 0U;
    size_t total_length = 0U;
    char *sql = NULL;
    char *cursor = NULL;

    if (chunk_length != 0U && repeat_count > (SIZE_MAX - prefix_length) / chunk_length) {
        return NULL;
    }
    repeated_length = chunk_length * repeat_count;
    if (prefix_length > SIZE_MAX - repeated_length ||
        prefix_length + repeated_length > SIZE_MAX - suffix_length - 1U) {
        return NULL;
    }
    total_length = prefix_length + repeated_length + suffix_length;
    sql = malloc(total_length + 1U);
    if (sql == NULL) {
        return NULL;
    }

    memcpy(sql, prefix, prefix_length);
    cursor = sql + prefix_length;
    for (size_t index = 0U; index < repeat_count; ++index) {
        memcpy(cursor, chunk, chunk_length);
        cursor += chunk_length;
    }
    memcpy(cursor, suffix, suffix_length);
    cursor[suffix_length] = '\0';
    return sql;
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
        "%s/mylite_text_type_%d_%s.mylite",
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
        fprintf(stderr, "%s: expected '%s', got '%s'\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected '%s' to contain '%s'\n", context, actual, needle);
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
