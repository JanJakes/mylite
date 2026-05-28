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
    size_t warning_count;
    const char *context;
};

struct expected_status {
    const char *warning_count;
    const char *row_count;
    const char *context;
};

static const char *const profiling_warning_message =
    "'INFORMATION_SCHEMA.PROFILING' is deprecated and will be removed in a future release. "
    "Please use Performance Schema instead";

static int test_information_schema_profiling_queries(void);
static int test_information_schema_profiling_reopen_preamble_and_handles(void);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_status(mylite_db *database, struct expected_status expected);
static int expect_show_warnings_row(mylite_db *database, const char *context);
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

    failures += test_information_schema_profiling_queries();
    failures += test_information_schema_profiling_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_profiling_queries(void) {
    static const char *const profiling_columns[] = {
        "QUERY_ID",
        "SEQ",
        "STATE",
        "DURATION",
        "CPU_USER",
        "CPU_SYSTEM",
        "CONTEXT_VOLUNTARY",
        "CONTEXT_INVOLUNTARY",
        "BLOCK_OPS_IN",
        "BLOCK_OPS_OUT",
        "MESSAGES_SENT",
        "MESSAGES_RECEIVED",
        "PAGE_FAULTS_MAJOR",
        "PAGE_FAULTS_MINOR",
        "SWAPS",
        "SOURCE_FUNCTION",
        "SOURCE_FILE",
        "SOURCE_LINE",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const query_id_column[] = {"QUERY_ID"};
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
        "PROFILING",
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
        "PROFILING",
        "QUERY_ID",
        "1",
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
        "PROFILING",
        "SEQ",
        "2",
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
        "PROFILING",
        "STATE",
        "3",
        "",
        "NO",
        "varchar",
        "10",
        "30",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(30)",
        "select",
        "PROFILING",
        "DURATION",
        "4",
        "",
        "NO",
        "decimal",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "decimal(905,0)",
        "select",
        "PROFILING",
        "CPU_USER",
        "5",
        "",
        "YES",
        "decimal",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "decimal(905,0)",
        "select",
        "PROFILING",
        "CPU_SYSTEM",
        "6",
        "",
        "YES",
        "decimal",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "decimal(905,0)",
        "select",
        "PROFILING",
        "CONTEXT_VOLUNTARY",
        "7",
        "",
        "YES",
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
        "PROFILING",
        "CONTEXT_INVOLUNTARY",
        "8",
        "",
        "YES",
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
        "PROFILING",
        "BLOCK_OPS_IN",
        "9",
        "",
        "YES",
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
        "PROFILING",
        "BLOCK_OPS_OUT",
        "10",
        "",
        "YES",
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
        "PROFILING",
        "MESSAGES_SENT",
        "11",
        "",
        "YES",
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
        "PROFILING",
        "MESSAGES_RECEIVED",
        "12",
        "",
        "YES",
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
        "PROFILING",
        "PAGE_FAULTS_MAJOR",
        "13",
        "",
        "YES",
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
        "PROFILING",
        "PAGE_FAULTS_MINOR",
        "14",
        "",
        "YES",
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
        "PROFILING",
        "SWAPS",
        "15",
        "",
        "YES",
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
        "PROFILING",
        "SOURCE_FUNCTION",
        "16",
        "",
        "YES",
        "varchar",
        "10",
        "30",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(30)",
        "select",
        "PROFILING",
        "SOURCE_FILE",
        "17",
        "",
        "YES",
        "varchar",
        "6",
        "20",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(20)",
        "select",
        "PROFILING",
        "SOURCE_LINE",
        "18",
        "",
        "YES",
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

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open profiling db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROFILING",
            .column_names = profiling_columns,
            .column_count = sizeof(profiling_columns) / sizeof(profiling_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 1U,
            .context = "empty profiling wildcard",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "1",
            .row_count = "-1",
            .context = "profiling wildcard status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROFILING",
            .column_names = profiling_columns,
            .column_count = sizeof(profiling_columns) / sizeof(profiling_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 1U,
            .context = "profiling warning source",
        }
    );
    failures += expect_show_warnings_row(database, "profiling warning row");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "empty profiling count",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "1",
            .row_count = "-1",
            .context = "profiling count status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.profiling",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "case-insensitive profiling table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING LIMIT 0",
            .column_names = count_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "profiling count limit zero",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "profiling limit zero status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.QUERY_ID FROM INFORMATION_SCHEMA.PROFILING AS p "
                   "WHERE p.QUERY_ID = -1",
            .column_names = query_id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 1U,
            .context = "empty profiling alias filtered",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT p.QUERY_ID FROM INFORMATION_SCHEMA.PROFILING AS p LIMIT 0",
            .column_names = query_id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "profiling alias limit zero",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PROFILING'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "profiling system table row",
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
                   "AND TABLE_NAME = 'PROFILING' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .warning_count = 0U,
            .context = "profiling columns metadata",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "profiling metadata status",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_profiling_reopen_preamble_and_handles(void) {
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

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first profiling db");
    failures +=
        expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second profiling db");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "first handle profiling count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "second handle profiling count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after profiling metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen first profiling db");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROFILING",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "reopened profiling count",
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
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, expected.context);

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

static int expect_status(mylite_db *database, struct expected_status expected) {
    static const char *const status_columns[] = {"@@warning_count", "ROW_COUNT()"};
    const char *const status_values[] = {expected.warning_count, expected.row_count};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .column_names = status_columns,
            .column_count = 2U,
            .values = status_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = expected.context,
        }
    );
}

static int expect_show_warnings_row(mylite_db *database, const char *context) {
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    const char *const warning_values[] = {"Warning", "1287", profiling_warning_message};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .column_names = warning_columns,
            .column_count = 3U,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_information_schema_profiling_%s_%d.mylite",
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
