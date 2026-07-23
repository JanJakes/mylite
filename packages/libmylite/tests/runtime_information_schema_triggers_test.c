#include "mylite_test_support.h"

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

static int test_information_schema_triggers_queries(void);
static int test_information_schema_triggers_reopen_preamble_and_handles(void);
static int seed_database(mylite_db *database, const char *schema_name);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_information_schema_triggers_queries();
    failures += test_information_schema_triggers_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_triggers_queries(void) {
    static const char sql_mode_set_column_type[] =
        "set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED',"
        "'ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','NOT_USED_9',"
        "'NOT_USED_10','NOT_USED_11','NOT_USED_12','NOT_USED_13','NOT_USED_14','NOT_USED_15',"
        "'NOT_USED_16','NOT_USED_17','NOT_USED_18','ANSI','NO_AUTO_VALUE_ON_ZERO',"
        "'NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES','STRICT_ALL_TABLES','NO_ZERO_IN_DATE',"
        "'NO_ZERO_DATE','ALLOW_INVALID_DATES','ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL',"
        "'NOT_USED_29','HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH',"
        "'TIME_TRUNCATE_FRACTIONAL')";
    static const char *const triggers_columns[] = {
        "TRIGGER_CATALOG",
        "TRIGGER_SCHEMA",
        "TRIGGER_NAME",
        "EVENT_MANIPULATION",
        "EVENT_OBJECT_CATALOG",
        "EVENT_OBJECT_SCHEMA",
        "EVENT_OBJECT_TABLE",
        "ACTION_ORDER",
        "ACTION_CONDITION",
        "ACTION_STATEMENT",
        "ACTION_ORIENTATION",
        "ACTION_TIMING",
        "ACTION_REFERENCE_OLD_TABLE",
        "ACTION_REFERENCE_NEW_TABLE",
        "ACTION_REFERENCE_OLD_ROW",
        "ACTION_REFERENCE_NEW_ROW",
        "CREATED",
        "SQL_MODE",
        "DEFINER",
        "CHARACTER_SET_CLIENT",
        "COLLATION_CONNECTION",
        "DATABASE_COLLATION",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const trigger_name_column[] = {"TRIGGER_NAME"};
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
        "TRIGGERS",
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
        "TRIGGERS",
        "TRIGGER_CATALOG",
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
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "TRIGGERS",
        "TRIGGER_SCHEMA",
        "2",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "TRIGGERS",
        "TRIGGER_NAME",
        "3",
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
        "TRIGGERS",
        "EVENT_MANIPULATION",
        "4",
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
        "enum('INSERT','UPDATE','DELETE')",
        "select",
        "TRIGGERS",
        "EVENT_OBJECT_CATALOG",
        "5",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "TRIGGERS",
        "EVENT_OBJECT_SCHEMA",
        "6",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "TRIGGERS",
        "EVENT_OBJECT_TABLE",
        "7",
        NULL,
        "NO",
        "varchar",
        "64",
        "192",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "TRIGGERS",
        "ACTION_ORDER",
        "8",
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
        "int unsigned",
        "select",
        "TRIGGERS",
        "ACTION_CONDITION",
        "9",
        NULL,
        "YES",
        "varbinary",
        "0",
        "0",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "varbinary(0)",
        "select",
        "TRIGGERS",
        "ACTION_STATEMENT",
        "10",
        NULL,
        "NO",
        "longtext",
        "4294967295",
        "4294967295",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "longtext",
        "select",
        "TRIGGERS",
        "ACTION_ORIENTATION",
        "11",
        "",
        "NO",
        "varchar",
        "3",
        "9",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        "TRIGGERS",
        "ACTION_TIMING",
        "12",
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
        "enum('BEFORE','AFTER')",
        "select",
        "TRIGGERS",
        "ACTION_REFERENCE_OLD_TABLE",
        "13",
        NULL,
        "YES",
        "varbinary",
        "0",
        "0",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "varbinary(0)",
        "select",
        "TRIGGERS",
        "ACTION_REFERENCE_NEW_TABLE",
        "14",
        NULL,
        "YES",
        "varbinary",
        "0",
        "0",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "varbinary(0)",
        "select",
        "TRIGGERS",
        "ACTION_REFERENCE_OLD_ROW",
        "15",
        "",
        "NO",
        "varchar",
        "3",
        "9",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        "TRIGGERS",
        "ACTION_REFERENCE_NEW_ROW",
        "16",
        "",
        "NO",
        "varchar",
        "3",
        "9",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(3)",
        "select",
        "TRIGGERS",
        "CREATED",
        "17",
        NULL,
        "NO",
        "timestamp",
        NULL,
        NULL,
        NULL,
        NULL,
        "2",
        NULL,
        NULL,
        "timestamp(2)",
        "select",
        "TRIGGERS",
        "SQL_MODE",
        "18",
        NULL,
        "NO",
        "set",
        "520",
        "1560",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        sql_mode_set_column_type,
        "select",
        "TRIGGERS",
        "DEFINER",
        "19",
        NULL,
        "NO",
        "varchar",
        "288",
        "864",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(288)",
        "select",
        "TRIGGERS",
        "CHARACTER_SET_CLIENT",
        "20",
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
        "TRIGGERS",
        "COLLATION_CONNECTION",
        "21",
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
        "TRIGGERS",
        "DATABASE_COLLATION",
        "22",
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
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open triggers query db");
    failures += seed_database(database, "app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'app'",
            .column_names = triggers_columns,
            .column_count = sizeof(triggers_columns) / sizeof(triggers_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty triggers wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "empty triggers count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.triggers WHERE TRIGGER_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive triggers table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT tr.TRIGGER_NAME FROM INFORMATION_SCHEMA.TRIGGERS AS tr "
                   "WHERE tr.TRIGGER_SCHEMA = 'app' ORDER BY tr.TRIGGER_NAME LIMIT 1",
            .column_names = trigger_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "empty triggers alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'TRIGGERS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "triggers system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'TRIGGERS' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "triggers columns metadata",
        }
    );
    failures += expect_row_count_status(database, "triggers row count status");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_triggers_reopen_preamble_and_handles(void) {
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first triggers db"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second triggers db"
    );
    failures += seed_database(first, "app");
    failures += seed_database(second, "other");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "first handle triggers count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql =
                "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'other'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "second handle triggers count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after triggers metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "reopen first triggers db"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "reopened triggers count",
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
