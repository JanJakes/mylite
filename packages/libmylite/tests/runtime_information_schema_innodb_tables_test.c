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

static int test_information_schema_innodb_table_rows(void);
static int setup_table_schema(mylite_db *database);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_information_schema_innodb_table_rows();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_table_rows(void) {
    static const char *const table_columns[] = {
        "TABLE_ID",
        "NAME",
        "FLAG",
        "N_COLS",
        "SPACE",
        "ROW_FORMAT",
        "ZIP_PAGE_SIZE",
        "SPACE_TYPE",
        "INSTANT_COLS",
        "TOTAL_ROW_VERSIONS",
    };
    static const char *const table_values[] = {
        "4", "app/t_compact",    "1",  "4", "0", "Compact",    "0",    "Single", "0", "0",
        "6", "app/t_compressed", "41", "4", "0", "Compressed", "8192", "Single", "0", "0",
        "2", "app/t_no_pk",      "33", "5", "0", "Dynamic",    "0",    "Single", "0", "0",
        "1", "app/t_primary",    "33", "5", "0", "Dynamic",    "0",    "Single", "0", "0",
        "5", "app/t_redundant",  "0",  "4", "0", "Redundant",  "0",    "Single", "0", "0",
        "3", "app/t_unique",     "33", "5", "0", "Dynamic",    "0",    "Single", "0", "0",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_six[] = {"6"};
    static const char *const count_five[] = {"5"};
    static const char *const alias_columns[] = {"NAME", "FLAG", "ZIP_PAGE_SIZE"};
    static const char *const alias_values[] = {
        "app/t_compact",
        "1",
        "0",
        "app/t_compressed",
        "41",
        "8192",
    };
    static const char *const n_cols_columns[] = {"NAME", "N_COLS"};
    static const char *const n_cols_values[] = {"app/t_no_pk", "6"};
    static const char *const renamed_columns[] = {"TABLE_ID", "NAME"};
    static const char *const renamed_values[] = {"3", "app/t_renamed"};
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
        "INNODB_TABLES",
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
        "INNODB_TABLES",
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
        "INNODB_TABLES",
        "NAME",
        "2",
        "",
        "NO",
        "varchar",
        "218",
        "655",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(655)",
        "select",
        "INNODB_TABLES",
        "FLAG",
        "3",
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
        "INNODB_TABLES",
        "N_COLS",
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
        "INNODB_TABLES",
        "SPACE",
        "5",
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
        "bigint",
        "select",
        "INNODB_TABLES",
        "ROW_FORMAT",
        "6",
        "",
        "YES",
        "varchar",
        "4",
        "12",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(12)",
        "select",
        "INNODB_TABLES",
        "ZIP_PAGE_SIZE",
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
        "int unsigned",
        "select",
        "INNODB_TABLES",
        "SPACE_TYPE",
        "8",
        "",
        "YES",
        "varchar",
        "3",
        "10",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(10)",
        "select",
        "INNODB_TABLES",
        "INSTANT_COLS",
        "9",
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
        "INNODB_TABLES",
        "TOTAL_ROW_VERSIONS",
        "10",
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
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "rows") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open innodb tables db");
    failures += setup_table_schema(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_ID, NAME, FLAG, N_COLS, SPACE, ROW_FORMAT, ZIP_PAGE_SIZE, "
                   "SPACE_TYPE, INSTANT_COLS, TOTAL_ROW_VERSIONS "
                   "FROM INFORMATION_SCHEMA.INNODB_TABLES ORDER BY NAME",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = table_values,
            .row_count = sizeof(table_values) / sizeof(table_values[0]) /
                         (sizeof(table_columns) / sizeof(table_columns[0])),
            .context = "innodb table rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "innodb table count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.NAME, t.FLAG, t.ZIP_PAGE_SIZE "
                   "FROM INFORMATION_SCHEMA.INNODB_TABLES AS t "
                   "WHERE t.ROW_FORMAT IN ('Compact', 'Compressed') ORDER BY t.NAME",
            .column_names = alias_columns,
            .column_count = sizeof(alias_columns) / sizeof(alias_columns[0]),
            .values = alias_values,
            .row_count = sizeof(alias_values) / sizeof(alias_values[0]) /
                         (sizeof(alias_columns) / sizeof(alias_columns[0])),
            .context = "innodb tables alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_tables WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "case-insensitive innodb tables table",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_TABLES WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "unqualified innodb tables count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TABLES'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "innodb tables system table row",
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
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TABLES' "
                   "ORDER BY TABLE_NAME",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb tables columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb tables row count status");

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen innodb tables db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_six,
            .row_count = 1U,
            .context = "reopened innodb table count",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE app",
            .context = "use app for innodb table changes",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "ALTER TABLE t_no_pk ADD COLUMN c INT",
            .context = "alter innodb table descriptor",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT NAME, N_COLS FROM INFORMATION_SCHEMA.INNODB_TABLES "
                   "WHERE NAME = 'app/t_no_pk'",
            .column_names = n_cols_columns,
            .column_count = sizeof(n_cols_columns) / sizeof(n_cols_columns[0]),
            .values = n_cols_values,
            .row_count = 1U,
            .context = "innodb table rows after add column",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "RENAME TABLE t_unique TO t_renamed",
            .context = "rename innodb table descriptor",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_ID, NAME FROM INFORMATION_SCHEMA.INNODB_TABLES "
                   "WHERE TABLE_ID = 3",
            .column_names = renamed_columns,
            .column_count = sizeof(renamed_columns) / sizeof(renamed_columns[0]),
            .values = renamed_values,
            .row_count = 1U,
            .context = "innodb table rows after rename",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "DROP TABLE t_compact",
            .context = "drop innodb table descriptor",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE 'app/%'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_five,
            .row_count = 1U,
            .context = "innodb table count after drop",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int setup_table_schema(mylite_db *database) {
    static const struct expected_statement statements[] = {
        {.sql = "CREATE DATABASE app", .context = "create app schema"},
        {.sql = "USE app", .context = "use app schema"},
        {.sql = "CREATE TABLE t_primary(id INT NOT NULL, v VARCHAR(10), PRIMARY KEY(id))",
         .context = "create primary table"},
        {.sql = "CREATE TABLE t_no_pk(a INT, b INT)", .context = "create no-primary table"},
        {.sql = "CREATE TABLE t_unique(a INT NOT NULL, b INT NOT NULL, UNIQUE KEY uq_a(a))",
         .context = "create unique table"},
        {.sql = "CREATE TABLE t_compact(a INT) ROW_FORMAT=COMPACT",
         .context = "create compact table"},
        {.sql = "CREATE TABLE t_redundant(a INT) ROW_FORMAT=REDUNDANT",
         .context = "create redundant table"},
        {.sql = "CREATE TABLE t_compressed(a INT) ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8",
         .context = "create compressed table"},
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
