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
    show_create_sql_capacity = 256,
    show_create_column_count = 2,
    decimal_base = 10,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_character_set = 1115,
    mysql_error_invalid_default = 1067,
    mysql_error_collation_not_valid_for_character_set = 1253,
    mysql_error_unknown_collation = 1273,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_single_row_result {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
};

struct create_form {
    const char *create_sql;
    const char *show_sql;
    const char *table_name;
    const char *expected_collation;
    const char *context;
    int is_temporary;
};

struct create_table_statement {
    const char *sql;
    const char *context;
};

struct expected_show_create_text {
    const char *show_sql;
    const char *table_name;
    const char *create_sql;
    const char *context;
};

static const char *const show_create_columns[show_create_column_count] = {
    "Table",
    "Create Table",
};

static int test_charset_collation_create_forms_persistence_and_preamble(void);
static int test_table_default_binary_charset_inheritance(void);
static int test_charset_collation_diagnostics(void);
static int test_independent_charset_collation_handles(void);
static int expect_show_create_single_int(mylite_db *database, struct create_form expected);
static int expect_show_create_text(mylite_db *database, struct expected_show_create_text expected);
static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected,
    const char *context
);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_create_table_ok(mylite_db *database, struct create_table_statement statement);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int execute_error_with_length(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct expected_sql_error expected,
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

    failures += test_charset_collation_create_forms_persistence_and_preamble();
    failures += test_table_default_binary_charset_inheritance();
    failures += test_charset_collation_diagnostics();
    failures += test_independent_charset_collation_handles();

    return failures == 0 ? 0 : 1;
}

