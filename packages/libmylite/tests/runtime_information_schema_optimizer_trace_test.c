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
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_information_schema_optimizer_trace_queries(void);
static int test_information_schema_optimizer_trace_reopen_preamble_and_handles(void);
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

    failures += test_information_schema_optimizer_trace_queries();
    failures += test_information_schema_optimizer_trace_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_optimizer_trace_queries(void) {
    static const char *const trace_columns[] = {
        "QUERY",
        "TRACE",
        "MISSING_BYTES_BEYOND_MAX_MEM_SIZE",
        "INSUFFICIENT_PRIVILEGES",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const query_column[] = {"QUERY"};
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
        "OPTIMIZER_TRACE",
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
        "OPTIMIZER_TRACE",
        "QUERY",
        "1",
        "",
        "NO",
        "varchar",
        "21845",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(65535)",
        "select",
        "OPTIMIZER_TRACE",
        "TRACE",
        "2",
        "",
        "NO",
        "varchar",
        "21845",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(65535)",
        "select",
        "OPTIMIZER_TRACE",
        "MISSING_BYTES_BEYOND_MAX_MEM_SIZE",
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
        "OPTIMIZER_TRACE",
        "INSUFFICIENT_PRIVILEGES",
        "4",
        "",
        "NO",
        "tinyint",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "tinyint(1)",
        "select",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open optimizer trace db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE",
            .column_names = trace_columns,
            .column_count = sizeof(trace_columns) / sizeof(trace_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty optimizer trace wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "empty optimizer trace count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.optimizer_trace",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive optimizer trace table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.QUERY FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE AS t "
                   "ORDER BY t.QUERY LIMIT 1",
            .column_names = query_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "empty optimizer trace alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'OPTIMIZER_TRACE'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "optimizer trace system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, NUMERIC_SCALE, "
                   "DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "
                   "COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'OPTIMIZER_TRACE' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "optimizer trace columns metadata",
        }
    );
    failures += expect_row_count_status(database, "optimizer trace row count status");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_optimizer_trace_reopen_preamble_and_handles(void) {
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first trace db");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second trace db");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "first handle optimizer trace count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "second handle optimizer trace count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after optimizer trace metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen first trace db");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "reopened optimizer trace count",
        }
    );

    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
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
        "/tmp/mylite_information_schema_optimizer_trace_%s_%d.mylite",
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
        fprintf(stderr, "%s: failed to read expected bytes\n", path);
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
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
        fprintf(stderr, "%s: byte mismatch\n", context);
        return 1;
    }
    return 0;
}
