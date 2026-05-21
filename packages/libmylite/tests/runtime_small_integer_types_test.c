#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
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
    small_integer_sql_capacity = 512,
    small_integer_column_count = 8,
    signed_attribute_column_count = 8,
    alias_column_count = 12,
    display_width_column_count = 16,
    alter_display_width_column_count = 4,
    bool_alias_column_count = 3,
    alter_bool_alias_column_count = 2,
    boolean_literal_column_count = 5,
    show_columns_field_count = 6,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_data_out_of_range = 1264,
    mysql_error_display_width_out_of_range = 1439,
    mysql_warning_integer_display_width_deprecated = 1681,
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

struct expected_statement_result {
    size_t warning_count;
    int64_t affected_rows;
};

struct expected_column_descriptor {
    const char *name;
    const char *logical_type;
    bool is_nullable;
};

static int test_small_integer_success_persistence_and_dml(void);
static int test_integer_signed_attribute_lifecycle(void);
static int test_integer_type_alias_lifecycle(void);
static int test_integer_display_width_lifecycle(void);
static int test_bool_boolean_alias_lifecycle(void);
static int test_boolean_literal_lifecycle(void);
static int test_explicit_default_null_lifecycle(void);
static int test_small_integer_diagnostics_and_rollback(void);
static int test_small_integer_independent_handles(void);
static int create_small_integer_table(mylite_db *database, const char *table_name);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t warning_count
);
static int expect_statement_warning_count_and_affected_rows(
    mylite_db *database,
    const char *sql,
    struct expected_statement_result expected
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_display_width_warnings(
    mylite_db *database,
    size_t expected_count,
    const char *context
);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    struct expected_column_descriptor expected,
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

    failures += test_small_integer_success_persistence_and_dml();
    failures += test_integer_signed_attribute_lifecycle();
    failures += test_integer_type_alias_lifecycle();
    failures += test_integer_display_width_lifecycle();
    failures += test_bool_boolean_alias_lifecycle();
    failures += test_boolean_literal_lifecycle();
    failures += test_explicit_default_null_lifecycle();
    failures += test_small_integer_diagnostics_and_rollback();
    failures += test_small_integer_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_small_integer_success_persistence_and_dml(void) {
    static const char *const boundary_rows[] = {
        "1", "0",   "0",   "-32768", "0",     "-8388608", "0",        "1",
        "2", "127", "42",  NULL,     "65535", "8388607",  "16777215", "2",
        "4", "-1",  "255", NULL,     "65535", "-5",       "16777215", "4",
    };
    static const char *const show_columns_rows[] = {
        "id",  "int",
        "NO",  "",
        NULL,  "",
        "ti",  "tinyint",
        "YES", "",
        NULL,  "",
        "tiu", "tinyint unsigned",
        "YES", "",
        NULL,  "",
        "si",  "smallint",
        "YES", "",
        NULL,  "",
        "siu", "smallint unsigned",
        "YES", "",
        NULL,  "",
        "mi",  "mediumint",
        "YES", "",
        NULL,  "",
        "miu", "mediumint unsigned",
        "YES", "",
        NULL,  "",
        "nn",  "tinyint",
        "NO",  "",
        NULL,  "",
    };
    static const char *const show_create_rows[] = {
        "ints",
        "CREATE TABLE `ints` (\n"
        "  `id` int NOT NULL,\n"
        "  `ti` tinyint DEFAULT NULL,\n"
        "  `tiu` tinyint unsigned DEFAULT NULL,\n"
        "  `si` smallint DEFAULT NULL,\n"
        "  `siu` smallint unsigned DEFAULT NULL,\n"
        "  `mi` mediumint DEFAULT NULL,\n"
        "  `miu` mediumint unsigned DEFAULT NULL,\n"
        "  `nn` tinyint NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const changed_column_row[] = {
        "changed",
        "mediumint unsigned",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const added_column_row[] = {
        "added",
        "tinyint unsigned",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const persisted_row[] = {"-1", "255", "255"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += create_small_integer_table(database, "ints");

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = small_integer_column_count,
            .context = "small integer SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = small_integer_column_count,
            .context = "small integer DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = small_integer_column_count,
            .context = "small integer EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ints",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "small integer SHOW CREATE TABLE",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read app schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "ints", &table),
        MYLITE_OK,
        "read ints table"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "ti",
            .logical_type = "TINYINT",
            .is_nullable = true,
        },
        "ti descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "tiu",
            .logical_type = "TINYINT UNSIGNED",
            .is_nullable = true,
        },
        "tiu descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "si",
            .logical_type = "SMALLINT",
            .is_nullable = true,
        },
        "si descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "siu",
            .logical_type = "SMALLINT UNSIGNED",
            .is_nullable = true,
        },
        "siu descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "mi",
            .logical_type = "MEDIUMINT",
            .is_nullable = true,
        },
        "mi descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "miu",
            .logical_type = "MEDIUMINT UNSIGNED",
            .is_nullable = true,
        },
        "miu descriptor"
    );

    failures += expect_dml_ok(database, "UPDATE ints SET ti = 0 WHERE ti = -128", 1);
    failures += expect_dml_ok(database, "UPDATE ints SET tiu = 42 WHERE ti <=> 127", 1);
    failures += expect_dml_ok(
        database,
        "UPDATE ints SET si = NULL WHERE si IS NOT NULL ORDER BY id DESC LIMIT 1",
        1
    );
    failures += expect_dml_ok(database, "DELETE FROM ints WHERE mi IS NULL", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO ints SET id = 4, ti = -1, tiu = 255, siu = 65535, "
        "mi = -5, miu = 16777215, nn = 4",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, tiu, si, siu, mi, miu, nn FROM ints ORDER BY id",
            .values = boundary_rows,
            .column_count = small_integer_column_count,
            .row_count = 3U,
            .context = "small integer DML values",
        }
    );

    failures += expect_statement_ok(database, "ALTER TABLE ints MODIFY ti SMALLINT");
    failures +=
        expect_statement_ok(database, "ALTER TABLE ints CHANGE tiu changed MEDIUMINT UNSIGNED");
    failures += expect_statement_ok(database, "ALTER TABLE ints ADD added TINYINT UNSIGNED");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints LIKE 'changed'",
            .values = changed_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "changed small integer column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints LIKE 'added'",
            .values = added_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "added small integer column",
        }
    );
    failures += expect_dml_ok(database, "UPDATE ints SET added = 255 WHERE id = 4", 1);

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "small integer lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ti, changed, added FROM ints WHERE id = 4",
            .values = persisted_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "small integer updates persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_integer_signed_attribute_lifecycle(void) {
    static const char *const show_columns_rows[] = {
        "id", "int",      "NO",  "", NULL, "", "ti", "tinyint",   "YES", "", NULL, "",
        "si", "smallint", "YES", "", NULL, "", "mi", "mediumint", "YES", "", NULL, "",
        "i",  "int",      "YES", "", NULL, "", "ii", "int",       "YES", "", NULL, "",
        "b",  "bigint",   "YES", "", NULL, "", "nn", "tinyint",   "NO",  "", NULL, "",
    };
    static const char *const show_create_rows[] = {
        "ints",
        "CREATE TABLE `ints` (\n"
        "  `id` int NOT NULL,\n"
        "  `ti` tinyint DEFAULT NULL,\n"
        "  `si` smallint DEFAULT NULL,\n"
        "  `mi` mediumint DEFAULT NULL,\n"
        "  `i` int DEFAULT NULL,\n"
        "  `ii` int DEFAULT NULL,\n"
        "  `b` bigint DEFAULT NULL,\n"
        "  `nn` tinyint NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const ordered_rows[] = {
        "1",
        "-127",
        "-32768",
        "-9223372036854775808",
        "2",
        "127",
        NULL,
        "-7",
    };
    static const char *const changed_column_row[] = {
        "changed",
        "mediumint",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const added_column_row[] = {"added", "tinyint", "YES", "", NULL, ""};
    static const char *const persisted_row[] = {"2", "-7", "7"};
    static const char *const alter_bad_column[] = {"c", "int", "NO", "", NULL, ""};
    static const char *const alter_bad_value[] = {"128"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "signed_attribute") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open signed file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE ints (id INT SIGNED NOT NULL, ti TINYINT SIGNED, "
        "si SMALLINT SIGNED, mi MEDIUMINT SIGNED, i INT SIGNED, "
        "ii INTEGER SIGNED, b BIGINT SIGNED, nn TINYINT SIGNED NOT NULL)"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = signed_attribute_column_count,
            .context = "signed attribute SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = signed_attribute_column_count,
            .context = "signed attribute DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN ints",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = signed_attribute_column_count,
            .context = "signed attribute EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE ints",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "signed attribute SHOW CREATE TABLE",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read signed schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "ints", &table),
        MYLITE_OK,
        "read signed table"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "ti", .logical_type = "TINYINT", .is_nullable = true},
        "signed tinyint descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "si", .logical_type = "SMALLINT", .is_nullable = true},
        "signed smallint descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "mi", .logical_type = "MEDIUMINT", .is_nullable = true},
        "signed mediumint descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "ii", .logical_type = "INT", .is_nullable = true},
        "signed integer descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "b", .logical_type = "BIGINT", .is_nullable = true},
        "signed bigint descriptor"
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO ints VALUES "
        "(1, -128, -32768, -8388608, -2147483648, -2147483648, "
        "-9223372036854775808, -1), "
        "(2, 127, 32767, 8388607, 2147483647, 2147483647, 9223372036854775807, 1)",
        2
    );
    failures += expect_dml_ok(database, "UPDATE ints SET ti = -127 WHERE ti = -128", 1);
    failures += expect_dml_ok(database, "UPDATE ints SET si = NULL WHERE ti <=> 127", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO ints SET id = 3, ti = NULL, si = 0, mi = 0, i = 0, ii = 0, b = 0, nn = 3",
        1
    );
    failures += expect_dml_ok(database, "DELETE FROM ints WHERE ti IS NULL", 1);
    failures +=
        expect_dml_ok(database, "UPDATE ints SET b = -7 WHERE id > 0 ORDER BY ti DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, ti, si, b FROM ints ORDER BY ti",
            .values = ordered_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "signed attribute DML values",
        }
    );

    failures += execute_error(
        database,
        "INSERT INTO ints VALUES (4, 128, 0, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ints VALUES (4, 0, -32769, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'si' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ints VALUES (4, 0, 0, 8388608, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'mi' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ints VALUES (4, 0, 0, 0, 2147483648, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ints VALUES (4, 0, 0, 0, 0, 0, 9223372036854775808, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'b' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ints VALUES (4, 0, 0, 0, 0, 0, 0, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE ints SET ti = 128",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_signed_unsigned (c INT SIGNED UNSIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += expect_statement_ok(database, "ALTER TABLE ints ADD added TINYINT SIGNED");
    failures += expect_statement_ok(database, "ALTER TABLE ints MODIFY si INT SIGNED");
    failures +=
        expect_statement_ok(database, "ALTER TABLE ints CHANGE mi changed MEDIUMINT SIGNED");
    failures += expect_statement_ok(database, "ALTER TABLE ints MODIFY i INT SIGNED");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints LIKE 'changed'",
            .values = changed_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "signed changed column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM ints LIKE 'added'",
            .values = added_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "signed added column",
        }
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "changed", .logical_type = "MEDIUMINT", .is_nullable = true},
        "changed signed descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "added", .logical_type = "TINYINT", .is_nullable = true},
        "added signed descriptor"
    );
    failures += expect_dml_ok(database, "UPDATE ints SET added = 7 WHERE id = 2", 1);

    failures += expect_statement_ok(
        database,
        "CREATE TABLE alter_bad (id INT NOT NULL, c INT SIGNED NOT NULL)"
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_bad VALUES (1, 128)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad MODIFY c TINYINT SIGNED NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'c' at row 1",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad CHANGE c changed TINYINT SIGNED NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'changed' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_bad LIKE 'c'",
            .values = alter_bad_column,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "failed signed ALTER preserves descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c FROM alter_bad WHERE id = 1",
            .values = alter_bad_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed signed ALTER preserves rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "signed attribute lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen signed file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, b, added FROM ints WHERE id = 2",
            .values = persisted_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "signed attribute updates persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_integer_type_alias_lifecycle(void) {
    static const char *const show_columns_rows[] = {
        "id",  "int",
        "NO",  "",
        NULL,  "",
        "i1",  "tinyint",
        "YES", "",
        NULL,  "",
        "i2",  "smallint",
        "YES", "",
        NULL,  "",
        "i3",  "mediumint",
        "YES", "",
        NULL,  "",
        "i4",  "int",
        "YES", "",
        NULL,  "",
        "i8",  "bigint",
        "YES", "",
        NULL,  "",
        "i1u", "tinyint unsigned",
        "YES", "",
        NULL,  "",
        "i2s", "smallint",
        "YES", "",
        NULL,  "",
        "i3u", "mediumint unsigned",
        "YES", "",
        NULL,  "",
        "i4u", "int unsigned",
        "YES", "",
        NULL,  "",
        "i8u", "bigint unsigned",
        "YES", "",
        NULL,  "",
        "nn",  "tinyint",
        "NO",  "",
        NULL,  "",
    };
    static const char *const show_create_rows[] = {
        "aliases",
        "CREATE TABLE `aliases` (\n"
        "  `id` int NOT NULL,\n"
        "  `i1` tinyint DEFAULT NULL,\n"
        "  `i2` smallint DEFAULT NULL,\n"
        "  `i3` mediumint DEFAULT NULL,\n"
        "  `i4` int DEFAULT NULL,\n"
        "  `i8` bigint DEFAULT NULL,\n"
        "  `i1u` tinyint unsigned DEFAULT NULL,\n"
        "  `i2s` smallint DEFAULT NULL,\n"
        "  `i3u` mediumint unsigned DEFAULT NULL,\n"
        "  `i4u` int unsigned DEFAULT NULL,\n"
        "  `i8u` bigint unsigned DEFAULT NULL,\n"
        "  `nn` tinyint NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const ordered_rows[] = {
        "1",
        "-127",
        "-32768",
        "-9223372036854775808",
        "2",
        "127",
        NULL,
        "-7",
    };
    static const char *const changed_column_row[] = {
        "changed",
        "mediumint",
        "YES",
        "",
        NULL,
        "",
    };
    static const char *const added_column_row[] = {"added", "tinyint", "YES", "", NULL, ""};
    static const char *const persisted_row[] = {"2", "-7", "7"};
    static const char *const alter_bad_column[] = {"c", "int", "NO", "", NULL, ""};
    static const char *const alter_bad_value[] = {"128"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "integer_aliases") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open alias file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE aliases (id INT4 NOT NULL, i1 INT1, i2 INT2, i3 INT3, "
        "i4 INT4, i8 INT8, i1u INT1 UNSIGNED, i2s INT2 SIGNED, "
        "i3u INT3 UNSIGNED, i4u INT4 UNSIGNED, i8u INT8 UNSIGNED, "
        "nn INT1 NOT NULL)"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = alias_column_count,
            .context = "integer alias SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE aliases",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = alias_column_count,
            .context = "integer alias DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN aliases",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = alias_column_count,
            .context = "integer alias EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE aliases",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "integer alias SHOW CREATE TABLE",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read alias schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "aliases", &table),
        MYLITE_OK,
        "read alias table"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "i1",
            .logical_type = "TINYINT",
            .is_nullable = true,
        },
        "int1 alias descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "i2",
            .logical_type = "SMALLINT",
            .is_nullable = true,
        },
        "int2 alias descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "i3",
            .logical_type = "MEDIUMINT",
            .is_nullable = true,
        },
        "int3 alias descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "i4",
            .logical_type = "INT",
            .is_nullable = true,
        },
        "int4 alias descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "i8",
            .logical_type = "BIGINT",
            .is_nullable = true,
        },
        "int8 alias descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "i1u",
            .logical_type = "TINYINT UNSIGNED",
            .is_nullable = true,
        },
        "int1 unsigned alias descriptor"
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO aliases VALUES "
        "(1, -128, -32768, -8388608, -2147483648, -9223372036854775808, "
        "0, -32768, 0, 0, 0, -1), "
        "(2, 127, 32767, 8388607, 2147483647, 9223372036854775807, "
        "255, 32767, 16777215, 4294967295, 9223372036854775807, 1)",
        2
    );
    failures += expect_dml_ok(database, "UPDATE aliases SET i1 = -127 WHERE i1 = -128", 1);
    failures += expect_dml_ok(database, "UPDATE aliases SET i2 = NULL WHERE i1 <=> 127", 1);
    failures += expect_dml_ok(
        database,
        "INSERT INTO aliases SET id = 3, i1 = NULL, i2 = 0, i3 = 0, i4 = 0, "
        "i8 = 0, i1u = 0, i2s = 0, i3u = 0, i4u = 0, i8u = 0, nn = 3",
        1
    );
    failures += expect_dml_ok(database, "DELETE FROM aliases WHERE i1 IS NULL", 1);
    failures += expect_dml_ok(
        database,
        "UPDATE aliases SET i8 = -7 WHERE id > 0 ORDER BY i1 DESC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i1, i2, i8 FROM aliases ORDER BY i1",
            .values = ordered_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "integer alias DML values",
        }
    );

    failures += execute_error(
        database,
        "INSERT INTO aliases VALUES (4, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i1' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO aliases VALUES (4, 0, -32769, 0, 0, 0, 0, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i2' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO aliases VALUES (4, 0, 0, 8388608, 0, 0, 0, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i3' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO aliases VALUES (4, 0, 0, 0, 2147483648, 0, 0, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i4' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO aliases VALUES (4, 0, 0, 0, 0, 9223372036854775808, 0, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i8' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE aliases SET i1u = -1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'i1u' at row 1",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_alias_width (c INT1(+1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_bool_width (c BOOL(1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += expect_statement_ok(database, "ALTER TABLE aliases ADD added INT1");
    failures += expect_statement_ok(database, "ALTER TABLE aliases MODIFY i2 INT4");
    failures += expect_statement_ok(database, "ALTER TABLE aliases CHANGE i3 changed INT3");
    failures += expect_statement_ok(database, "ALTER TABLE aliases MODIFY i4 INT4");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases LIKE 'changed'",
            .values = changed_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "alias changed column",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM aliases LIKE 'added'",
            .values = added_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "alias added column",
        }
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "changed",
            .logical_type = "MEDIUMINT",
            .is_nullable = true,
        },
        "changed alias descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (struct expected_column_descriptor){
            .name = "added",
            .logical_type = "TINYINT",
            .is_nullable = true,
        },
        "added alias descriptor"
    );
    failures += expect_dml_ok(database, "UPDATE aliases SET added = 7 WHERE id = 2", 1);

    failures +=
        expect_statement_ok(database, "CREATE TABLE alter_bad (id INT NOT NULL, c INT4 NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO alter_bad VALUES (1, 128)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad MODIFY c INT1 NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'c' at row 1",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad CHANGE c changed INT1 NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'changed' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_bad LIKE 'c'",
            .values = alter_bad_column,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "failed alias ALTER preserves descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c FROM alter_bad WHERE id = 1",
            .values = alter_bad_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed alias ALTER preserves rows",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "integer alias lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen alias file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i8, added FROM aliases WHERE id = 2",
            .values = persisted_row,
            .column_count = 3U,
            .row_count = 1U,
            .context = "integer alias updates persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_integer_display_width_lifecycle(void) {
    static const char *const show_columns_rows[] = {
        "ti0", "tinyint",
        "YES", "",
        NULL,  "",
        "ti1", "tinyint(1)",
        "YES", "",
        NULL,  "",
        "ti2", "tinyint",
        "YES", "",
        NULL,  "",
        "si",  "smallint",
        "YES", "",
        NULL,  "",
        "mi",  "mediumint",
        "YES", "",
        NULL,  "",
        "i",   "int",
        "YES", "",
        NULL,  "",
        "ii",  "int",
        "YES", "",
        NULL,  "",
        "bi",  "bigint",
        "YES", "",
        NULL,  "",
        "iu",  "int unsigned",
        "YES", "",
        NULL,  "",
        "tis", "tinyint(1)",
        "YES", "",
        NULL,  "",
        "tiu", "tinyint unsigned",
        "YES", "",
        NULL,  "",
        "i1",  "tinyint(1)",
        "YES", "",
        NULL,  "",
        "i2",  "smallint",
        "YES", "",
        NULL,  "",
        "i3",  "mediumint",
        "YES", "",
        NULL,  "",
        "i4",  "int",
        "YES", "",
        NULL,  "",
        "i8",  "bigint",
        "YES", "",
        NULL,  "",
    };
    static const char *const show_create_rows[] = {
        "widths",
        "CREATE TABLE `widths` (\n"
        "  `ti0` tinyint DEFAULT NULL,\n"
        "  `ti1` tinyint(1) DEFAULT NULL,\n"
        "  `ti2` tinyint DEFAULT NULL,\n"
        "  `si` smallint DEFAULT NULL,\n"
        "  `mi` mediumint DEFAULT NULL,\n"
        "  `i` int DEFAULT NULL,\n"
        "  `ii` int DEFAULT NULL,\n"
        "  `bi` bigint DEFAULT NULL,\n"
        "  `iu` int unsigned DEFAULT NULL,\n"
        "  `tis` tinyint(1) DEFAULT NULL,\n"
        "  `tiu` tinyint unsigned DEFAULT NULL,\n"
        "  `i1` tinyint(1) DEFAULT NULL,\n"
        "  `i2` smallint DEFAULT NULL,\n"
        "  `i3` mediumint DEFAULT NULL,\n"
        "  `i4` int DEFAULT NULL,\n"
        "  `i8` bigint DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const selected_rows[] = {
        "-1",
        "127",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "10",
        "11",
        "12",
        "13",
        "14",
        "15",
    };
    static const char *const tinyint_one_row[] = {"127"};
    static const char *const width_255_columns[] = {"c", "int", "YES", "", NULL, ""};
    static const char *const alter_show_columns_rows[] = {
        "a",  "tinyint(1)", "YES", "", NULL, "", "b", "tinyint", "YES", "", NULL, "",
        "c2", "int",        "YES", "", NULL, "", "d", "int",     "YES", "", NULL, "",
    };
    static const char *const alter_selected_rows[] = {"1", "1", "5", NULL, "2", "0", "6", NULL};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    uint64_t sqlite_schema_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "display_width") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open display width file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_warning_count(
        database,
        "CREATE TABLE widths (ti0 TINYINT(0), ti1 TINYINT(1), ti2 TINYINT(2), "
        "si SMALLINT(5), mi MEDIUMINT(9), i INT(11), ii INTEGER(10), "
        "bi BIGINT(20), iu INT(10) UNSIGNED, tis TINYINT(1) SIGNED, "
        "tiu TINYINT(1) UNSIGNED, i1 INT1(1), i2 INT2(5), i3 INT3(7), "
        "i4 INT4(9), i8 INT8(20))",
        display_width_column_count
    );
    failures += expect_display_width_warnings(
        database,
        display_width_column_count,
        "display width CREATE warnings"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM widths",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = display_width_column_count,
            .context = "display width SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE widths",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = display_width_column_count,
            .context = "display width DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN widths",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = display_width_column_count,
            .context = "display width EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE widths",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "display width SHOW CREATE TABLE",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read display width schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "widths", &table),
        MYLITE_OK,
        "read display width table"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "ti0", .logical_type = "TINYINT", .is_nullable = true},
        "ti0 display width descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "ti1", .logical_type = "TINYINT(1)", .is_nullable = true},
        "ti1 display width descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "tis", .logical_type = "TINYINT(1)", .is_nullable = true},
        "tis display width descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "tiu", .logical_type = "TINYINT UNSIGNED", .is_nullable = true},
        "tiu display width descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "i1", .logical_type = "TINYINT(1)", .is_nullable = true},
        "int1 display width descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "i2", .logical_type = "SMALLINT", .is_nullable = true},
        "int2 display width descriptor"
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO widths VALUES (-1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, "
        "11, 12, 13, 14, 15)",
        1
    );
    failures += expect_dml_ok(database, "UPDATE widths SET ti1 = 127 WHERE i1 <=> 11", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ti1 FROM widths WHERE ti1 = 127",
            .values = tinyint_one_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "display width updated value",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ti0, ti1, ti2, si, mi, i, ii, bi, iu, tis, tiu, i1, i2, i3, i4, i8 "
                   "FROM widths",
            .values = selected_rows,
            .column_count = display_width_column_count,
            .row_count = 1U,
            .context = "display width selected values",
        }
    );

    failures += expect_statement_warning_count(database, "CREATE TABLE width_255 (c INT(255))", 1U);
    failures += expect_display_width_warnings(database, 1U, "display width 255 warning");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM width_255",
            .values = width_255_columns,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "display width 255 SHOW COLUMNS",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE width_256 (c INT(256))",
        (struct expected_sql_error){
            .code = mysql_error_display_width_out_of_range,
            .sqlstate = "42000",
            .message_part = "Display width out of range for column 'c' (max = 255)",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE width_plus (c INT(+1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE width_minus (c INT(-1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE width_empty (c INT())",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE width_after_unsigned (c INT UNSIGNED(1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE width_zerofill (c INT(1) ZEROFILL)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures += expect_statement_warning_count(
        database,
        "CREATE TABLE alter_widths (a TINYINT, b TINYINT(1), c INT)",
        1U
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_widths VALUES (1, 1, 5), (2, 0, 6)", 2);

    sqlite_schema_generation = mylite_connection_session_state(database)->sqlite_schema_generation;
    failures += expect_statement_warning_count_and_affected_rows(
        database,
        "ALTER TABLE alter_widths MODIFY a TINYINT(1)",
        (struct expected_statement_result){.warning_count = 1U, .affected_rows = 0}
    );
    failures += expect_int64(
        (int64_t)mylite_connection_session_state(database)->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "display width metadata ALTER keeps SQLite schema generation"
    );

    sqlite_schema_generation = mylite_connection_session_state(database)->sqlite_schema_generation;
    failures += expect_dml_ok(database, "ALTER TABLE alter_widths MODIFY b TINYINT", 0);
    failures += expect_int64(
        (int64_t)mylite_connection_session_state(database)->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "display width removal ALTER keeps SQLite schema generation"
    );

    sqlite_schema_generation = mylite_connection_session_state(database)->sqlite_schema_generation;
    failures += expect_statement_warning_count_and_affected_rows(
        database,
        "ALTER TABLE alter_widths CHANGE c c2 INT(1)",
        (struct expected_statement_result){.warning_count = 1U, .affected_rows = 0}
    );
    failures += expect_true(
        mylite_connection_session_state(database)->sqlite_schema_generation >
            sqlite_schema_generation,
        "display width CHANGE rename updates SQLite schema generation"
    );
    failures += expect_statement_warning_count_and_affected_rows(
        database,
        "ALTER TABLE alter_widths ADD d INT(1)",
        (struct expected_statement_result){.warning_count = 1U, .affected_rows = 0}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_widths",
            .values = alter_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = alter_display_width_column_count,
            .context = "display width ALTER SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c2, d FROM alter_widths ORDER BY c2",
            .values = alter_selected_rows,
            .column_count = alter_display_width_column_count,
            .row_count = 2U,
            .context = "display width ALTER values",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "display width lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen display width file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM widths",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = display_width_column_count,
            .context = "display width metadata persists after reopen",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ti1 FROM widths WHERE i1 <=> 11",
            .values = tinyint_one_row,
            .column_count = 1U,
            .row_count = 1U,
            .context = "display width update persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_bool_boolean_alias_lifecycle(void) {
    static const char *const show_columns_rows[] = {
        "b",
        "tinyint(1)",
        "YES",
        "",
        NULL,
        "",
        "c",
        "tinyint(1)",
        "YES",
        "",
        NULL,
        "",
        "nn",
        "tinyint(1)",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const show_create_rows[] = {
        "bools",
        "CREATE TABLE `bools` (\n"
        "  `b` tinyint(1) DEFAULT NULL,\n"
        "  `c` tinyint(1) DEFAULT NULL,\n"
        "  `nn` tinyint(1) NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const selected_rows[] = {
        "-1",
        "0",
        "1",
        "1",
        "127",
        "1",
        "127",
        NULL,
        "1",
    };
    static const char *const ordered_rows[] = {"127", "1"};
    static const char *const warning_rows[] = {0};
    static const char *const alter_show_columns_rows[] = {
        "flag",
        "tinyint(1)",
        "NO",
        "",
        NULL,
        "",
        "added",
        "tinyint(1)",
        "NO",
        "",
        NULL,
        "",
    };
    static const char *const alter_selected_rows[] = {"1", "0", "2", "0"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    uint64_t sqlite_schema_generation = 0U;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "bool_boolean") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open bool alias file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE bools (b BOOL, c BOOLEAN, nn BOOL NOT NULL)");
    failures +=
        expect_statement_ok(database, "CREATE TABLE bool_identifiers (BOOL INT, UNKNOWN INT)");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = warning_rows,
            .column_count = 3U,
            .row_count = 0U,
            .context = "bool alias CREATE has no warnings",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM bools",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = bool_alias_column_count,
            .context = "bool alias SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "DESCRIBE bools",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = bool_alias_column_count,
            .context = "bool alias DESCRIBE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "EXPLAIN bools",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = bool_alias_column_count,
            .context = "bool alias EXPLAIN table",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE bools",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "bool alias SHOW CREATE TABLE",
        }
    );

    failures += expect_int(
        mylite_catalog_read_schema_by_name(database, "app", &schema),
        MYLITE_OK,
        "read bool alias schema"
    );
    failures += expect_int(
        mylite_catalog_read_table_by_name(database, schema.schema_id, "bools", &table),
        MYLITE_OK,
        "read bool alias table"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "b", .logical_type = "TINYINT(1)", .is_nullable = true},
        "bool descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "c", .logical_type = "TINYINT(1)", .is_nullable = true},
        "boolean descriptor"
    );
    failures += expect_column_descriptor(
        database,
        table.table_id,
        (
            struct expected_column_descriptor
        ){.name = "nn", .logical_type = "TINYINT(1)", .is_nullable = false},
        "bool not null descriptor"
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO bools VALUES (-1, 0, 1), (1, 2, 1), (127, NULL, 1)",
        3
    );
    failures += expect_dml_ok(database, "UPDATE bools SET c = 127 WHERE b = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT b, c, nn FROM bools ORDER BY b",
            .values = selected_rows,
            .column_count = bool_alias_column_count,
            .row_count = 3U,
            .context = "bool alias selected values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT b FROM bools WHERE b IS NOT NULL ORDER BY b DESC LIMIT 2",
            .values = ordered_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "bool alias predicate order limit",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM bools WHERE b = -1", 1);

    failures += execute_error(
        database,
        "INSERT INTO bools VALUES (128, 0, 1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'b' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO bools VALUES (-129, 0, 1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'b' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO bools VALUES (0, 0, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures +=
        expect_statement_warning_count(database, "CREATE TABLE alter_bools (a TINYINT(1))", 1U);
    failures += expect_dml_ok(database, "INSERT INTO alter_bools VALUES (1), (2)", 2);

    sqlite_schema_generation = mylite_connection_session_state(database)->sqlite_schema_generation;
    failures += expect_dml_ok(database, "ALTER TABLE alter_bools MODIFY a BOOL", 0);
    failures += expect_int64(
        (int64_t)mylite_connection_session_state(database)->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "bool alias metadata ALTER keeps SQLite schema generation"
    );

    sqlite_schema_generation = mylite_connection_session_state(database)->sqlite_schema_generation;
    failures += expect_dml_ok(database, "ALTER TABLE alter_bools CHANGE a a BOOLEAN", 0);
    failures += expect_int64(
        (int64_t)mylite_connection_session_state(database)->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "boolean alias same-column CHANGE keeps SQLite schema generation"
    );

    sqlite_schema_generation = mylite_connection_session_state(database)->sqlite_schema_generation;
    failures +=
        expect_dml_ok(database, "ALTER TABLE alter_bools CHANGE a flag BOOLEAN NOT NULL", 0);
    failures += expect_true(
        mylite_connection_session_state(database)->sqlite_schema_generation >
            sqlite_schema_generation,
        "bool alias CHANGE rename updates SQLite schema generation"
    );
    failures += expect_dml_ok(database, "ALTER TABLE alter_bools ADD added BOOL NOT NULL", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_bools",
            .values = alter_show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = alter_bool_alias_column_count,
            .context = "bool alias ALTER SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT flag, added FROM alter_bools ORDER BY flag",
            .values = alter_selected_rows,
            .column_count = alter_bool_alias_column_count,
            .row_count = 2U,
            .context = "bool alias ALTER values",
        }
    );

    failures += execute_error(
        database,
        "CREATE TABLE bad_bool_width (c BOOL(1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_boolean_width (c BOOLEAN(1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_bool_unsigned (c BOOL UNSIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_bool_signed (c BOOL SIGNED)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE bad_bool_zerofill (c BOOL ZEROFILL)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "bool alias lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen bool alias file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM bools",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = bool_alias_column_count,
            .context = "bool alias metadata persists after reopen",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT b, c, nn FROM bools ORDER BY b",
            .values = &selected_rows[bool_alias_column_count],
            .column_count = bool_alias_column_count,
            .row_count = 2U,
            .context = "bool alias DML persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_boolean_literal_lifecycle(void) {
    static const char *const after_insert_rows[] = {
        "1",
        "1",
        "0",
        "1",
        "0",
        "2",
        "0",
        "1",
        "0",
        "1",
        "3",
        "1",
        "0",
        "0",
        "1",
    };
    static const char *const updated_i_rows[] = {"1", "1", "2", "1", "3", "1"};
    static const char *const updated_c_rows[] = {"1", "0", "2", "0", "3", "0"};
    static const char *const noop_b_rows[] = {"1", "1", "2", "0", "3", "1"};
    static const char *const true_predicate_rows[] = {"1", "3"};
    static const char *const false_predicate_rows[] = {"2"};
    static const char *const greater_false_predicate_rows[] = {"2", "3"};
    static const char *const all_id_rows[] = {"1", "2", "3"};
    static const char *const remaining_id_rows[] = {"1", "3"};
    static const char *const final_rows[] = {"1", "1", "3", "0"};
    static const char *const final_true_rows[] = {"1"};
    static const char *const second_handle_rows[] = {"1", "0"};
    char path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_db *second_database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "boolean_literals") != 0 ||
        make_test_path(second_path, sizeof(second_path), "boolean_literals_second") != 0) {
        return 1;
    }
    remove_related_files(path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open boolean literal file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE flags ("
        "id INT NOT NULL, b BOOL NULL, c BOOLEAN NOT NULL, i INT NULL, u INT UNSIGNED NULL)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO flags VALUES (1, TRUE, FALSE, TRUE, FALSE), "
        "(2, false, true, false, true)",
        2
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO flags SET id = 3, b = TRUE, c = FALSE, i = false, u = true",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, b, c, i, u FROM flags ORDER BY id",
            .values = after_insert_rows,
            .column_count = boolean_literal_column_count,
            .row_count = 3U,
            .context = "boolean literal insert values",
        }
    );

    failures += expect_dml_ok(database, "UPDATE flags SET i = TRUE WHERE i = FALSE", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i FROM flags ORDER BY id",
            .values = updated_i_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "boolean literal update true",
        }
    );
    failures += expect_dml_ok(database, "UPDATE flags SET c = FALSE WHERE b = FALSE", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, c FROM flags ORDER BY id",
            .values = updated_c_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "boolean literal update false into not null",
        }
    );
    failures += expect_dml_ok(database, "UPDATE flags SET b = TRUE WHERE b = TRUE", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, b FROM flags ORDER BY id",
            .values = noop_b_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "boolean literal no-op changed rows",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE b = TRUE ORDER BY id",
            .values = true_predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "boolean literal equal true predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE b <=> TRUE ORDER BY id",
            .values = true_predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "boolean literal null-safe true predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE b <> TRUE ORDER BY id",
            .values = false_predicate_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "boolean literal not-equal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE b != TRUE ORDER BY id",
            .values = false_predicate_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "boolean literal bang-not-equal predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE b < TRUE ORDER BY id",
            .values = false_predicate_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "boolean literal less-than predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE u > FALSE ORDER BY id",
            .values = greater_false_predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "boolean literal greater-than predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE c <= FALSE ORDER BY id",
            .values = all_id_rows,
            .column_count = 1U,
            .row_count = 3U,
            .context = "boolean literal range predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE u >= FALSE ORDER BY id",
            .values = all_id_rows,
            .column_count = 1U,
            .row_count = 3U,
            .context = "boolean literal unsigned predicate",
        }
    );

    failures += expect_dml_ok(database, "DELETE FROM flags WHERE b = FALSE", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags ORDER BY id",
            .values = remaining_id_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "boolean literal delete predicate",
        }
    );
    failures += expect_dml_ok(database, "UPDATE flags SET b = FALSE ORDER BY id DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, b FROM flags ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "boolean literal ordered limited update",
        }
    );

    failures += execute_error(
        database,
        "SELECT id FROM flags LIMIT TRUE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO flags VALUES (+TRUE, FALSE, FALSE, FALSE, FALSE)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE flags SET i = -FALSE",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "UPDATE flags SET i = TRUE / 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE TRUE ORDER BY id",
            .values = remaining_id_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "boolean literal bare true predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM flags WHERE b IS TRUE",
            .values = final_true_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "boolean literal is true predicate",
        }
    );

    failures += expect_int(
        mylite_open(second_path, &second_database),
        MYLITE_OK,
        "open second boolean file"
    );
    failures += expect_statement_ok(second_database, "CREATE DATABASE app");
    failures += expect_statement_ok(second_database, "USE app");
    failures += expect_statement_ok(second_database, "CREATE TABLE flags (id INT, b BOOL)");
    failures += expect_dml_ok(second_database, "INSERT INTO flags VALUES (1, FALSE)", 1);
    failures += expect_query_values(
        second_database,
        (struct expected_query){
            .sql = "SELECT id, b FROM flags",
            .values = second_handle_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "second handle boolean literal state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, b FROM flags ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "first handle boolean literal state",
        }
    );
    mylite_close(second_database);
    second_database = NULL;
    remove_related_files(second_path);

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "boolean literal lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen boolean literal file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, b FROM flags ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "boolean literal rows persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_explicit_default_null_lifecycle(void) {
    static const char *const show_create_rows[] = {
        "defaults",
        "CREATE TABLE `defaults` (\n"
        "  `a` int DEFAULT NULL,\n"
        "  `b` bigint unsigned DEFAULT NULL,\n"
        "  `c` tinyint(1) DEFAULT NULL,\n"
        "  `nn` int NOT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const initial_rows[] = {NULL, NULL, NULL, "7"};
    static const char *const explicit_default_rows[] =
        {NULL, NULL, NULL, "1", NULL, NULL, NULL, "7"};
    static const char *const update_default_rows[] = {NULL, NULL, NULL, "1", NULL, NULL, NULL, "7"};
    static const char *const added_rows[] = {"1", NULL, "2", NULL};
    static const char *const omitted_added_rows[] = {"1", NULL, "2", NULL, "3", NULL};
    static const char *const renamed_column_row[] = {"renamed", "bigint", "YES", "", NULL, ""};
    static const char *const second_handle_rows[] = {NULL};
    char path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_db *second_database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "explicit_default_null") != 0 ||
        make_test_path(second_path, sizeof(second_path), "explicit_default_null_second") != 0) {
        return 1;
    }
    remove_related_files(path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open default null file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE defaults ("
        "a INT DEFAULT NULL, b BIGINT UNSIGNED NULL DEFAULT NULL, "
        "c BOOL DEFAULT NULL, nn INT NOT NULL)"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE defaults",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "explicit default null SHOW CREATE",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO defaults (nn) VALUES (7)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c, nn FROM defaults",
            .values = initial_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "explicit default null omitted nullable values",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE create_bad (bad INT NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'bad'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE IF NOT EXISTS defaults (bad INT NOT NULL DEFAULT NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'bad'",
        }
    );
    failures += expect_statement_warning_count(
        database,
        "CREATE TABLE IF NOT EXISTS defaults (a INT, a INT)",
        1U
    );

    failures += expect_statement_ok(database, "CREATE TABLE alter_target (id INT NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO alter_target VALUES (1), (2)", 2);
    failures += expect_statement_warning_count_and_affected_rows(
        database,
        "ALTER TABLE alter_target ADD COLUMN added INT DEFAULT NULL",
        (struct expected_statement_result){.warning_count = 0U, .affected_rows = 0}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM alter_target ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "default null add column backfills null",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO alter_target (id) VALUES (3)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM alter_target ORDER BY id",
            .values = omitted_added_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "default null added column omitted insert",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_target ADD COLUMN bad_added INT NOT NULL DEFAULT NULL",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'bad_added'",
        }
    );
    failures += expect_statement_warning_count_and_affected_rows(
        database,
        "ALTER TABLE alter_target MODIFY added BIGINT DEFAULT NULL",
        (struct expected_statement_result){.warning_count = 0U, .affected_rows = 3}
    );
    failures += expect_statement_warning_count_and_affected_rows(
        database,
        "ALTER TABLE alter_target CHANGE added renamed BIGINT DEFAULT NULL",
        (struct expected_statement_result){.warning_count = 0U, .affected_rows = 0}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_target LIKE 'renamed'",
            .values = renamed_column_row,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "default null change column metadata",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM alter_target ORDER BY id",
            .values = omitted_added_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "default null modify change preserves rows",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_target MODIFY renamed BIGINT NOT NULL DEFAULT NULL",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'renamed'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_default (a INT DEFAULT 'abc')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'a'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_order (a INT DEFAULT NULL NULL)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures +=
        expect_dml_ok(database, "INSERT INTO defaults VALUES (DEFAULT, DEFAULT, DEFAULT, 1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c, nn FROM defaults ORDER BY nn",
            .values = explicit_default_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "explicit DML DEFAULT nullable values",
        }
    );
    failures += expect_dml_ok(database, "UPDATE defaults SET a = DEFAULT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT a, b, c, nn FROM defaults ORDER BY nn",
            .values = update_default_rows,
            .column_count = 4U,
            .row_count = 2U,
            .context = "update DML DEFAULT nullable values",
        }
    );

    failures += expect_int(
        mylite_open(second_path, &second_database),
        MYLITE_OK,
        "open second default null file"
    );
    failures += expect_statement_ok(second_database, "CREATE DATABASE app");
    failures += expect_statement_ok(second_database, "USE app");
    failures += expect_statement_ok(second_database, "CREATE TABLE defaults (id INT DEFAULT NULL)");
    failures += expect_dml_ok(second_database, "INSERT INTO defaults VALUES (NULL)", 1);
    failures += expect_query_values(
        second_database,
        (struct expected_query){
            .sql = "SELECT id FROM defaults",
            .values = second_handle_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle default null state",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM alter_target ORDER BY id",
            .values = omitted_added_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "first handle default null state",
        }
    );
    mylite_close(second_database);
    second_database = NULL;
    remove_related_files(second_path);

    mylite_close(database);
    database = NULL;

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "explicit default null lifecycle preserves preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen default null file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, renamed FROM alter_target ORDER BY id",
            .values = omitted_added_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "default null rows persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_small_integer_diagnostics_and_rollback(void) {
    static const char *const alter_bad_column[] = {"c", "int", "NO", "", NULL, ""};
    static const char *const alter_bad_value[] = {"128"};
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
        "CREATE TABLE ranges (id INT NOT NULL, ti TINYINT NOT NULL, "
        "tiu TINYINT UNSIGNED, si SMALLINT, siu SMALLINT UNSIGNED, "
        "mi MEDIUMINT, miu MEDIUMINT UNSIGNED)"
    );
    failures += expect_dml_ok(database, "INSERT INTO ranges VALUES (1, 0, 0, 0, 0, 0, 0)", 1);

    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 128, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, -129, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 256, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, -1, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 32768, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'si' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, -32769, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'si' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 65536, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'siu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, -1, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'siu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, 8388608, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'mi' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, -8388609, 0)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'mi' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, 0, 16777216)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'miu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, 0, 0, 0, 0, 0, -1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'miu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO ranges VALUES (2, NULL, 0, 0, 0, 0, 0)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'ti' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE ranges SET ti = 128",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' at row 1",
        }
    );
    failures += execute_error(
        database,
        "UPDATE ranges SET tiu = -1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' at row 1",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM ranges WHERE ti = 128",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'ti' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "DELETE FROM ranges WHERE tiu = -1",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'tiu' in WHERE",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_width (c TINYINT(+1))",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE alter_bad (id INT NOT NULL, c INT NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO alter_bad VALUES (1, 128)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad MODIFY c TINYINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'c' at row 1",
        }
    );
    failures += execute_error(
        database,
        "ALTER TABLE alter_bad CHANGE c changed TINYINT NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'changed' at row 1",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM alter_bad LIKE 'c'",
            .values = alter_bad_column,
            .column_count = show_columns_field_count,
            .row_count = 1U,
            .context = "failed small integer ALTER preserves descriptor",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT c FROM alter_bad WHERE id = 1",
            .values = alter_bad_value,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed small integer ALTER preserves rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_small_integer_independent_handles(void) {
    static const char *const first_expected[] = {"7"};
    static const char *const second_expected[] = {"1"};
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE ints (id INT NOT NULL, ti TINYINT)");
    failures += expect_statement_ok(second, "CREATE TABLE ints (id INT NOT NULL, ti TINYINT)");
    failures += expect_dml_ok(first, "INSERT INTO ints VALUES (1, 1)", 1);
    failures += expect_dml_ok(second, "INSERT INTO ints VALUES (1, 1)", 1);
    failures += expect_dml_ok(first, "UPDATE ints SET ti = 7 WHERE id = 1", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT ti FROM ints WHERE id = 1",
            .values = first_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first small integer handle",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT ti FROM ints WHERE id = 1",
            .values = second_expected,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second small integer handle",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int create_small_integer_table(mylite_db *database, const char *table_name) {
    char sql[small_integer_sql_capacity];
    int written = snprintf(
        sql,
        sizeof(sql),
        "CREATE TABLE %s (id INT NOT NULL, ti TINYINT, tiu TINYINT UNSIGNED, "
        "si SMALLINT, siu SMALLINT UNSIGNED, mi MEDIUMINT, "
        "miu MEDIUMINT UNSIGNED, nn TINYINT NOT NULL)",
        table_name
    );
    int failures = 0;

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "small integer CREATE TABLE SQL is too long\n");
        return 1;
    }
    failures += expect_statement_ok(database, sql);

    written = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO %s VALUES "
        "(1, -128, 0, -32768, 0, -8388608, 0, 1), "
        "(2, 127, 255, 32767, 65535, 8388607, 16777215, 2), "
        "(3, NULL, NULL, NULL, NULL, NULL, NULL, 3)",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        fprintf(stderr, "small integer INSERT SQL is too long\n");
        return failures + 1;
    }
    failures += expect_dml_ok(database, sql, 3);

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

