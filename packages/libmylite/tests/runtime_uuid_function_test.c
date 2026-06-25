#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <errno.h>
#include <stdbool.h>
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
    uuid_text_size = 36,
    uuid_metadata_charset_id = 33,
    uuid_short_metadata_charset_id = 63,
    uuid_short_display_length = 21,
    uuid_short_metadata_flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL |
                                MYLITE_RESULT_COLUMN_FLAG_UNSIGNED |
                                MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
    uuid_short_charset_column = 4,
    uuid_short_collation_column = 5,
    uuid_short_coercibility_column = 6,
    uuid_short_warning_count_column = 7,
    uuid_short_row_count_column = 8,
    uuid_generated_scalar_column_count = 5,
    uuid_short_generated_scalar_column_count = 4,
    uuid_is_uuid_column = 5,
    uuid_length_column = 6,
    uuid_char_length_column = 7,
    uuid_version_probe_column = 8,
    uuid_variant_probe_column = 9,
    uuid_charset_column = 10,
    uuid_collation_column = 11,
    uuid_coercibility_column = 12,
    uuid_warning_count_column = 13,
    uuid_row_count_column = 14,
    uuid_dash_time_low_index = 8,
    uuid_dash_time_mid_index = 13,
    uuid_dash_time_high_index = 18,
    uuid_dash_clock_sequence_index = 23,
    uuid_version_index = 14,
    uuid_variant_index = 19,
    decimal_radix = 10,
    mysql_error_unknown_column = 1054,
    mysql_error_native_function_arity = 1582,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct uuid_distinct_values {
    const char *first;
    const char *second;
    const char *third;
};

