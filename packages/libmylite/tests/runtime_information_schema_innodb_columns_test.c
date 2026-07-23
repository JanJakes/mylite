#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_statement {
    const char *sql;
    const char *context;
};

static int test_information_schema_innodb_column_rows(void);
static int setup_column_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_column_rows();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_column_rows(void) {
    static const char *const row_columns[] = {
        "NAME",
        "POS",
        "MTYPE",
        "PRTYPE",
        "LEN",
        "HAS_DEFAULT",
        "DEFAULT_VALUE",
    };
    static const char *const c_rows[] = {
        "id",       "0",        "6",      "1283",   "4",      "0",       NULL,      "nullable_int",
        "1",        "6",        "1027",   "4",      "0",      NULL,      "big",     "2",
        "6",        "1544",     "8",      "0",      NULL,     "c",       "3",       "13",
        "16711934", "16",       "0",      NULL,     "v",      "4",       "12",      "16711951",
        "40",       "0",        NULL,     "b",      "5",      "3",       "4130046", "3",
        "0",        NULL,       "vb",     "6",      "4",      "4129807", "5",       "0",
        NULL,       "d",        "7",      "3",      "525558", "4",       "0",       NULL,
        "f",        "8",        "9",      "1028",   "4",      "0",       NULL,      "dbl",
        "9",        "10",       "1029",   "8",      "0",      NULL,      "t",       "10",
        "5",        "16711932", "10",     "0",      NULL,     "bl",      "11",      "5",
        "4130044",  "10",       "0",      NULL,     "js",     "12",      "5",       "3015925",
        "12",       "0",        NULL,     "y",      "13",     "6",       "1549",    "1",
        "0",        NULL,       "da",     "14",     "6",      "1034",    "3",       "0",
        NULL,       "ti",       "15",     "3",      "525323", "3",       "0",       NULL,
        "dt",       "16",       "3",      "525324", "5",      "0",       NULL,      "ts",
        "17",       "3",        "525319", "4",      "0",      NULL,
    };
    static const char *const numeric_rows[] = {
        "ti",   "0",  "6", "1025",    "1", "0", NULL, "ti_u", "1",  "6", "1793",    "1", "0", NULL,
        "si",   "2",  "6", "1026",    "2", "0", NULL, "si_u", "3",  "6", "1794",    "2", "0", NULL,
        "mi",   "4",  "6", "1033",    "3", "0", NULL, "mi_u", "5",  "6", "1801",    "3", "0", NULL,
        "i",    "6",  "6", "1027",    "4", "0", NULL, "i_u",  "7",  "6", "1795",    "4", "0", NULL,
        "bi",   "8",  "6", "1032",    "8", "0", NULL, "bi_u", "9",  "6", "1800",    "8", "0", NULL,
        "de1",  "10", "3", "525558",  "3", "0", NULL, "de2",  "11", "3", "525558",  "9", "0", NULL,
        "bit1", "12", "3", "4130320", "1", "0", NULL, "bit9", "13", "3", "4130576", "2", "0", NULL,
    };
    static const char *const string_rows[] = {
        "c_null",        "0",  "13", "16711934", "16", "0", NULL,
        "c_not_null",    "1",  "13", "16712190", "16", "0", NULL,
        "vc_null",       "2",  "12", "16711695", "40", "0", NULL,
        "vc_not_null",   "3",  "12", "16711951", "40", "0", NULL,
        "nchar_col",     "4",  "13", "2162942",  "9",  "0", NULL,
        "nvarchar_col",  "5",  "12", "2162703",  "15", "0", NULL,
        "txt",           "6",  "5",  "16711932", "9",  "0", NULL,
        "medtxt",        "7",  "5",  "16711932", "11", "0", NULL,
        "longtxt",       "8",  "5",  "16711932", "12", "0", NULL,
        "bin_null",      "9",  "3",  "4130046",  "3",  "0", NULL,
        "bin_not_null",  "10", "3",  "4130302",  "3",  "0", NULL,
        "vb_null",       "11", "4",  "4129807",  "5",  "0", NULL,
        "vb_not_null",   "12", "4",  "4130063",  "5",  "0", NULL,
        "tinybl",        "13", "5",  "4130044",  "9",  "0", NULL,
        "medbl",         "14", "5",  "4130044",  "11", "0", NULL,
        "longbl",        "15", "5",  "4130044",  "12", "0", NULL,
        "enum_col",      "16", "6",  "766",      "1",  "0", NULL,
        "enum_not_null", "17", "6",  "1022",     "1",  "0", NULL,
        "set_col",       "18", "6",  "766",      "1",  "0", NULL,
        "set_not_null",  "19", "6",  "1022",     "1",  "0", NULL,
        "js_null",       "20", "5",  "3015925",  "12", "0", NULL,
        "js_not_null",   "21", "5",  "3016181",  "12", "0", NULL,
        "geom_null",     "22", "14", "1279",     "12", "0", NULL,
        "geom_not_null", "23", "14", "1535",     "12", "0", NULL,
    };
    static const char *const charset_rows[] = {
        "c_ascii",         "0", "13", "721150",  "4",  "0", NULL,
        "vc_ascii",        "1", "12", "721167",  "10", "0", NULL,
        "txt_ascii",       "2", "5",  "721148",  "10", "0", NULL,
        "c_u8bin",         "3", "13", "3014910", "16", "0", NULL,
        "vc_u8gen",        "4", "12", "2949135", "40", "0", NULL,
        "c_bin_charset",   "5", "3",  "4130046", "4",  "0", NULL,
        "vc_bin_charset",  "6", "4",  "4129807", "10", "0", NULL,
        "txt_bin_charset", "7", "5",  "4130044", "10", "0", NULL,
    };
    static const char *const alias_columns[] = {"NAME", "PRTYPE", "LEN"};
    static const char *const alias_values[] = {
        "c_ascii",
        "721150",
        "4",
        "vc_ascii",
        "721167",
        "10",
        "txt_ascii",
        "721148",
        "10",
    };
    static const char *const added_columns[] = {"NAME", "POS", "MTYPE", "PRTYPE", "LEN"};
    static const char *const added_values[] = {"added", "18", "6", "1283", "4"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_sixty_four[] = {"64"};
    static const char *const count_sixty_five[] = {"65"};
    static const char *const count_nineteen[] = {"19"};
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "AUTO_INCREMENT",
    };
    static const char *const system_table_values[] = {
        "INNODB_COLUMNS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const columns_metadata_columns[] = {
        "TABLE_NAME",
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "COLUMN_DEFAULT",
        "IS_NULLABLE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "DATETIME_PRECISION",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
    };
    static const char *const columns_metadata_values[] = {
        "INNODB_COLUMNS",
        "TABLE_ID",
        "1",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_COLUMNS",
        "NAME",
        "2",
        "",
        "NO",
        "varchar",
        "64",
        "193",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(193)",
        "select",
        "INNODB_COLUMNS",
        "POS",
        "3",
        "",
        "NO",
        "bigint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "bigint unsigned",
        "select",
        "INNODB_COLUMNS",
        "MTYPE",
        "4",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_COLUMNS",
        "PRTYPE",
        "5",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_COLUMNS",
        "LEN",
        "6",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_COLUMNS",
        "HAS_DEFAULT",
        "7",
        "",
        "NO",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "int",
        "select",
        "INNODB_COLUMNS",
        "DEFAULT_VALUE",
        "8",
        "",
        "YES",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "text",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb columns db");
    failures += setup_column_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, POS, MTYPE, PRTYPE, LEN, HAS_DEFAULT, DEFAULT_VALUE "
                   "FROM INFORMATION_SCHEMA.INNODB_COLUMNS WHERE TABLE_ID = 1 ORDER BY POS",
            .column_names = row_columns,
            .column_count = sizeof(row_columns) / sizeof(row_columns[0]),
            .values = c_rows,
            .row_count =
                sizeof(c_rows) / sizeof(c_rows[0]) / (sizeof(row_columns) / sizeof(row_columns[0])),
            .context = "innodb columns c_sample rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, POS, MTYPE, PRTYPE, LEN, HAS_DEFAULT, DEFAULT_VALUE "
                   "FROM INFORMATION_SCHEMA.INNODB_COLUMNS WHERE TABLE_ID = 2 ORDER BY POS",
            .column_names = row_columns,
            .column_count = sizeof(row_columns) / sizeof(row_columns[0]),
            .values = numeric_rows,
            .row_count = sizeof(numeric_rows) / sizeof(numeric_rows[0]) /
                         (sizeof(row_columns) / sizeof(row_columns[0])),
            .context = "innodb columns numeric rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, POS, MTYPE, PRTYPE, LEN, HAS_DEFAULT, DEFAULT_VALUE "
                   "FROM INFORMATION_SCHEMA.INNODB_COLUMNS WHERE TABLE_ID = 3 ORDER BY POS",
            .column_names = row_columns,
            .column_count = sizeof(row_columns) / sizeof(row_columns[0]),
            .values = string_rows,
            .row_count = sizeof(string_rows) / sizeof(string_rows[0]) /
                         (sizeof(row_columns) / sizeof(row_columns[0])),
            .context = "innodb columns string rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, POS, MTYPE, PRTYPE, LEN, HAS_DEFAULT, DEFAULT_VALUE "
                   "FROM INFORMATION_SCHEMA.INNODB_COLUMNS WHERE TABLE_ID = 4 ORDER BY POS",
            .column_names = row_columns,
            .column_count = sizeof(row_columns) / sizeof(row_columns[0]),
            .values = charset_rows,
            .row_count = sizeof(charset_rows) / sizeof(charset_rows[0]) /
                         (sizeof(row_columns) / sizeof(row_columns[0])),
            .context = "innodb columns charset rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_COLUMNS "
                   "WHERE TABLE_ID BETWEEN 1 AND 4",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_sixty_four,
            .row_count = 1U,
            .context = "innodb columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT c.NAME, c.PRTYPE, c.LEN FROM INFORMATION_SCHEMA.INNODB_COLUMNS AS c "
                   "WHERE c.TABLE_ID = 4 AND c.NAME LIKE '%ascii' ORDER BY c.POS",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = alias_values,
            .row_count = sizeof(alias_values) / sizeof(alias_values[0]) /
                         (sizeof(alias_columns) / sizeof(alias_columns[0])),
            .context = "innodb columns alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_columns "
                   "WHERE TABLE_ID BETWEEN 1 AND 4",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_sixty_four,
            .row_count = 1U,
            .context = "case-insensitive innodb columns table",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_COLUMNS WHERE TABLE_ID BETWEEN 1 AND 4",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_sixty_four,
            .row_count = 1U,
            .context = "unqualified innodb columns count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_COLUMNS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "innodb columns system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, "
                   "PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_COLUMNS' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb columns columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb columns row count status");

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen innodb columns db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_COLUMNS "
                   "WHERE TABLE_ID BETWEEN 1 AND 4",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_sixty_four,
            .row_count = 1U,
            .context = "reopened innodb columns count",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use app for innodb column changes",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE c_sample ADD COLUMN added INT NOT NULL",
            .context = "alter innodb column descriptor",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, POS, MTYPE, PRTYPE, LEN "
                   "FROM INFORMATION_SCHEMA.INNODB_COLUMNS "
                   "WHERE TABLE_ID = 1 AND NAME = 'added'",
            .column_names = added_columns,
            .column_count = sizeof(added_columns) / sizeof(added_columns[0]),
            .values = added_values,
            .row_count = 1U,
            .context = "innodb columns after add column",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_COLUMNS "
                   "WHERE TABLE_ID BETWEEN 1 AND 4",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_sixty_five,
            .row_count = 1U,
            .context = "innodb columns count after add column",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "RENAME TABLE c_sample TO renamed_sample",
            .context = "rename innodb column descriptor table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_COLUMNS WHERE TABLE_ID = 1",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nineteen,
            .row_count = 1U,
            .context = "innodb columns count after rename",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_column_schema(mylite_db *database) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE DATABASE app", .context = "create app schema"},
        {.sql = "USE app", .context = "use app schema"},
        {.sql = "CREATE TABLE c_sample("
                "id INT NOT NULL, nullable_int INT, big BIGINT UNSIGNED, c CHAR(4), "
                "v VARCHAR(10) NOT NULL, b BINARY(3), vb VARBINARY(5), d DECIMAL(8,2), "
                "f FLOAT, dbl DOUBLE, t TEXT, bl BLOB, js JSON, y YEAR, da DATE, "
                "ti TIME, dt DATETIME, ts TIMESTAMP NULL, PRIMARY KEY(id))",
         .context = "create column sample table"},
        {.sql = "CREATE TABLE numeric_sample("
                "ti TINYINT, ti_u TINYINT UNSIGNED NOT NULL, si SMALLINT, "
                "si_u SMALLINT UNSIGNED NOT NULL, mi MEDIUMINT, "
                "mi_u MEDIUMINT UNSIGNED NOT NULL, i INT, i_u INT UNSIGNED NOT NULL, "
                "bi BIGINT, bi_u BIGINT UNSIGNED NOT NULL, de1 DECIMAL(5,0), "
                "de2 DECIMAL(18,4), bit1 BIT(1), bit9 BIT(9) NOT NULL)",
         .context = "create numeric sample table"},
        {.sql = "CREATE TABLE string_sample("
                "c_null CHAR(4), c_not_null CHAR(4) NOT NULL, vc_null VARCHAR(10), "
                "vc_not_null VARCHAR(10) NOT NULL, nchar_col NCHAR(3), "
                "nvarchar_col NATIONAL VARCHAR(5), txt TINYTEXT, medtxt MEDIUMTEXT, "
                "longtxt LONGTEXT, bin_null BINARY(3), bin_not_null BINARY(3) NOT NULL, "
                "vb_null VARBINARY(5), vb_not_null VARBINARY(5) NOT NULL, "
                "tinybl TINYBLOB, medbl MEDIUMBLOB, longbl LONGBLOB, "
                "enum_col ENUM('a','bb'), enum_not_null ENUM('a','bb') NOT NULL, "
                "set_col SET('a','bb'), set_not_null SET('a','bb') NOT NULL, "
                "js_null JSON, js_not_null JSON NOT NULL, geom_null GEOMETRY, "
                "geom_not_null GEOMETRY NOT NULL)",
         .context = "create string sample table"},
        {.sql = "CREATE TABLE charset_sample("
                "c_ascii CHAR(4) CHARACTER SET ascii, "
                "vc_ascii VARCHAR(10) CHARACTER SET ascii NOT NULL, "
                "txt_ascii TEXT CHARACTER SET ascii, "
                "c_u8bin CHAR(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin, "
                "vc_u8gen VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci, "
                "c_bin_charset CHAR(4) CHARACTER SET binary, "
                "vc_bin_charset VARCHAR(10) CHARACTER SET binary, "
                "txt_bin_charset TEXT CHARACTER SET binary)",
         .context = "create charset sample table"},
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0]); ++index) {
        failures += expect_statement_ok(database, statements[index]);
    }
    return failures;
}

static int expect_statement_ok(mylite_db *database, struct expected_statement expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            expected.context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const status_values[] = {"0", "-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        remove(related);
    }
}
