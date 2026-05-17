#include <mylite/mylite.h>

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
    mysql_error_unknown_column = 1054,
    information_schema_collation_applicability_metadata_row_count = 2,
    information_schema_character_sets_metadata_row_count = 4,
    information_schema_collations_metadata_row_count = 7,
    information_schema_engines_metadata_row_count = 6,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_information_schema_static_catalogs(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    return test_information_schema_static_catalogs() == 0 ? 0 : 1;
}

static int test_information_schema_static_catalogs(void) {
    static const char *const engines_columns[] = {
        "ENGINE",
        "SUPPORT",
        "COMMENT",
        "TRANSACTIONS",
        "XA",
        "SAVEPOINTS",
    };
    static const char *const engines_values[] = {
        "InnoDB",
        "DEFAULT",
        "Supports transactions, row-level locking, and foreign keys",
        "YES",
        "YES",
        "YES",
    };
    static const char *const character_sets_columns[] = {
        "CHARACTER_SET_NAME",
        "DEFAULT_COLLATE_NAME",
        "DESCRIPTION",
        "MAXLEN",
    };
    static const char *const character_sets_values[] = {
        "binary",
        "binary",
        "Binary pseudo charset",
        "1",
        "ascii",
        "ascii_general_ci",
        "US ASCII",
        "1",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "UTF-8 Unicode",
        "4",
    };
    static const char *const collations_columns[] = {
        "COLLATION_NAME",
        "CHARACTER_SET_NAME",
        "ID",
        "IS_DEFAULT",
        "IS_COMPILED",
        "SORTLEN",
        "PAD_ATTRIBUTE",
    };
    static const char *const collations_values[] = {
        "binary",
        "binary",
        "63",
        "Yes",
        "Yes",
        "1",
        "NO PAD",
        "ascii_bin",
        "ascii",
        "65",
        "",
        "Yes",
        "1",
        "PAD SPACE",
        "ascii_general_ci",
        "ascii",
        "11",
        "Yes",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb4_general_ci",
        "utf8mb4",
        "45",
        "",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb4_bin",
        "utf8mb4",
        "46",
        "",
        "Yes",
        "1",
        "PAD SPACE",
        "utf8mb4_unicode_ci",
        "utf8mb4",
        "224",
        "",
        "Yes",
        "8",
        "PAD SPACE",
        "utf8mb4_unicode_520_ci",
        "utf8mb4",
        "246",
        "",
        "Yes",
        "8",
        "PAD SPACE",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "255",
        "Yes",
        "Yes",
        "0",
        "NO PAD",
        "utf8mb4_0900_bin",
        "utf8mb4",
        "309",
        "",
        "Yes",
        "1",
        "NO PAD",
    };
    static const char *const collation_applicability_columns[] = {
        "COLLATION_NAME",
        "CHARACTER_SET_NAME",
    };
    static const char *const collation_applicability_values[] = {
        "ascii_bin",
        "ascii",
        "ascii_general_ci",
        "ascii",
        "binary",
        "binary",
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "utf8mb4_0900_bin",
        "utf8mb4",
        "utf8mb4_bin",
        "utf8mb4",
        "utf8mb4_general_ci",
        "utf8mb4",
        "utf8mb4_unicode_520_ci",
        "utf8mb4",
        "utf8mb4_unicode_ci",
        "utf8mb4",
    };
    static const char *const single_engine_column[] = {"ENGINE"};
    static const char *const single_engine_value[] = {"InnoDB"};
    static const char *const single_charset_column[] = {"CHARACTER_SET_NAME"};
    static const char *const single_charset_value[] = {"utf8mb4"};
    static const char *const single_collation_column[] = {"COLLATION_NAME"};
    static const char *const single_collation_value[] = {"utf8mb4_0900_ai_ci"};
    static const char *const first_collation_value[] = {"ascii_bin"};
    static const char *const single_applicability_values[] = {"utf8mb4_bin", "utf8mb4"};
    static const char *const support_column[] = {"SUPPORT"};
    static const char *const support_value[] = {"DEFAULT"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_nine[] = {"9"};
    static const char *const count_zero[] = {"0"};
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
    };
    static const char *const system_table_values[] = {
        "information_schema", "CHARACTER_SETS", "SYSTEM VIEW", NULL, "10", NULL, "0",
        "information_schema", "COLLATIONS",     "SYSTEM VIEW", NULL, "10", NULL, "0",
        "information_schema", "ENGINES",        "SYSTEM VIEW", NULL, "10", NULL, "0",
    };
    static const char *const collation_applicability_system_table_values[] = {
        "information_schema",
        "COLLATION_CHARACTER_SET_APPLICABILITY",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
    };
    static const char *const metadata_columns[] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
        "SRS_ID",
    };
    static const char *const character_sets_metadata_values[] = {
        "CHARACTER_SET_NAME",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        NULL,
        "DEFAULT_COLLATE_NAME",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        NULL,
        "DESCRIPTION",
        "3",
        NULL,
        "NO",
        "varchar",
        "2048",
        "6144",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(2048)",
        "select",
        NULL,
        "MAXLEN",
        "4",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int unsigned",
        "select",
        NULL,
    };
    static const char *const collations_metadata_values[] = {
        "COLLATION_NAME",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        NULL,
        "CHARACTER_SET_NAME",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        NULL,
        "ID",
        "3",
        "0",
        "NO",
        "bigint",
        NULL,
        NULL,
        "20",
        "0",
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        NULL,
        "IS_DEFAULT",
        "4",
        "",
        "NO",
        "varchar",
        "3",
        "9",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        NULL,
        "IS_COMPILED",
        "5",
        "",
        "NO",
        "varchar",
        "3",
        "9",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        NULL,
        "SORTLEN",
        "6",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        "int unsigned",
        "select",
        NULL,
        "PAD_ATTRIBUTE",
        "7",
        NULL,
        "NO",
        "enum",
        "9",
        "27",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "enum('PAD SPACE','NO PAD')",
        "select",
        NULL,
    };
    static const char *const collation_applicability_metadata_values[] = {
        "COLLATION_NAME",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        NULL,
        "CHARACTER_SET_NAME",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        NULL,
    };
    static const char *const engines_metadata_values[] = {
        "ENGINE",
        "1",
        "",
        "NO",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        NULL,
        "SUPPORT",
        "2",
        "",
        "NO",
        "varchar",
        "2",
        "8",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(8)",
        "select",
        NULL,
        "COMMENT",
        "3",
        "",
        "NO",
        "varchar",
        "26",
        "80",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(80)",
        "select",
        NULL,
        "TRANSACTIONS",
        "4",
        "",
        "YES",
        "varchar",
        "1",
        "3",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        NULL,
        "XA",
        "5",
        "",
        "YES",
        "varchar",
        "1",
        "3",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        NULL,
        "SAVEPOINTS",
        "6",
        "",
        "YES",
        "varchar",
        "1",
        "3",
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        NULL,
    };
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *second_database = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &database), MYLITE_OK, "open first db");
    if (database == NULL) {
        remove_related_files(first_path);
        remove_related_files(second_path);
        return failures + 1;
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ENGINES",
            .column_names = engines_columns,
            .column_count = sizeof(engines_columns) / sizeof(engines_columns[0]),
            .values = engines_values,
            .row_count = 1U,
            .context = "engines static row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.CHARACTER_SETS",
            .column_names = character_sets_columns,
            .column_count = sizeof(character_sets_columns) / sizeof(character_sets_columns[0]),
            .values = character_sets_values,
            .row_count = sizeof(character_sets_values) / sizeof(character_sets_values[0]) /
                         (sizeof(character_sets_columns) / sizeof(character_sets_columns[0])),
            .context = "character sets static row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.COLLATIONS",
            .column_names = collations_columns,
            .column_count = sizeof(collations_columns) / sizeof(collations_columns[0]),
            .values = collations_values,
            .row_count = sizeof(collations_values) / sizeof(collations_values[0]) /
                         (sizeof(collations_columns) / sizeof(collations_columns[0])),
            .context = "collations static row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLLATION_NAME, CHARACTER_SET_NAME "
                   "FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "
                   "ORDER BY COLLATION_NAME",
            .column_names = collation_applicability_columns,
            .column_count = sizeof(collation_applicability_columns) /
                            sizeof(collation_applicability_columns[0]),
            .values = collation_applicability_values,
            .row_count = sizeof(collation_applicability_values) /
                         sizeof(collation_applicability_values[0]) /
                         (sizeof(collation_applicability_columns) /
                          sizeof(collation_applicability_columns[0])),
            .context = "collation applicability static rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'innodb'",
            .column_names = single_engine_column,
            .column_count = 1U,
            .values = single_engine_value,
            .row_count = 1U,
            .context = "engines case-insensitive predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CHARACTER_SET_NAME FROM INFORMATION_SCHEMA.CHARACTER_SETS "
                   "WHERE CHARACTER_SET_NAME = 'UTF8MB4'",
            .column_names = single_charset_column,
            .column_count = 1U,
            .values = single_charset_value,
            .row_count = 1U,
            .context = "character sets case-insensitive predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATIONS AS c "
                   "WHERE c.COLLATION_NAME = 'UTF8MB4_0900_AI_CI'",
            .column_names = single_collation_column,
            .column_count = 1U,
            .values = single_collation_value,
            .row_count = 1U,
            .context = "collations alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT a.COLLATION_NAME, a.CHARACTER_SET_NAME "
                   "FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY AS a "
                   "WHERE a.COLLATION_NAME = 'UTF8MB4_BIN'",
            .column_names = collation_applicability_columns,
            .column_count = sizeof(collation_applicability_columns) /
                            sizeof(collation_applicability_columns[0]),
            .values = single_applicability_values,
            .row_count = 1U,
            .context = "collation applicability alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SUPPORT FROM INFORMATION_SCHEMA.ENGINES ORDER BY ENGINE DESC LIMIT 1",
            .column_names = support_column,
            .column_count = 1U,
            .values = support_value,
            .row_count = 1U,
            .context = "engines ordered limited projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATIONS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "collations count star",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "collation applicability count star",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLLATION_NAME "
                   "FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "
                   "ORDER BY COLLATION_NAME LIMIT 1",
            .column_names = single_collation_column,
            .column_count = 1U,
            .values = first_collation_value,
            .row_count = 1U,
            .context = "collation applicability ordered limited projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND (TABLE_NAME = 'ENGINES' OR TABLE_NAME = 'CHARACTER_SETS' "
                   "OR TABLE_NAME = 'COLLATIONS') ORDER BY TABLE_NAME",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 3U,
            .context = "static catalog system table rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'COLLATION_CHARACTER_SET_APPLICABILITY'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = collation_applicability_system_table_values,
            .row_count = 1U,
            .context = "collation applicability system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES, SRS_ID FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'CHARACTER_SETS' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = character_sets_metadata_values,
            .row_count = information_schema_character_sets_metadata_row_count,
            .context = "character sets system column metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES, SRS_ID FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'COLLATIONS' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = collations_metadata_values,
            .row_count = information_schema_collations_metadata_row_count,
            .context = "collations system column metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES, SRS_ID FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'COLLATION_CHARACTER_SET_APPLICABILITY' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = collation_applicability_metadata_values,
            .row_count = information_schema_collation_applicability_metadata_row_count,
            .context = "collation applicability system column metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES, SRS_ID FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'ENGINES' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = engines_metadata_values,
            .row_count = information_schema_engines_metadata_row_count,
            .context = "engines system column metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND (TABLE_NAME = 'CHARACTER_SETS' "
                   "OR TABLE_NAME = 'COLLATION_CHARACTER_SET_APPLICABILITY' "
                   "OR TABLE_NAME = 'COLLATIONS' OR TABLE_NAME = 'ENGINES') "
                   "AND (COLUMN_KEY IS NULL OR COLUMN_KEY <> '' "
                   "OR EXTRA IS NULL OR EXTRA <> '' "
                   "OR COLUMN_COMMENT IS NULL OR COLUMN_COMMENT <> '' "
                   "OR GENERATION_EXPRESSION IS NULL OR GENERATION_EXPRESSION <> '')",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "static catalog empty system column fields",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.ENGINES",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES ORDER BY nope",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'order clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT COLLATION_NAME FROM "
                   "INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "
                   "WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT COLLATION_NAME FROM "
                   "INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "
                   "ORDER BY nope",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'order clause'",
        }
    );

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(first_path, &database), MYLITE_OK, "reopen first db");
    if (database != NULL) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql =
                    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY",
                .column_names = count_column,
                .column_count = 1U,
                .values = count_nine,
                .row_count = 1U,
                .context = "reopened collation applicability static rows",
            }
        );
    }
    failures += expect_int(mylite_open(second_path, &second_database), MYLITE_OK, "open second db");
    if (database != NULL && second_database != NULL) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = "SELECT ENGINE FROM INFORMATION_SCHEMA.ENGINES",
                .column_names = single_engine_column,
                .column_count = 1U,
                .values = single_engine_value,
                .row_count = 1U,
                .context = "first independent handle static catalog row",
            }
        );
        failures += expect_query(
            second_database,
            (struct expected_query){
                .sql =
                    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY",
                .column_names = count_column,
                .column_count = 1U,
                .values = count_nine,
                .row_count = 1U,
                .context = "second independent handle collation applicability rows",
            }
        );
    }

    mylite_close(database);
    mylite_close(second_database);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected query OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "%s: expected error, got %d\n", expected.sql, rc);
        failures += 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    failures += expect_size(mylite_result_column_count(result), 0U, expected.sql);
    mylite_result_free(result);
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
        "%s/mylite_information_schema_static_catalogs_%d_%s.mylite",
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(stderr, "%s: expected %s, got %s\n", context, expected, actual);
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
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