static int test_no_source_dual_and_do_uuid(void);
static int test_table_backed_uuid_rows_and_file_safety(void);
static int test_uuid_short_function(void);
static int test_uuid_errors_and_identifier_compatibility(void);
static int test_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_uuid_shape(const char *actual, const char *context);
static int expect_uuid_metadata(const mylite_result *result, size_t column, const char *context);
static int expect_distinct_uuid_values(struct uuid_distinct_values values, const char *context);
static int expect_uuid_short_metadata(
    const mylite_result *result,
    size_t column,
    const char *context
);
static int expect_uuid_short_value(const char *actual, uint64_t *out_value, const char *context);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint32(uint32_t actual, uint32_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_no_source_dual_and_do_uuid();
    failures += test_table_backed_uuid_rows_and_file_safety();
    failures += test_uuid_short_function();
    failures += test_uuid_errors_and_identifier_compatibility();
    failures += test_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_no_source_dual_and_do_uuid(void) {
    static const char *const expected_columns[] = {
        "UUID()",
        "uuid()",
        "Uuid()",
        "UUID ()",
        "(UUID())",
        "IS_UUID(UUID())",
        "LENGTH(UUID())",
        "CHAR_LENGTH(UUID())",
        "SUBSTRING(UUID(),15,1)",
        "SUBSTRING(UUID(),20,1)",
        "CHARSET(UUID())",
        "COLLATION(UUID())",
        "COERCIBILITY(UUID())",
        "@@warning_count",
        "ROW_COUNT()",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "no_source") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open UUID no-source file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_ok(
        database,
        "SELECT UUID(),uuid(),Uuid(),UUID (),(UUID()),IS_UUID(UUID()),LENGTH(UUID()),"
        "CHAR_LENGTH(UUID()),SUBSTRING(UUID(),15,1),SUBSTRING(UUID(),20,1),CHARSET(UUID()),"
        "COLLATION(UUID()),COERCIBILITY(UUID()),@@warning_count,ROW_COUNT()",
        &result
    );
    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            sizeof(expected_columns) / sizeof(expected_columns[0]),
            "UUID no-source column count"
        );
        failures += expect_size(mylite_result_row_count(result), 1U, "UUID no-source row count");
        failures +=
            expect_int64(mylite_result_affected_rows(result), 0, "UUID no-source affected rows");
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "UUID no-source warning count");
    }
    for (size_t column = 0U;
         failures == 0 && column < sizeof(expected_columns) / sizeof(expected_columns[0]);
         ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected_columns[column],
            "UUID no-source column name"
        );
    }
    for (size_t column = 0U; failures == 0 && column < uuid_generated_scalar_column_count;
         ++column) {
        failures +=
            expect_uuid_shape(mylite_result_value_text(result, 0U, column), "UUID no-source value");
        failures += expect_uuid_metadata(result, column, "UUID no-source metadata");
    }
    if (failures == 0) {
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_is_uuid_column),
            "1",
            "IS_UUID(UUID())"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_length_column),
            "36",
            "LENGTH(UUID())"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_char_length_column),
            "36",
            "CHAR_LENGTH(UUID())"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_version_probe_column),
            "1",
            "UUID() version nibble"
        );
        failures += expect_contains(
            "89ab",
            mylite_result_value_text(result, 0U, uuid_variant_probe_column),
            "UUID variant nibble"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_charset_column),
            "utf8mb3",
            "UUID charset"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_collation_column),
            "utf8mb3_general_ci",
            "UUID collation"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_coercibility_column),
            "4",
            "UUID coercibility"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_warning_count_column),
            "0",
            "UUID warning count"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_row_count_column),
            "0",
            "UUID row count"
        );
        failures += expect_distinct_uuid_values(
            (struct uuid_distinct_values){
                .first = mylite_result_value_text(result, 0U, 0U),
                .second = mylite_result_value_text(result, 0U, 1U),
                .third = mylite_result_value_text(result, 0U, 2U),
            },
            "UUID no-source distinct values"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT CONCAT('', UUID()) AS c", &result);
    if (failures == 0) {
        failures += expect_uuid_shape(mylite_result_value_text(result, 0U, 0U), "UUID CONCAT");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT UUID() AS u FROM DUAL", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 1U, "UUID DUAL column count");
        failures += expect_text(mylite_result_column_name(result, 0U), "u", "UUID DUAL alias");
        failures += expect_size(mylite_result_row_count(result), 1U, "UUID DUAL row count");
        failures += expect_uuid_shape(mylite_result_value_text(result, 0U, 0U), "UUID DUAL");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DO UUID()", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "UUID DO columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "UUID DO rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "UUID DO affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "UUID DO warnings");
    }
    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_uuid_rows_and_file_safety(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "table") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open UUID table file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1),(2),(3)", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(database, "SELECT id, UUID() AS u FROM t ORDER BY id", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 2U, "UUID table columns");
        failures += expect_size(mylite_result_row_count(result), 3U, "UUID table rows");
        failures += expect_text(mylite_result_value_text(result, 0U, 0U), "1", "UUID table id 1");
        failures += expect_text(mylite_result_value_text(result, 1U, 0U), "2", "UUID table id 2");
        failures += expect_text(mylite_result_value_text(result, 2U, 0U), "3", "UUID table id 3");
        failures += expect_uuid_shape(mylite_result_value_text(result, 0U, 1U), "UUID table row 1");
        failures += expect_uuid_shape(mylite_result_value_text(result, 1U, 1U), "UUID table row 2");
        failures += expect_uuid_shape(mylite_result_value_text(result, 2U, 1U), "UUID table row 3");
        failures += expect_uuid_metadata(result, 1U, "UUID table metadata");
        failures += expect_distinct_uuid_values(
            (struct uuid_distinct_values){
                .first = mylite_result_value_text(result, 0U, 1U),
                .second = mylite_result_value_text(result, 1U, 1U),
                .third = mylite_result_value_text(result, 2U, 1U),
            },
            "UUID table distinct values"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(database, "SELECT id, CONCAT('', UUID()) AS u FROM t ORDER BY id", &result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 2U, "UUID table CONCAT columns");
        failures += expect_size(mylite_result_row_count(result), 3U, "UUID table CONCAT rows");
        failures +=
            expect_uuid_shape(mylite_result_value_text(result, 0U, 1U), "UUID table CONCAT row 1");
        failures +=
            expect_uuid_shape(mylite_result_value_text(result, 1U, 1U), "UUID table CONCAT row 2");
        failures +=
            expect_uuid_shape(mylite_result_value_text(result, 2U, 1U), "UUID table CONCAT row 3");
        failures += expect_distinct_uuid_values(
            (struct uuid_distinct_values){
                .first = mylite_result_value_text(result, 0U, 1U),
                .second = mylite_result_value_text(result, 1U, 1U),
                .third = mylite_result_value_text(result, 2U, 1U),
            },
            "UUID table CONCAT distinct values"
        );
    }
    mylite_result_free(result);
    result = NULL;

    session = mylite_connection_session_state(database);
    failures += expect_uint64(
        session->catalog_generation,
        catalog_generation,
        "UUID table catalog generation"
    );
    failures += expect_uint64(
        session->sqlite_schema_generation,
        sqlite_schema_generation,
        "UUID table sqlite schema generation"
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "UUID table preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen UUID table file");
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "SELECT COUNT(*) AS c FROM t", &result);
    if (failures == 0) {
        failures +=
            expect_text(mylite_result_value_text(result, 0U, 0U), "3", "UUID table reopen rows");
    }
    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_uuid_short_function(void) {
    static const char *const expected_columns[] = {
        "UUID_SHORT()",
        "uuid_short()",
        "Uuid_Short()",
        "UUID_SHORT ()",
        "CHARSET(UUID_SHORT())",
        "COLLATION(UUID_SHORT())",
        "COERCIBILITY(UUID_SHORT())",
        "@@warning_count",
        "ROW_COUNT()",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    uint64_t values[uuid_short_generated_scalar_column_count] = {0};
    uint64_t table_values[3] = {0};
    int failures = 0;

    if (make_test_path(path, sizeof(path), "short") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open UUID_SHORT file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "DO 0", NULL);
    failures += execute_ok(
        database,
        "SELECT UUID_SHORT(),uuid_short(),Uuid_Short(),UUID_SHORT (),"
        "CHARSET(UUID_SHORT()),COLLATION(UUID_SHORT()),COERCIBILITY(UUID_SHORT()),"
        "@@warning_count,ROW_COUNT()",
        &result
    );
    if (failures == 0) {
        failures += expect_size(
            mylite_result_column_count(result),
            sizeof(expected_columns) / sizeof(expected_columns[0]),
            "UUID_SHORT no-source column count"
        );
        failures += expect_size(mylite_result_row_count(result), 1U, "UUID_SHORT no-source rows");
        failures += expect_int64(
            mylite_result_affected_rows(result),
            0,
            "UUID_SHORT no-source affected rows"
        );
        failures +=
            expect_size(mylite_result_warning_count(result), 0U, "UUID_SHORT no-source warnings");
    }
    for (size_t column = 0U;
         failures == 0 && column < sizeof(expected_columns) / sizeof(expected_columns[0]);
         ++column) {
        failures += expect_text(
            mylite_result_column_name(result, column),
            expected_columns[column],
            "UUID_SHORT no-source column name"
        );
    }
    for (size_t column = 0U; failures == 0 && column < uuid_short_generated_scalar_column_count;
         ++column) {
        failures += expect_uuid_short_value(
            mylite_result_value_text(result, 0U, column),
            &values[column],
            "UUID_SHORT no-source value"
        );
        failures += expect_uuid_short_metadata(result, column, "UUID_SHORT no-source metadata");
        if (column > 0U) {
            failures += expect_uint64(
                values[column],
                values[column - 1U] + 1U,
                "UUID_SHORT no-source sequence"
            );
        }
    }
    if (failures == 0) {
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_short_charset_column),
            "binary",
            "UUID_SHORT charset"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_short_collation_column),
            "binary",
            "UUID_SHORT collation"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_short_coercibility_column),
            "5",
            "UUID_SHORT coercibility"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_short_warning_count_column),
            "0",
            "UUID_SHORT warning count"
        );
        failures += expect_text(
            mylite_result_value_text(result, 0U, uuid_short_row_count_column),
            "0",
            "UUID_SHORT row count"
        );
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "SELECT UUID_SHORT() AS u FROM DUAL", &result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_column_count(result), 1U, "UUID_SHORT DUAL column count");
        failures +=
            expect_text(mylite_result_column_name(result, 0U), "u", "UUID_SHORT DUAL alias");
        failures += expect_size(mylite_result_row_count(result), 1U, "UUID_SHORT DUAL rows");
        failures += expect_uuid_short_value(
            mylite_result_value_text(result, 0U, 0U),
            &values[0],
            "UUID_SHORT DUAL value"
        );
        failures += expect_uuid_short_metadata(result, 0U, "UUID_SHORT DUAL metadata");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "DO UUID_SHORT()", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, "UUID_SHORT DO columns");
        failures += expect_size(mylite_result_row_count(result), 0U, "UUID_SHORT DO rows");
        failures += expect_int64(mylite_result_affected_rows(result), 0, "UUID_SHORT DO affected");
        failures += expect_size(mylite_result_warning_count(result), 0U, "UUID_SHORT DO warnings");
    }
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1),(2),(3)", NULL);
    failures += execute_ok(database, "SELECT id, UUID_SHORT() AS u FROM t ORDER BY id", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 2U, "UUID_SHORT table columns");
        failures += expect_size(mylite_result_row_count(result), 3U, "UUID_SHORT table rows");
        failures += expect_text(mylite_result_value_text(result, 0U, 0U), "1", "UUID_SHORT id 1");
        failures += expect_text(mylite_result_value_text(result, 1U, 0U), "2", "UUID_SHORT id 2");
        failures += expect_text(mylite_result_value_text(result, 2U, 0U), "3", "UUID_SHORT id 3");
        failures += expect_uuid_short_metadata(result, 1U, "UUID_SHORT table metadata");
    }
    for (size_t row = 0U; failures == 0 && row < 3U; ++row) {
        failures += expect_uuid_short_value(
            mylite_result_value_text(result, row, 1U),
            &table_values[row],
            "UUID_SHORT table value"
        );
        if (row > 0U) {
            failures += expect_uint64(
                table_values[row],
                table_values[row - 1U] + 1U,
                "UUID_SHORT table sequence"
            );
        }
    }
    mylite_result_free(result);

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_uuid_errors_and_identifier_compatibility(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open UUID errors file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_error(
        database,
        "SELECT UUID(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_SHORT(NULL)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_SHORT(1, 2)",
        (struct expected_sql_error){
            .code = mysql_error_native_function_arity,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'UUID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT UUID_SHORT",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'UUID_SHORT'",
        }
    );
    failures += execute_ok(database, "CREATE TABLE uuid (uuid INT)", NULL);
    failures += execute_ok(database, "INSERT INTO uuid VALUES (7)", NULL);
    failures += execute_ok(database, "SELECT uuid FROM uuid", &result);
    if (failures == 0) {
        failures += expect_size(mylite_result_row_count(result), 1U, "UUID identifier row count");
        failures += expect_text(mylite_result_value_text(result, 0U, 0U), "7", "UUID identifier");
    }
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "CREATE TABLE uuid_short (uuid_short INT)", NULL);
    failures += execute_ok(database, "INSERT INTO uuid_short VALUES (9)", NULL);
    failures += execute_ok(database, "SELECT uuid_short FROM uuid_short", &result);
    if (failures == 0) {
        failures +=
            expect_size(mylite_result_row_count(result), 1U, "UUID_SHORT identifier row count");
        failures +=
            expect_text(mylite_result_value_text(result, 0U, 0U), "9", "UUID_SHORT identifier");
    }
    mylite_result_free(result);
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *first_result = NULL;
    mylite_result *second_result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first UUID handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second UUID handle");
    failures += execute_ok(first, "SELECT UUID()", &first_result);
    failures += execute_ok(second, "SELECT UUID()", &second_result);
    if (failures == 0) {
        failures +=
            expect_uuid_shape(mylite_result_value_text(first_result, 0U, 0U), "first UUID handle");
        failures += expect_uuid_shape(
            mylite_result_value_text(second_result, 0U, 0U),
            "second UUID handle"
        );
    }

    mylite_result_free(first_result);
    mylite_result_free(second_result);
    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *local_result = NULL;
    int rc =
        mylite_execute(database, sql, strlen(sql), out_result == NULL ? &local_result : out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(local_result);
        return 1;
    }
    mylite_result_free(local_result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error\n", sql);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_uuid_shape(const char *actual, const char *context) {
    if (actual == NULL || strlen(actual) != uuid_text_size) {
        fprintf(stderr, "%s: expected 36-byte UUID, got [%s]\n", context, actual);
        return 1;
    }
    for (size_t index = 0U; index < uuid_text_size; ++index) {
        char value = actual[index];

        if (index == uuid_dash_time_low_index || index == uuid_dash_time_mid_index ||
            index == uuid_dash_time_high_index || index == uuid_dash_clock_sequence_index) {
            if (value != '-') {
                fprintf(stderr, "%s: expected dash at %zu in [%s]\n", context, index, actual);
                return 1;
            }
        } else if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) {
            fprintf(stderr, "%s: expected lowercase hex UUID, got [%s]\n", context, actual);
            return 1;
        }
    }
    if (actual[uuid_version_index] != '1') {
        fprintf(stderr, "%s: expected version 1 UUID, got [%s]\n", context, actual);
        return 1;
    }
    if (strchr("89ab", actual[uuid_variant_index]) == NULL) {
        fprintf(stderr, "%s: expected RFC 4122 variant UUID, got [%s]\n", context, actual);
        return 1;
    }
    return 0;
}

