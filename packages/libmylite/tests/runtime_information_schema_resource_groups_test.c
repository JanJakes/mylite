#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    vcpu_ids_capacity = 32,
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

static int test_information_schema_resource_groups_queries(void);
static int test_information_schema_resource_groups_file_backed_safety(void);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_status(mylite_db *database, struct expected_status expected);
static int format_expected_vcpu_ids(char *buffer, size_t buffer_size);
static long online_processor_count(void);
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

    failures += test_information_schema_resource_groups_queries();
    failures += test_information_schema_resource_groups_file_backed_safety();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_resource_groups_queries(void) {
    static const char *const resource_group_columns[] = {
        "RESOURCE_GROUP_NAME",
        "RESOURCE_GROUP_TYPE",
        "RESOURCE_GROUP_ENABLED",
        "VCPU_IDS",
        "THREAD_PRIORITY",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const priority_column[] = {"THREAD_PRIORITY"};
    static const char *const priority_zero[] = {"0"};
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
        "RESOURCE_GROUPS",
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
        "RESOURCE_GROUPS",
        "RESOURCE_GROUP_NAME",
        "1",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "RESOURCE_GROUPS",
        "RESOURCE_GROUP_TYPE",
        "2",
        NULL,
        "NO",
        "enum",
        "6",
        "18",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "enum('SYSTEM','USER')",
        "select",
        "RESOURCE_GROUPS",
        "RESOURCE_GROUP_ENABLED",
        "3",
        NULL,
        "NO",
        "tinyint",
        NULL,
        NULL,
        "3",
        "0",
        NULL,
        NULL,
        NULL,
        "tinyint(1)",
        "select",
        "RESOURCE_GROUPS",
        "VCPU_IDS",
        "4",
        NULL,
        "YES",
        "blob",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "blob",
        "select",
        "RESOURCE_GROUPS",
        "THREAD_PRIORITY",
        "5",
        NULL,
        "NO",
        "int",
        NULL,
        NULL,
        "10",
        "0",
        NULL,
        NULL,
        NULL,
        "int",
        "select",
    };
    char vcpu_ids[vcpu_ids_capacity];
    const char *resource_group_rows[] = {
        "SYS_default",
        "SYSTEM",
        "1",
        vcpu_ids,
        "0",
        "USR_default",
        "USER",
        "1",
        vcpu_ids,
        "0",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += format_expected_vcpu_ids(vcpu_ids, sizeof(vcpu_ids));
    failures +=
        expect_int(mylite_open(":memory:", &database), MYLITE_OK, "open resource groups db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS LIMIT 0",
            .column_names = resource_group_columns,
            .column_count = sizeof(resource_group_columns) / sizeof(resource_group_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "resource groups wildcard columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT RESOURCE_GROUP_NAME, RESOURCE_GROUP_TYPE, RESOURCE_GROUP_ENABLED, "
                   "VCPU_IDS, THREAD_PRIORITY FROM INFORMATION_SCHEMA.RESOURCE_GROUPS "
                   "ORDER BY RESOURCE_GROUP_NAME",
            .column_names = resource_group_columns,
            .column_count = sizeof(resource_group_columns) / sizeof(resource_group_columns[0]),
            .values = resource_group_rows,
            .row_count = sizeof(resource_group_rows) / sizeof(resource_group_rows[0]) /
                         (sizeof(resource_group_columns) / sizeof(resource_group_columns[0])),
            .warning_count = 0U,
            .context = "resource groups default rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM information_schema.resource_groups "
                   "WHERE RESOURCE_GROUP_TYPE = 'USER'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "case-insensitive resource groups user count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT r.THREAD_PRIORITY FROM INFORMATION_SCHEMA.RESOURCE_GROUPS AS r "
                   "WHERE r.RESOURCE_GROUP_NAME = 'USR_default'",
            .column_names = priority_column,
            .column_count = 1U,
            .values = priority_zero,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "resource groups alias projection",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "resource groups alias status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'RESOURCE_GROUPS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "resource groups system table row",
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
                   "AND TABLE_NAME = 'RESOURCE_GROUPS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .warning_count = 0U,
            .context = "resource groups columns metadata",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_information_schema_resource_groups_file_backed_safety(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_two[] = {"2"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "file") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file resource groups db");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.RESOURCE_GROUPS",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_two,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "file resource groups count",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after resource groups metadata query"
    );

    mylite_close(database);
    remove_related_files(path);
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

static int format_expected_vcpu_ids(char *buffer, size_t buffer_size) {
    long cpu_count = online_processor_count();
    int written = 0;

    if (cpu_count <= 1) {
        written = snprintf(buffer, buffer_size, "0");
    } else {
        written = snprintf(buffer, buffer_size, "0-%ld", cpu_count - 1);
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        fprintf(stderr, "failed to format expected vcpu ids\n");
        return 1;
    }
    return 0;
}

static long online_processor_count(void) {
#ifdef _WIN32
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors > 0U) {
        return (long)info.dwNumberOfProcessors;
    }
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    if (count > 0) {
        return count;
    }
#endif
    return 1;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_information_schema_resource_groups_%s_%d.mylite",
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
