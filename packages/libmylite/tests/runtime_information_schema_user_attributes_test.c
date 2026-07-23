#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
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

static int test_information_schema_user_attributes_queries(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    return test_information_schema_user_attributes_queries() == 0 ? 0 : 1;
}

static int test_information_schema_user_attributes_queries(void) {
    static const char *const user_attributes_columns[] = {
        "USER",
        "HOST",
        "ATTRIBUTE",
    };
    static const char *const root_attribute_values[] = {
        "root",
        "%",
        NULL,
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const host_column[] = {"HOST"};
    static const char *const host_value[] = {"%"};
    static const char *const attribute_column[] = {"ATTRIBUTE"};
    static const char *const attribute_null_value[] = {NULL};
    static const char *const user_column[] = {"USER"};
    static const char *const root_user_value[] = {"root"};
    static const char *const system_table_columns[] = {
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "VERSION",
        "ROW_FORMAT",
        "TABLE_ROWS",
        "DATA_LENGTH",
    };
    static const char *const system_table_values[] = {
        "USER_ATTRIBUTES",
        "SYSTEM VIEW",
        NULL,
        "10",
        NULL,
        "0",
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
        "DATETIME_PRECISION",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
    };
    static const char *const metadata_values[] = {
        "USER",       "1",
        "",           "NO",
        "char",       "32",
        "96",         NULL,
        NULL,         NULL,
        "utf8mb3",    "utf8mb3_bin",
        "char(32)",   "select",
        "HOST",       "2",
        "",           "NO",
        "char",       "255",
        "255",        NULL,
        NULL,         NULL,
        "ascii",      "ascii_general_ci",
        "char(255)",  "select",
        "ATTRIBUTE",  "3",
        NULL,         "YES",
        "longtext",   "4294967295",
        "4294967295", NULL,
        NULL,         NULL,
        "utf8mb4",    "utf8mb4_bin",
        "longtext",   "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_default_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.USER_ATTRIBUTES",
            .column_names = user_attributes_columns,
            .column_count = sizeof(user_attributes_columns) / sizeof(user_attributes_columns[0]),
            .values = root_attribute_values,
            .row_count = 1U,
            .context = "user attributes wildcard projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER, HOST, ATTRIBUTE FROM INFORMATION_SCHEMA.USER_ATTRIBUTES "
                   "WHERE USER = 'root' AND HOST = '%' ORDER BY USER",
            .column_names = user_attributes_columns,
            .column_count = sizeof(user_attributes_columns) / sizeof(user_attributes_columns[0]),
            .values = root_attribute_values,
            .row_count = 1U,
            .context = "user attributes root row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.USER_ATTRIBUTES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "user attributes count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ua.HOST FROM INFORMATION_SCHEMA.USER_ATTRIBUTES AS ua "
                   "WHERE ua.USER = 'root' AND ua.ATTRIBUTE IS NULL",
            .column_names = host_column,
            .column_count = 1U,
            .values = host_value,
            .row_count = 1U,
            .context = "user attributes alias predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER FROM INFORMATION_SCHEMA.USER_ATTRIBUTES "
                   "WHERE HOST LIKE '%' ORDER BY USER LIMIT 1",
            .column_names = user_column,
            .column_count = 1U,
            .values = root_user_value,
            .row_count = 1U,
            .context = "user attributes unquoted user column",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'USER_ATTRIBUTES'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "user attributes system table metadata",
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
                   "AND TABLE_NAME = 'USER_ATTRIBUTES' ORDER BY ORDINAL_POSITION",
            .column_names = metadata_columns,
            .column_count = sizeof(metadata_columns) / sizeof(metadata_columns[0]),
            .values = metadata_values,
            .row_count = sizeof(metadata_values) / sizeof(metadata_values[0]) /
                         (sizeof(metadata_columns) / sizeof(metadata_columns[0])),
            .context = "user attributes columns metadata",
        }
    );
    failures += expect_statement_ok(database, "USE information_schema");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ATTRIBUTE FROM USER_ATTRIBUTES WHERE USER = 'root'",
            .column_names = attribute_column,
            .column_count = 1U,
            .values = attribute_null_value,
            .row_count = 1U,
            .context = "selected information_schema user attributes",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT USER FROM INFORMATION_SCHEMA.USER_ATTRIBUTES WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got OK\n", expected.sql);
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures +=
        mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove(path);
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
    remove(related);
}