static int expect_uuid_metadata(const mylite_result *result, size_t column, const char *context) {
    int failures = 0;

    failures += expect_int(
        (int)mylite_result_column_type(result, column),
        MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
        context
    );
    failures += expect_uint32(mylite_result_column_flags(result, column), 0U, context);
    failures += expect_uint32(
        mylite_result_column_charset_id(result, column),
        uuid_metadata_charset_id,
        context
    );
    failures += expect_uint32(
        mylite_result_column_collation_id(result, column),
        uuid_metadata_charset_id,
        context
    );
    failures +=
        expect_uint64(mylite_result_column_display_length(result, column), uuid_text_size, context);
    failures += expect_int(mylite_result_column_nullable(result, column), 1, context);
    return failures;
}

static int expect_distinct_uuid_values(struct uuid_distinct_values values, const char *context) {
    if (values.first == NULL || values.second == NULL || values.third == NULL ||
        strcmp(values.first, values.second) == 0 || strcmp(values.first, values.third) == 0 ||
        strcmp(values.second, values.third) == 0) {
        fprintf(stderr, "%s: expected distinct UUID values\n", context);
        return 1;
    }
    return 0;
}

static int expect_uuid_short_metadata(
    const mylite_result *result,
    size_t column,
    const char *context
) {
    int failures = 0;

    failures += expect_int(
        (int)mylite_result_column_type(result, column),
        MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
        context
    );
    failures += expect_uint32(
        mylite_result_column_flags(result, column),
        uuid_short_metadata_flags,
        context
    );
    failures += expect_uint32(
        mylite_result_column_charset_id(result, column),
        uuid_short_metadata_charset_id,
        context
    );
    failures += expect_uint32(
        mylite_result_column_collation_id(result, column),
        uuid_short_metadata_charset_id,
        context
    );
    failures += expect_uint64(
        mylite_result_column_display_length(result, column),
        uuid_short_display_length,
        context
    );
    failures += expect_int(mylite_result_column_nullable(result, column), 0, context);
    return failures;
}

