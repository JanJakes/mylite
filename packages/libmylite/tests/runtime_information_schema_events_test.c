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
    mysql_error_unknown_column = 1054,
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

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_information_schema_events_queries(void);
static int test_information_schema_events_reopen_preamble_and_handles(void);
static int seed_database(mylite_db *database, const char *schema_name);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
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

    failures += test_information_schema_events_queries();
    failures += test_information_schema_events_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_events_queries(void) {
    static const char sql_mode_set_column_type[] =
        "set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED',"
        "'ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','NOT_USED_9',"
        "'NOT_USED_10','NOT_USED_11','NOT_USED_12','NOT_USED_13','NOT_USED_14',"
        "'NOT_USED_15','NOT_USED_16','NOT_USED_17','NOT_USED_18','ANSI',"
        "'NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
        "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','ALLOW_INVALID_DATES',"
        "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NOT_USED_29','HIGH_NOT_PRECEDENCE',"
        "'NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH','TIME_TRUNCATE_FRACTIONAL')";
    static const char interval_field_column_type[] =
        "enum('YEAR','QUARTER','MONTH','DAY','HOUR','MINUTE','WEEK','SECOND',"
        "'MICROSECOND','YEAR_MONTH','DAY_HOUR','DAY_MINUTE','DAY_SECOND','HOUR_MINUTE',"
        "'HOUR_SECOND','MINUTE_SECOND','DAY_MICROSECOND','HOUR_MICROSECOND',"
        "'MINUTE_MICROSECOND','SECOND_MICROSECOND')";
    static const char *const events_columns[] = {
        "EVENT_CATALOG",
        "EVENT_SCHEMA",
        "EVENT_NAME",
        "DEFINER",
        "TIME_ZONE",
        "EVENT_BODY",
        "EVENT_DEFINITION",
        "EVENT_TYPE",
        "EXECUTE_AT",
        "INTERVAL_VALUE",
        "INTERVAL_FIELD",
        "SQL_MODE",
        "STARTS",
        "ENDS",
        "STATUS",
        "ON_COMPLETION",
        "CREATED",
        "LAST_ALTERED",
        "LAST_EXECUTED",
        "EVENT_COMMENT",
        "ORIGINATOR",
        "CHARACTER_SET_CLIENT",
        "COLLATION_CONNECTION",
        "DATABASE_COLLATION",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const event_name_column[] = {"EVENT_NAME"};
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
        "EVENTS",
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
        "EVENT_CATALOG",
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
        "EVENT_SCHEMA",
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
        "EVENT_NAME",
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
        "DEFINER",
        "4",
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
        "TIME_ZONE",
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
        "EVENT_BODY",
        "6",
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
        "EVENT_DEFINITION",
        "7",
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
        "EVENT_TYPE",
        "8",
        "",
        "NO",
        "varchar",
        "9",
        "27",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(9)",
        "select",
        "EXECUTE_AT",
        "9",
        NULL,
        "YES",
        "datetime",
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        NULL,
        NULL,
        "datetime",
        "select",
        "INTERVAL_VALUE",
        "10",
        NULL,
        "YES",
        "varchar",
        "256",
        "768",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(256)",
        "select",
        "INTERVAL_FIELD",
        "11",
        NULL,
        "YES",
        "enum",
        "18",
        "54",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        interval_field_column_type,
        "select",
        "SQL_MODE",
        "12",
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
        "STARTS",
        "13",
        NULL,
        "YES",
        "datetime",
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        NULL,
        NULL,
        "datetime",
        "select",
        "ENDS",
        "14",
        NULL,
        "YES",
        "datetime",
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        NULL,
        NULL,
        "datetime",
        "select",
        "STATUS",
        "15",
        "",
        "NO",
        "varchar",
        "21",
        "63",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(21)",
        "select",
        "ON_COMPLETION",
        "16",
        "",
        "NO",
        "varchar",
        "12",
        "36",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(12)",
        "select",
        "CREATED",
        "17",
        NULL,
        "NO",
        "timestamp",
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        NULL,
        NULL,
        "timestamp",
        "select",
        "LAST_ALTERED",
        "18",
        NULL,
        "NO",
        "timestamp",
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        NULL,
        NULL,
        "timestamp",
        "select",
        "LAST_EXECUTED",
        "19",
        NULL,
        "YES",
        "datetime",
        NULL,
        NULL,
        NULL,
        NULL,
        "0",
        NULL,
        NULL,
        "datetime",
        "select",
        "EVENT_COMMENT",
        "20",
        NULL,
        "NO",
        "varchar",
        "2048",
        "6144",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "varchar(2048)",
        "select",
        "ORIGINATOR",
        "21",
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
        "CHARACTER_SET_CLIENT",
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
        "COLLATION_CONNECTION",
        "23",
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
        "DATABASE_COLLATION",
        "24",
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
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open events query db");
    failures += seed_database(database, "app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = 'app'",
            .column_names = events_columns,
            .column_count = sizeof(events_columns) / sizeof(events_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty events wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "empty events count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.events WHERE EVENT_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive events table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ev.EVENT_NAME FROM INFORMATION_SCHEMA.EVENTS AS ev "
                   "WHERE ev.EVENT_SCHEMA = 'app' ORDER BY ev.EVENT_NAME LIMIT 1",
            .column_names = event_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "empty events alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'EVENTS'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "events system table row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, "
                   "DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, PRIVILEGES "
                   "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' "
                   "AND TABLE_NAME = 'EVENTS' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "events columns metadata",
        }
    );
    failures += expect_row_count_status(database, "events row count status");
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT EVENT_NAME FROM INFORMATION_SCHEMA.EVENTS WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_events_reopen_preamble_and_handles(void) {
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

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first events db");
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second events db"
    );
    failures += seed_database(first, "app");
    failures += seed_database(second, "other");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "first handle events count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = 'other'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "second handle events count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after events metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "reopen first events db"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "reopened events count",
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

static int expect_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", expected.sql);
        mylite_result_free(result);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, expected.sql);
    failures +=
        mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, expected.sql);
    if (strstr(mylite_errmsg(database), expected.message_part) == NULL) {
        fprintf(
            stderr,
            "%s: expected message containing %s, got %s\n",
            expected.sql,
            expected.message_part,
            mylite_errmsg(database)
        );
        ++failures;
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
