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
    mysql_error_unknown_column = 1054,
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

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_information_schema_schemata_extensions_queries(void);
static int seed_database(mylite_db *database);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    return test_information_schema_schemata_extensions_queries() == 0 ? 0 : 1;
}

static int test_information_schema_schemata_extensions_queries(void) {
    static const char *const schemata_extensions_columns[] = {
        "CATALOG_NAME",
        "SCHEMA_NAME",
        "OPTIONS",
    };
    static const char *const app_row_values[] = {
        "def",
        "app",
        "",
    };
    static const char *const builtin_columns[] = {
        "SCHEMA_NAME",
        "OPTIONS",
    };
    static const char *const builtin_values[] = {
        "information_schema",
        "",
        "mysql",
        "",
        "performance_schema",
        "",
        "sys",
        "",
    };
    static const char *const schema_name_column[] = {"SCHEMA_NAME"};
    static const char *const ordered_limit_values[] = {"app", "information_schema", "mysql"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_all_values[] = {"5"};
    static const char *const system_table_columns[] = {
        "TABLE_SCHEMA",
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
        "information_schema",
        "SCHEMATA_EXTENSIONS",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
        "0",
        NULL,
    };
    static const char *const columns_metadata_columns[] = {
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
        "CATALOG_NAME", "1",           NULL,          "NO",
        "varchar",      "64",          "192",         NULL,
        NULL,           NULL,          "utf8mb3",     "utf8mb3_bin",
        "varchar(64)",  "select",      "SCHEMA_NAME", "2",
        NULL,           "NO",          "varchar",     "64",
        "192",          NULL,          NULL,          NULL,
        "utf8mb3",      "utf8mb3_bin", "varchar(64)", "select",
        "OPTIONS",      "3",           NULL,          "YES",
        "varchar",      "256",         "768",         NULL,
        NULL,           NULL,          "utf8mb3",     "utf8mb3_general_ci",
        "varchar(256)", "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_default_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += seed_database(database);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS "
                   "WHERE SCHEMA_NAME = 'app'",
            .column_names = schemata_extensions_columns,
            .column_count =
                sizeof(schemata_extensions_columns) / sizeof(schemata_extensions_columns[0]),
            .values = app_row_values,
            .row_count = 1U,
            .context = "schemata extensions app row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME, OPTIONS FROM INFORMATION_SCHEMA.schemata_extensions "
                   "WHERE SCHEMA_NAME IN ('information_schema', 'mysql', "
                   "'performance_schema', 'sys') ORDER BY SCHEMA_NAME",
            .column_names = builtin_columns,
            .column_count = sizeof(builtin_columns) / sizeof(builtin_columns[0]),
            .values = builtin_values,
            .row_count = sizeof(builtin_values) / sizeof(builtin_values[0]) /
                         (sizeof(builtin_columns) / sizeof(builtin_columns[0])),
            .context = "schemata extensions built-in rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT s.SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS AS s "
                   "WHERE s.OPTIONS = '' ORDER BY s.SCHEMA_NAME LIMIT 3",
            .column_names = schema_name_column,
            .column_count = 1U,
            .values = ordered_limit_values,
            .row_count = 3U,
            .context = "schemata extensions alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS "
                   "WHERE OPTIONS = ''",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_all_values,
            .row_count = 1U,
            .context = "schemata extensions count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'SCHEMATA_EXTENSIONS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "schemata extensions system table metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, PRIVILEGES "
                   "FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'SCHEMATA_EXTENSIONS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "schemata extensions columns metadata",
        }
    );
    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT SCHEMA_NAME FROM SCHEMATA_EXTENSIONS WHERE SCHEMA_NAME = 'app'",
            .column_names = schema_name_column,
            .column_count = 1U,
            .values = ordered_limit_values,
            .row_count = 1U,
            .context = "selected information_schema unqualified schemata extensions",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS "
                   "WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int seed_database(mylite_db *database) {
    int failures = 0;

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "CREATE TABLE app.t (id INT)");

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
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
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
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column),
            expected.column_names[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected.values[value_index],
                expected.context
            );
        }
    }

    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
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
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures +=
        mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, expected.sql);
    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}
