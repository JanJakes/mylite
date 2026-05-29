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
    connection_id_text_capacity = 32,
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

static int test_information_schema_innodb_session_temp_tablespaces_queries(void);
static int test_information_schema_innodb_session_temp_tablespaces_file_backed_safety(void);
static int capture_connection_id(
    mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const char *context
);
static int expect_statement_ok(mylite_db *database, struct expected_statement expected);
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

    failures += test_information_schema_innodb_session_temp_tablespaces_queries();
    failures += test_information_schema_innodb_session_temp_tablespaces_file_backed_safety();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_innodb_session_temp_tablespaces_queries(void) {
    static const char *const table_columns[] = {
        "ID",
        "SPACE",
        "PATH",
        "SIZE",
        "STATE",
        "PURPOSE",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_ten[] = {"10"};
    static const char *const count_nine[] = {"9"};
    static const char *const count_one[] = {"1"};
    static const char *const path_column[] = {"PATH"};
    static const char *const intrinsic_path[] = {"./#innodb_temp/temp_10.ibt"};
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
        "INNODB_SESSION_TEMP_TABLESPACES",
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
        "INNODB_SESSION_TEMP_TABLESPACES",
        "ID",
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
        "int unsigned",
        "select",
        "INNODB_SESSION_TEMP_TABLESPACES",
        "SPACE",
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
        "int unsigned",
        "select",
        "INNODB_SESSION_TEMP_TABLESPACES",
        "PATH",
        "3",
        "",
        "NO",
        "varchar",
        "1333",
        "4001",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(4001)",
        "select",
        "INNODB_SESSION_TEMP_TABLESPACES",
        "SIZE",
        "4",
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
        "INNODB_SESSION_TEMP_TABLESPACES",
        "STATE",
        "5",
        "",
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(192)",
        "select",
        "INNODB_SESSION_TEMP_TABLESPACES",
        "PURPOSE",
        "6",
        "",
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(192)",
        "select",
    };
    char path[test_path_capacity];
    char connection_id[connection_id_text_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);
    connection_id[0] = '\0';

    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open innodb session temp tablespaces db"
    );
    failures += capture_connection_id(
        database,
        connection_id,
        sizeof(connection_id),
        "capture innodb session temp tablespaces connection id"
    );

    const char *const table_values[] = {
        "0",           "4243767281", "./#innodb_temp/temp_1.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767282", "./#innodb_temp/temp_2.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767283", "./#innodb_temp/temp_3.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767284", "./#innodb_temp/temp_4.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767285", "./#innodb_temp/temp_5.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767286", "./#innodb_temp/temp_6.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767287", "./#innodb_temp/temp_7.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767288", "./#innodb_temp/temp_8.ibt",  "81920", "INACTIVE", "NONE",
        "0",           "4243767289", "./#innodb_temp/temp_9.ibt",  "81920", "INACTIVE", "NONE",
        connection_id, "4243767290", "./#innodb_temp/temp_10.ibt", "81920", "ACTIVE",   "INTRINSIC",
    };
    const char *const active_values[] = {
        connection_id,
        "4243767290",
        "./#innodb_temp/temp_10.ibt",
        "81920",
        "ACTIVE",
        "INTRINSIC",
    };

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ID, SPACE, PATH, SIZE, STATE, PURPOSE "
                   "FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES ORDER BY SPACE",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = table_values,
            .row_count = sizeof(table_values) / sizeof(table_values[0]) /
                         (sizeof(table_columns) / sizeof(table_columns[0])),
            .context = "innodb session temp tablespaces baseline rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_ten,
            .row_count = 1U,
            .context = "innodb session temp tablespaces count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_session_temp_tablespaces "
                   "WHERE ID = 0 AND STATE = 'INACTIVE' AND PURPOSE = 'NONE'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_nine,
            .row_count = 1U,
            .context = "case-insensitive innodb session temp tablespaces inactive count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ID, SPACE, PATH, SIZE, STATE, PURPOSE "
                   "FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "
                   "WHERE STATE = 'ACTIVE' ORDER BY SPACE",
            .column_names = table_columns,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .values = active_values,
            .row_count = 1U,
            .context = "innodb session temp tablespaces active intrinsic row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT t.PATH FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES AS t "
                   "WHERE t.PURPOSE = 'INTRINSIC'",
            .column_names = path_column,
            .column_count = 1U,
            .values = intrinsic_path,
            .row_count = 1U,
            .context = "innodb session temp tablespaces alias path",
        }
    );
    failures += expect_statement_ok(
        database,
        (struct expected_statement){
            .sql = "USE information_schema",
            .context = "use information_schema for innodb session temp tablespaces",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INNODB_SESSION_TEMP_TABLESPACES "
                   "WHERE STATE = 'ACTIVE'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_one,
            .row_count = 1U,
            .context = "unqualified innodb session temp tablespaces active count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, "
                   "DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'INNODB_SESSION_TEMP_TABLESPACES'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "innodb session temp tablespaces system table row",
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
                   "AND TABLE_NAME = 'INNODB_SESSION_TEMP_TABLESPACES' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "innodb session temp tablespaces columns metadata",
        }
    );
    failures += expect_row_count_status(database, "innodb session temp tablespaces status");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_innodb_session_temp_tablespaces_file_backed_safety(void) {
    static const char *const id_column[] = {"ID"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    char first_connection_id[connection_id_text_capacity];
    char second_connection_id[connection_id_text_capacity];
    char reopened_connection_id[connection_id_text_capacity];
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
    first_connection_id[0] = '\0';
    second_connection_id[0] = '\0';
    reopened_connection_id[0] = '\0';

    failures += expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first innodb session temp tablespaces db"
    );
    failures += expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second innodb session temp tablespaces db"
    );
    failures += capture_connection_id(
        first,
        first_connection_id,
        sizeof(first_connection_id),
        "capture first innodb session temp tablespaces connection id"
    );
    failures += capture_connection_id(
        second,
        second_connection_id,
        sizeof(second_connection_id),
        "capture second innodb session temp tablespaces connection id"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT ID FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "
                   "WHERE PURPOSE = 'INTRINSIC'",
            .column_names = id_column,
            .column_count = 1U,
            .values = (const char *const[]){first_connection_id},
            .row_count = 1U,
            .context = "first handle innodb session temp tablespaces intrinsic id",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT ID FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "
                   "WHERE PURPOSE = 'INTRINSIC'",
            .column_names = id_column,
            .column_count = 1U,
            .values = (const char *const[]){second_connection_id},
            .row_count = 1U,
            .context = "second handle innodb session temp tablespaces intrinsic id",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after innodb session temp tablespaces metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "reopen innodb session temp tablespaces db"
    );
    failures += capture_connection_id(
        first,
        reopened_connection_id,
        sizeof(reopened_connection_id),
        "capture reopened innodb session temp tablespaces connection id"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT ID FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "
                   "WHERE PURPOSE = 'INTRINSIC'",
            .column_names = id_column,
            .column_count = 1U,
            .values = (const char *const[]){reopened_connection_id},
            .row_count = 1U,
            .context = "reopened innodb session temp tablespaces intrinsic id",
        }
    );

    mylite_close(first);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int capture_connection_id(
    mylite_db *database,
    char *buffer,
    size_t buffer_size,
    const char *context
) {
    mylite_result *result = NULL;
    const char *value = NULL;
    int rc = mylite_execute(
        database,
        "SELECT CONNECTION_ID()",
        strlen("SELECT CONNECTION_ID()"),
        &result
    );
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            context,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", context);
        return 1;
    }

    failures += expect_size(mylite_result_column_count(result), 1U, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        expect_text_or_null(mylite_result_column_name(result, 0U), "CONNECTION_ID()", context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    value = mylite_result_value_text(result, 0U, 0U);
    if (value == NULL) {
        fprintf(stderr, "%s: expected non-NULL connection id\n", context);
        failures += 1;
    } else {
        int written = snprintf(buffer, buffer_size, "%s", value);

        if (written < 0 || (size_t)written >= buffer_size) {
            fprintf(stderr, "%s: connection id text is too long\n", context);
            failures += 1;
        }
    }

    mylite_result_free(result);
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
        "/tmp/mylite_information_schema_innodb_session_temp_tablespaces_%s_%d.mylite",
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
