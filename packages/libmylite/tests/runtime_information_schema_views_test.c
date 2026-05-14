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
    seed_create_database_sql_capacity = 128,
    seed_create_table_sql_capacity = 160,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_information_schema_views_queries(void);
static int test_information_schema_views_reopen_preamble_and_handles(void);
static int seed_database(mylite_db *database, const char *schema_name);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_information_schema_views_queries();
    failures += test_information_schema_views_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_views_queries(void) {
    static const char *const views_columns[] = {
        "TABLE_CATALOG",
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "VIEW_DEFINITION",
        "CHECK_OPTION",
        "IS_UPDATABLE",
        "DEFINER",
        "SECURITY_TYPE",
        "CHARACTER_SET_CLIENT",
        "COLLATION_CONNECTION",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const table_name_column[] = {"TABLE_NAME"};
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
        "VIEWS",
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
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "COLUMN_TYPE",
        "PRIVILEGES",
    };
    static const char *const columns_metadata_values[] = {
        "TABLE_CATALOG",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "TABLE_SCHEMA",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "TABLE_NAME",
        "3",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "VIEW_DEFINITION",
        "4",
        NULL,
        "YES",
        "longtext",
        "4294967295",
        "4294967295",
        "utf8mb3",
        "utf8mb3_bin",
        "longtext",
        "select",
        "CHECK_OPTION",
        "5",
        NULL,
        "YES",
        "enum",
        "8",
        "24",
        "utf8mb3",
        "utf8mb3_bin",
        "enum('NONE','LOCAL','CASCADED')",
        "select",
        "IS_UPDATABLE",
        "6",
        NULL,
        "YES",
        "enum",
        "3",
        "9",
        "utf8mb3",
        "utf8mb3_bin",
        "enum('NO','YES')",
        "select",
        "DEFINER",
        "7",
        NULL,
        "YES",
        "varchar",
        "288",
        "864",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(288)",
        "select",
        "SECURITY_TYPE",
        "8",
        NULL,
        "YES",
        "varchar",
        "7",
        "21",
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(7)",
        "select",
        "CHARACTER_SET_CLIENT",
        "9",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "COLLATION_CONNECTION",
        "10",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open views query db");
    failures += seed_database(database, "app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'app'",
            .column_names = views_columns,
            .column_count = sizeof(views_columns) / sizeof(views_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty views wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "empty views count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.views WHERE TABLE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive views table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT v.TABLE_NAME FROM INFORMATION_SCHEMA.VIEWS AS v "
                   "WHERE v.TABLE_SCHEMA = 'app' ORDER BY v.TABLE_NAME LIMIT 1",
            .column_names = table_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "empty views alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'VIEWS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "views system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, PRIVILEGES "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'VIEWS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "views columns metadata",
        }
    );
    failures += expect_row_count_status(database, "views row count status");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_views_reopen_preamble_and_handles(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first views db");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second views db");
    failures += seed_database(first, "app");
    failures += seed_database(second, "other");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "first handle views count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'other'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "second handle views count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after views metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen first views db");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "reopened views count",
        }
    );

    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_database(mylite_db *database, const char *schema_name) {
    char create_database_sql[seed_create_database_sql_capacity];
    char create_table_sql[seed_create_table_sql_capacity];
    int create_database_written = 0;
    int create_table_written = 0;
    int failures = 0;

    create_database_written = snprintf(
        create_database_sql,
        sizeof(create_database_sql),
        "CREATE DATABASE %s",
        schema_name
    );
    create_table_written = snprintf(
        create_table_sql,
        sizeof(create_table_sql),
        "CREATE TABLE %s.t(id INT)",
        schema_name
    );
    if (create_database_written < 0 ||
        (size_t)create_database_written >= sizeof(create_database_sql) ||
        create_table_written < 0 || (size_t)create_table_written >= sizeof(create_table_sql)) {
        fprintf(stderr, "%s: failed to build seed SQL\n", schema_name);
        return 1;
    }

    failures += expect_statement_ok(database, create_database_sql);
    failures += expect_statement_ok(database, create_table_sql);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
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

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            size_t value_index = (row_index * expected.column_count) + column_index;

            failures += expect_text_or_null(
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_information_schema_views_%s_%d.mylite",
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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: expected readable file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file bytes\n", path);
        fclose(file);
        return 1;
    }

    fclose(file);
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (expected == NULL && actual == NULL) {
        return 0;
    }
    if (expected != NULL && actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %s, got %s\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
    );
    return 1;
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