static int test_charset_collation_create_forms_persistence_and_preamble(void) {
    static const char *const select_columns[] = {"id"};
    static const char *const select_values[] = {"1"};
    static const char *const table_collation_columns[] = {"TABLE_COLLATION"};
    static const char *const unicode_table_collation[] = {"utf8mb4_unicode_ci"};
    static const struct create_form forms[] = {
        {
            .create_sql = "CREATE TABLE default_charset (id INT) DEFAULT CHARSET=utf8mb4",
            .show_sql = "SHOW CREATE TABLE default_charset",
            .table_name = "default_charset",
            .context = "default charset",
        },
        {
            .create_sql =
                "CREATE TABLE default_character_set (id INT) DEFAULT CHARACTER SET utf8mb4",
            .show_sql = "SHOW CREATE TABLE default_character_set",
            .table_name = "default_character_set",
            .context = "default character set",
        },
        {
            .create_sql = "CREATE TABLE character_set_equal (id INT) CHARACTER SET=utf8mb4",
            .show_sql = "SHOW CREATE TABLE character_set_equal",
            .table_name = "character_set_equal",
            .context = "character set equal",
        },
        {
            .create_sql = "CREATE TABLE charset_space (id INT) CHARSET utf8mb4",
            .show_sql = "SHOW CREATE TABLE charset_space",
            .table_name = "charset_space",
            .context = "charset space",
        },
        {
            .create_sql = "CREATE TABLE collate_only (id INT) COLLATE=utf8mb4_0900_ai_ci",
            .show_sql = "SHOW CREATE TABLE collate_only",
            .table_name = "collate_only",
            .context = "collate only",
        },
        {
            .create_sql = "CREATE TABLE legacy_general (id INT) COLLATE=utf8mb4_general_ci",
            .show_sql = "SHOW CREATE TABLE legacy_general",
            .table_name = "legacy_general",
            .expected_collation = "utf8mb4_general_ci",
            .context = "legacy general collation",
        },
        {
            .create_sql = "CREATE TABLE legacy_bin (id INT) COLLATE=utf8mb4_bin",
            .show_sql = "SHOW CREATE TABLE legacy_bin",
            .table_name = "legacy_bin",
            .expected_collation = "utf8mb4_bin",
            .context = "legacy bin collation",
        },
        {
            .create_sql = "CREATE TABLE bin_0900 (id INT) DEFAULT CHARSET=utf8mb4 "
                          "COLLATE=utf8mb4_0900_bin",
            .show_sql = "SHOW CREATE TABLE bin_0900",
            .table_name = "bin_0900",
            .expected_collation = "utf8mb4_0900_bin",
            .context = "0900 bin collation",
        },
        {
            .create_sql = "CREATE TEMPORARY TABLE temp_bin_0900 (id INT) "
                          "DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin",
            .show_sql = "SHOW CREATE TABLE temp_bin_0900",
            .table_name = "temp_bin_0900",
            .expected_collation = "utf8mb4_0900_bin",
            .context = "temporary 0900 bin collation",
            .is_temporary = 1,
        },
        {
            .create_sql = "CREATE TABLE legacy_unicode (id INT) DEFAULT CHARSET=utf8mb4 "
                          "COLLATE=utf8mb4_unicode_ci",
            .show_sql = "SHOW CREATE TABLE legacy_unicode",
            .table_name = "legacy_unicode",
            .expected_collation = "utf8mb4_unicode_ci",
            .context = "legacy unicode collation",
        },
        {
            .create_sql = "CREATE TABLE legacy_unicode_520 (id INT) COLLATE=utf8mb4_unicode_520_ci",
            .show_sql = "SHOW CREATE TABLE legacy_unicode_520",
            .table_name = "legacy_unicode_520",
            .expected_collation = "utf8mb4_unicode_520_ci",
            .context = "legacy unicode 520 collation",
        },
        {
            .create_sql = "CREATE TABLE repeated_collation (id INT) COLLATE=utf8mb4_general_ci "
                          "COLLATE=utf8mb4_unicode_ci",
            .show_sql = "SHOW CREATE TABLE repeated_collation",
            .table_name = "repeated_collation",
            .expected_collation = "utf8mb4_unicode_ci",
            .context = "repeated collation last wins",
        },
        {
            .create_sql = "CREATE TABLE default_collate (id INT) DEFAULT COLLATE "
                          "utf8mb4_0900_ai_ci",
            .show_sql = "SHOW CREATE TABLE default_collate",
            .table_name = "default_collate",
            .context = "default collate",
        },
        {
            .create_sql = "CREATE TABLE string_names (id INT) DEFAULT CHARSET='utf8mb4' "
                          "COLLATE=\"utf8mb4_0900_ai_ci\"",
            .show_sql = "SHOW CREATE TABLE string_names",
            .table_name = "string_names",
            .context = "string option names",
        },
        {
            .create_sql = "CREATE TABLE quoted_names (id INT) DEFAULT CHARSET=`utf8mb4` "
                          "COLLATE=`utf8mb4_0900_ai_ci`",
            .show_sql = "SHOW CREATE TABLE quoted_names",
            .table_name = "quoted_names",
            .context = "quoted option names",
        },
        {
            .create_sql = "CREATE TABLE uppercase_names (id INT) DEFAULT CHARSET=UTF8MB4 "
                          "COLLATE=UTF8MB4_0900_AI_CI",
            .show_sql = "SHOW CREATE TABLE uppercase_names",
            .table_name = "uppercase_names",
            .context = "uppercase option names",
        },
        {
            .create_sql = "CREATE TABLE engine_charset (id INT) ENGINE=InnoDB DEFAULT "
                          "CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
            .show_sql = "SHOW CREATE TABLE engine_charset",
            .table_name = "engine_charset",
            .context = "engine charset collation",
        },
        {
            .create_sql = "CREATE TABLE app.qualified_options (id INT) "
                          "COLLATE=utf8mb4_0900_ai_ci DEFAULT CHARSET=utf8mb4 ENGINE=InnoDB",
            .show_sql = "SHOW CREATE TABLE app.qualified_options",
            .table_name = "qualified_options",
            .context = "qualified reverse options",
        },
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "forms") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open forms database");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_error(
        database,
        "CREATE TABLE no_schema (id INT) DEFAULT CHARSET=utf8mb4",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_statement_ok(database, "USE app");

    for (size_t form_index = 0U; form_index < sizeof(forms) / sizeof(forms[0]); ++form_index) {
        failures += execute_create_table_ok(
            database,
            (struct create_table_statement){
                .sql = forms[form_index].create_sql,
                .context = forms[form_index].context,
            }
        );
        failures += expect_show_create_single_int(database, forms[form_index]);
    }
    failures += execute_create_table_ok(
        database,
        (struct create_table_statement){
            .sql = "CREATE TABLE ascii_charset (id INT) DEFAULT CHARSET=ascii",
            .context = "ascii default charset",
        }
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE ascii_charset",
            .table_name = "ascii_charset",
            .create_sql = "CREATE TABLE `ascii_charset` (\n"
                          "  `id` int DEFAULT NULL\n"
                          ") ENGINE=InnoDB DEFAULT CHARSET=ascii",
            .context = "ascii default charset show create",
        }
    );
    failures += execute_create_table_ok(
        database,
        (struct create_table_statement){
            .sql = "CREATE TABLE ascii_collate (id INT) DEFAULT COLLATE=ascii_bin",
            .context = "ascii default collate",
        }
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE ascii_collate",
            .table_name = "ascii_collate",
            .create_sql = "CREATE TABLE `ascii_collate` (\n"
                          "  `id` int DEFAULT NULL\n"
                          ") ENGINE=InnoDB DEFAULT CHARSET=ascii COLLATE=ascii_bin",
            .context = "ascii default collate show create",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'legacy_unicode'",
        (struct expected_single_row_result){
            .columns = table_collation_columns,
            .values = unicode_table_collation,
            .column_count = sizeof(table_collation_columns) / sizeof(table_collation_columns[0]),
        },
        "information schema table collation"
    );
    failures += execute_create_table_ok(
        database,
        (struct create_table_statement){
            .sql = "CREATE TABLE row_count_status (id INT) DEFAULT CHARSET=utf8mb4 "
                   "COLLATE=utf8mb4_0900_ai_ci",
            .context = "row count status create",
        }
    );
    failures += expect_row_count(database, 0, "row count after create");

    failures += execute_statement_ok(database, "INSERT INTO engine_charset VALUES (1)");
    failures += expect_single_row_result(
        database,
        "SELECT id FROM engine_charset",
        (struct expected_single_row_result){
            .columns = select_columns,
            .values = select_values,
            .column_count = sizeof(select_columns) / sizeof(select_columns[0]),
        },
        "row from explicit charset table"
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after explicit charset create"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen forms database");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_create_single_int(
        database,
        (struct create_form){
            .show_sql = "SHOW CREATE TABLE engine_charset",
            .table_name = "engine_charset",
            .context = "reopened show create explicit charset",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT id FROM engine_charset",
        (struct expected_single_row_result){
            .columns = select_columns,
            .values = select_values,
            .column_count = sizeof(select_columns) / sizeof(select_columns[0]),
        },
        "reopened row from explicit charset table"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_default_binary_charset_inheritance(void) {
    static const char binary_default_create[] = "CREATE TABLE `binary_default` (\n"
                                                "  `id` int DEFAULT NULL,\n"
                                                "  `v` varbinary(10) DEFAULT NULL,\n"
                                                "  `c` binary(3) DEFAULT NULL,\n"
                                                "  `txt` blob,\n"
                                                "  `tiny` tinyblob,\n"
                                                "  `med` mediumblob,\n"
                                                "  `lon` longblob\n"
                                                ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char collate_binary_create[] = "CREATE TABLE `collate_binary` (\n"
                                                "  `v` varbinary(5) DEFAULT NULL,\n"
                                                "  `txt` blob\n"
                                                ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char both_binary_create[] = "CREATE TABLE `both_binary` (\n"
                                             "  `v` varbinary(5) DEFAULT NULL\n"
                                             ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char binary_override_create[] =
        "CREATE TABLE `binary_override` (\n"
        "  `v` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL,\n"
        "  `c` char(2) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL,\n"
        "  `txt` blob\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char temp_binary_create[] = "CREATE TEMPORARY TABLE `temp_binary` (\n"
                                             "  `v` varbinary(3) DEFAULT NULL,\n"
                                             "  `c` binary(2) DEFAULT NULL\n"
                                             ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char enum_set_binary_create[] = "CREATE TABLE `enum_set_binary` (\n"
                                                 "  `e` enum('a','b') DEFAULT NULL,\n"
                                                 "  `s` set('x','y') DEFAULT NULL\n"
                                                 ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char like_binary_create[] = "CREATE TABLE `like_binary` (\n"
                                             "  `id` int DEFAULT NULL,\n"
                                             "  `v` varbinary(10) DEFAULT NULL,\n"
                                             "  `c` binary(3) DEFAULT NULL,\n"
                                             "  `txt` blob,\n"
                                             "  `tiny` tinyblob,\n"
                                             "  `med` mediumblob,\n"
                                             "  `lon` longblob\n"
                                             ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char binary_key_create[] = "CREATE TABLE `binary_key` (\n"
                                            "  `v` varbinary(10) DEFAULT NULL,\n"
                                            "  KEY `v_idx` (`v`)\n"
                                            ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char *const table_collation_columns[] = {"TABLE_COLLATION"};
    static const char *const binary_table_collation[] = {"binary"};
    static const char *const binary_key_stat_columns[] = {
        "INDEX_NAME",
        "COLUMN_NAME",
        "SUB_PART",
    };
    static const char *const binary_key_stat_values[] = {"v_idx", "v", NULL};
    static const char *const v_metadata_columns[] = {
        "DATA_TYPE",
        "COLUMN_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
    };
    static const char *const v_metadata_values[] = {
        "varbinary",
        "varbinary(10)",
        NULL,
        NULL,
        "10",
        "10",
    };
    static const char *const text_metadata_values[] = {
        "blob",
        "blob",
        NULL,
        NULL,
        "65535",
        "65535",
    };
    static const char *const show_full_columns[] = {
        "Field",
        "Type",
        "Collation",
        "Null",
        "Key",
        "Default",
        "Extra",
        "Privileges",
        "Comment",
    };
    static const char *const show_full_v_values[] = {
        "v",
        "varbinary(10)",
        NULL,
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const data_columns[] = {
        "HEX(v)",
        "LENGTH(v)",
        "HEX(c)",
        "LENGTH(c)",
        "HEX(txt)",
        "LENGTH(txt)",
        "HEX(tiny)",
        "HEX(med)",
        "HEX(lon)",
    };
    static const char *const data_values[] = {
        "6162",
        "2",
        "787900",
        "3",
        "68656C6C6F",
        "5",
        "7469",
        "6D6564",
        "6C6F6E67",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "binary-table-default") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open binary default file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE binary_default("
        "id INT, v VARCHAR(10), c CHAR(3), txt TEXT, tiny TINYTEXT, "
        "med MEDIUMTEXT, lon LONGTEXT"
        ") DEFAULT CHARSET=binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE binary_default",
            .table_name = "binary_default",
            .create_sql = binary_default_create,
            .context = "binary default show create",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'binary_default'",
        (struct expected_single_row_result){
            .columns = table_collation_columns,
            .values = binary_table_collation,
            .column_count = sizeof(table_collation_columns) / sizeof(table_collation_columns[0]),
        },
        "binary default table collation"
    );
    failures += expect_single_row_result(
        database,
        "SELECT DATA_TYPE, COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, "
        "CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH "
        "FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'binary_default' AND COLUMN_NAME = 'v'",
        (struct expected_single_row_result){
            .columns = v_metadata_columns,
            .values = v_metadata_values,
            .column_count = sizeof(v_metadata_columns) / sizeof(v_metadata_columns[0]),
        },
        "binary default varbinary information_schema"
    );
    failures += expect_single_row_result(
        database,
        "SELECT DATA_TYPE, COLUMN_TYPE, CHARACTER_SET_NAME, COLLATION_NAME, "
        "CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH "
        "FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'binary_default' AND COLUMN_NAME = 'txt'",
        (struct expected_single_row_result){
            .columns = v_metadata_columns,
            .values = text_metadata_values,
            .column_count = sizeof(v_metadata_columns) / sizeof(v_metadata_columns[0]),
        },
        "binary default blob information_schema"
    );
    failures += expect_single_row_result(
        database,
        "SHOW FULL COLUMNS FROM binary_default LIKE 'v'",
        (struct expected_single_row_result){
            .columns = show_full_columns,
            .values = show_full_v_values,
            .column_count = sizeof(show_full_columns) / sizeof(show_full_columns[0]),
        },
        "binary default show full columns"
    );
    failures += execute_statement_ok(
        database,
        "INSERT INTO binary_default VALUES(1, 'ab', 'xy', 'hello', 'ti', 'med', 'long')"
    );
    failures += expect_single_row_result(
        database,
        "SELECT HEX(v), LENGTH(v), HEX(c), LENGTH(c), HEX(txt), LENGTH(txt), "
        "HEX(tiny), HEX(med), HEX(lon) FROM binary_default",
        (struct expected_single_row_result){
            .columns = data_columns,
            .values = data_values,
            .column_count = sizeof(data_columns) / sizeof(data_columns[0]),
        },
        "binary default stored values"
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE binary_key (v VARCHAR(10), KEY v_idx(v)) DEFAULT CHARSET=binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE binary_key",
            .table_name = "binary_key",
            .create_sql = binary_key_create,
            .context = "binary default full-column key show create",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT INDEX_NAME, COLUMN_NAME, SUB_PART FROM INFORMATION_SCHEMA.STATISTICS "
        "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'binary_key'",
        (struct expected_single_row_result){
            .columns = binary_key_stat_columns,
            .values = binary_key_stat_values,
            .column_count = sizeof(binary_key_stat_columns) / sizeof(binary_key_stat_columns[0]),
        },
        "binary default full-column key information_schema"
    );

    failures += execute_statement_ok(
        database,
        "CREATE TABLE collate_binary(v VARCHAR(5), txt TEXT) COLLATE=binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE collate_binary",
            .table_name = "collate_binary",
            .create_sql = collate_binary_create,
            .context = "binary collate-only show create",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE both_binary(v VARCHAR(5)) DEFAULT CHARACTER SET binary COLLATE binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE both_binary",
            .table_name = "both_binary",
            .create_sql = both_binary_create,
            .context = "binary charset and collation show create",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE binary_override("
        "v VARCHAR(10) CHARACTER SET utf8mb4, "
        "c CHAR(2) COLLATE utf8mb4_bin, "
        "txt TEXT"
        ") DEFAULT CHARSET=binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE binary_override",
            .table_name = "binary_override",
            .create_sql = binary_override_create,
            .context = "binary table default explicit column override",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TEMPORARY TABLE temp_binary(v VARCHAR(3), c CHAR(2)) DEFAULT CHARSET=binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE temp_binary",
            .table_name = "temp_binary",
            .create_sql = temp_binary_create,
            .context = "temporary binary default show create",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE enum_set_binary(e ENUM('a','b'), s SET('x','y')) DEFAULT CHARSET=binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE enum_set_binary",
            .table_name = "enum_set_binary",
            .create_sql = enum_set_binary_create,
            .context = "enum set binary default show create",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE like_binary LIKE binary_default");
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE like_binary",
            .table_name = "like_binary",
            .create_sql = like_binary_create,
            .context = "binary default create like",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "binary table default preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen binary default file");
    failures += execute_statement_ok(database, "USE app");
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE binary_default",
            .table_name = "binary_default",
            .create_sql = binary_default_create,
            .context = "reopened binary default show create",
        }
    );
    failures += expect_single_row_result(
        database,
        "SELECT HEX(v), LENGTH(v), HEX(c), LENGTH(c), HEX(txt), LENGTH(txt), "
        "HEX(tiny), HEX(med), HEX(lon) FROM binary_default",
        (struct expected_single_row_result){
            .columns = data_columns,
            .values = data_values,
            .column_count = sizeof(data_columns) / sizeof(data_columns[0]),
        },
        "reopened binary default stored values"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_charset_collation_diagnostics(void) {
    static const char raw_nul_charset_sql[] =
        "CREATE TABLE raw_nul_charset (id INT) DEFAULT CHARSET='utf8"
        "\0"
        "mb4'";
    static const char raw_nul_collation_sql[] =
        "CREATE TABLE raw_nul_collation (id INT) COLLATE=`utf8mb4"
        "\0"
        "_0900_ai_ci`";
    static const char binary_default_value_create[] = "CREATE TABLE `binary_default_value` (\n"
                                                      "  `v` varbinary(10) DEFAULT 'ab'\n"
                                                      ") ENGINE=InnoDB DEFAULT CHARSET=binary";
    static const char *const binary_default_value_columns[] = {
        "HEX(v)",
        "LENGTH(v)",
    };
    static const char *const binary_default_value_values[] = {
        "6162",
        "2",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics database");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");

    failures += execute_error(
        database,
        "CREATE TABLE unknown_charset (id INT) DEFAULT CHARSET=nosuch_charset",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'nosuch_charset'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unsupported_charset (id INT) DEFAULT CHARSET=latin2",
        (struct expected_sql_error){
            .code = mysql_error_unknown_character_set,
            .sqlstate = "42000",
            .message_part = "Unknown character set: 'latin2'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE mismatched_charset (id INT) DEFAULT CHARSET=latin1 "
        "COLLATE=utf8mb4_unicode_ci",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part =
                "COLLATION 'utf8mb4_unicode_ci' is not valid for CHARACTER SET 'latin1'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE mismatched_collation (id INT) DEFAULT CHARSET=utf8mb4 "
        "COLLATE=latin1_swedish_ci",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part =
                "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE unknown_collation (id INT) COLLATE=nosuch_collation",
        (struct expected_sql_error){
            .code = mysql_error_unknown_collation,
            .sqlstate = "HY000",
            .message_part = "Unknown collation: 'nosuch_collation'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE binary_utf8_mismatch (id INT) DEFAULT CHARSET=binary "
        "COLLATE=utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'binary'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE utf8_binary_mismatch (id INT) DEFAULT CHARSET=utf8mb4 COLLATE=binary",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'binary' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE ascii_utf8_mismatch (id INT) DEFAULT CHARSET=ascii COLLATE=utf8mb4_bin",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'utf8mb4_bin' is not valid for CHARACTER SET 'ascii'",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE utf8_ascii_mismatch (id INT) DEFAULT CHARSET=utf8mb4 COLLATE=ascii_bin",
        (struct expected_sql_error){
            .code = mysql_error_collation_not_valid_for_character_set,
            .sqlstate = "42000",
            .message_part = "COLLATION 'ascii_bin' is not valid for CHARACTER SET 'utf8mb4'",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE binary_default_value (v VARCHAR(10) DEFAULT 'ab') DEFAULT CHARSET=binary"
    );
    failures += expect_show_create_text(
        database,
        (struct expected_show_create_text){
            .show_sql = "SHOW CREATE TABLE binary_default_value",
            .table_name = "binary_default_value",
            .create_sql = binary_default_value_create,
            .context = "binary table default inherited column default",
        }
    );
    failures += execute_statement_ok(database, "INSERT INTO binary_default_value () VALUES ()");
    failures += expect_single_row_result(
        database,
        "SELECT HEX(v), LENGTH(v) FROM binary_default_value",
        (struct expected_single_row_result){
            .columns = binary_default_value_columns,
            .values = binary_default_value_values,
            .column_count =
                sizeof(binary_default_value_columns) / sizeof(binary_default_value_columns[0]),
        },
        "binary table default inherited default bytes"
    );
    failures += execute_error(
        database,
        "ALTER TABLE legacy_unicode DEFAULT CHARSET=binary",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE charset_default (id INT) DEFAULT CHARSET=DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE collate_default (id INT) COLLATE=DEFAULT",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nul_charset (id INT) DEFAULT CHARSET='utf8\\0mb4'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table character set names do not support NUL bytes",
        }
    );
    failures += execute_error(
        database,
        "CREATE TABLE nul_collation (id INT) COLLATE='utf8mb4\\0_0900_ai_ci'",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table collation names do not support NUL bytes",
        }
    );
    failures += execute_error_with_length(
        database,
        raw_nul_charset_sql,
        sizeof(raw_nul_charset_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table character set names do not support NUL bytes",
        },
        "raw NUL charset"
    );
    failures += execute_error_with_length(
        database,
        raw_nul_collation_sql,
        sizeof(raw_nul_collation_sql) - 1U,
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "table collation names do not support NUL bytes",
        },
        "raw NUL collation"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_charset_collation_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent-a") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent-b") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");

    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures +=
        execute_statement_ok(first, "CREATE TABLE only_first (id INT) DEFAULT CHARSET=utf8mb4");

    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures +=
        execute_statement_ok(second, "CREATE TABLE only_second (id INT) COLLATE=utf8mb4_bin");

    failures += expect_show_create_single_int(
        first,
        (struct create_form){
            .show_sql = "SHOW CREATE TABLE only_first",
            .table_name = "only_first",
            .context = "first handle charset table",
        }
    );
    failures += expect_show_create_single_int(
        second,
        (struct create_form){
            .show_sql = "SHOW CREATE TABLE only_second",
            .table_name = "only_second",
            .expected_collation = "utf8mb4_bin",
            .context = "second handle collation table",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int expect_show_create_single_int(mylite_db *database, struct create_form expected) {
    char create_sql[show_create_sql_capacity];
    const char *collation =
        expected.expected_collation == NULL ? "utf8mb4_0900_ai_ci" : expected.expected_collation;
    int written = snprintf(
        create_sql,
        sizeof(create_sql),
        "CREATE %sTABLE `%s` (\n"
        "  `id` int DEFAULT NULL\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=%s",
        expected.is_temporary != 0 ? "TEMPORARY " : "",
        expected.table_name,
        collation
    );
    const char *const values[show_create_column_count] = {expected.table_name, create_sql};

    if (written < 0 || (size_t)written >= sizeof(create_sql)) {
        fprintf(stderr, "%s: failed to build expected SHOW CREATE TABLE text\n", expected.context);
        return 1;
    }

    return expect_single_row_result(
        database,
        expected.show_sql,
        (struct expected_single_row_result){
            .columns = show_create_columns,
            .values = values,
            .column_count = show_create_column_count,
        },
        expected.context
    );
}

static int expect_show_create_text(mylite_db *database, struct expected_show_create_text expected) {
    const char *const values[show_create_column_count] = {
        expected.table_name,
        expected.create_sql,
    };

    return expect_single_row_result(
        database,
        expected.show_sql,
        (struct expected_single_row_result){
            .columns = show_create_columns,
            .values = values,
            .column_count = show_create_column_count,
        },
        expected.context
    );
}

static int expect_single_row_result(
    mylite_db *database,
    const char *sql,
    struct expected_single_row_result expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), expected.column_count, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, column_index),
            expected.values[column_index],
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, "SELECT ROW_COUNT()", &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    if (mylite_result_value_text(result, 0U, 0U) == NULL) {
        fprintf(stderr, "%s: expected row count value\n", context);
        failures += 1;
    } else {
        failures += expect_int64(
            strtoll(mylite_result_value_text(result, 0U, 0U), NULL, decimal_base),
            expected,
            context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int execute_create_table_ok(mylite_db *database, struct create_table_statement statement) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, statement.sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), 0U, statement.context);
    failures += expect_size(mylite_result_row_count(result), 0U, statement.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, statement.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, statement.context);

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
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    return execute_error_with_length(database, sql, strlen(sql), expected, sql);
}

static int execute_error_with_length(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    struct expected_sql_error expected,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_size, &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected error %d/%s, got success\n",
            context,
            expected.code,
            expected.sqlstate
        );
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, context);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, context);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_table_charset_collation_%s_%d.mylite",
        name,
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path for %s\n", name);
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
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        remove(related_path);
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
        fprintf(stderr, "%s: failed to seek to %ld\n", path, offset);
        failures += 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read %zu bytes\n", path, size);
        failures += 1;
    }
    fclose(file);
    return failures;
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
    fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected text [%s], got [%s]\n",
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

    fprintf(
        stderr,
        "%s: expected text [%s] to contain [%s]\n",
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
    fprintf(stderr, "%s: byte buffer differs\n", context);
    return 1;
}