static int expect_uuid_short_value(const char *actual, uint64_t *out_value, const char *context) {
    char *end = NULL;
    unsigned long long parsed = 0ULL;

    if (actual == NULL || actual[0] == '\0') {
        fprintf(
            stderr,
            "%s: expected UUID_SHORT integer, got [%s]\n",
            context,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }
    errno = 0;
    parsed = strtoull(actual, &end, decimal_radix);
    if (errno != 0 || end == actual || end == NULL || *end != '\0' || parsed == 0ULL) {
        fprintf(stderr, "%s: expected UUID_SHORT integer, got [%s]\n", context, actual);
        return 1;
    }
    if (out_value != NULL) {
        *out_value = (uint64_t)parsed;
    }
    return 0;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *tmpdir = getenv("TMPDIR");
    int written = 0;

    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }
    written = snprintf(
        path,
        path_size,
        "%s/mylite_uuid_function_%d_%s.mylite",
        tmpdir,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "UUID test path too long\n");
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
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0 || fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "%s: failed to read file\n", path);
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

static int expect_uint32(uint32_t actual, uint32_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %u, got %u\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %llu, got %llu\n",
            context,
            (unsigned long long)expected,
            (unsigned long long)actual
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if ((actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
        (actual != NULL && expected != NULL && strcmp(actual, expected) != 0)) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected [%s] to contain [%s]\n", context, actual, needle);
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (actual == NULL || expected == NULL || memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