static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), warning_count, sql);
    mylite_result_free(result);

    return failures;
}

static int expect_statement_warning_count_and_affected_rows(
    mylite_db *database,
    const char *sql,
    struct expected_statement_result expected
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

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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

static int expect_display_width_warnings(
    mylite_db *database,
    size_t expected_count,
    const char *context
) {
    static const char *const display_width_warning_message =
        "Integer display width is deprecated and will be removed in a future release.";
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS", &result);

    failures += expect_size(mylite_result_column_count(result), 3U, context);
    failures += expect_size(mylite_result_row_count(result), expected_count, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t row = 0U; row < expected_count; ++row) {
        failures += expect_text(mylite_result_value_text(result, row, 0U), "Warning", context);
        failures += expect_text(mylite_result_value_text(result, row, 1U), "1681", context);
        failures += expect_text(
            mylite_result_value_text(result, row, 2U),
            display_width_warning_message,
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
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int expect_column_descriptor(
    mylite_db *database,
    int64_t table_id,
    struct expected_column_descriptor expected,
    const char *context
) {
    struct mylite_catalog_column_descriptor column = {0};
    int failures = expect_int(
        mylite_catalog_read_column_by_name(database, table_id, expected.name, &column),
        MYLITE_OK,
        context
    );

    failures += expect_text(column.logical_type, expected.logical_type, context);
    failures += expect_text(column.physical_type, "INTEGER", context);
    failures += expect_true(column.is_nullable == expected.is_nullable, context);

    return failures;
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
        "%s/mylite_small_integer_types_%d_%s.mylite",
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
