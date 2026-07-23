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

static int test_information_schema_routines_queries(void);
static int test_information_schema_routines_reopen_preamble_and_handles(void);
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

    failures += test_information_schema_routines_queries();
    failures += test_information_schema_routines_reopen_preamble_and_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_routines_queries(void) {
    static const char sql_mode_set_column_type[] =
        "set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED',"
        "'ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','NOT_USED_9',"
        "'NOT_USED_10','NOT_USED_11','NOT_USED_12','NOT_USED_13','NOT_USED_14',"
        "'NOT_USED_15','NOT_USED_16','NOT_USED_17','NOT_USED_18','ANSI',"
        "'NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
        "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','ALLOW_INVALID_DATES',"
        "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NOT_USED_29','HIGH_NOT_PRECEDENCE',"
        "'NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH','TIME_TRUNCATE_FRACTIONAL')";
    static const char routine_type_column_type[] = "enum('FUNCTION','PROCEDURE')";
    static const char sql_data_access_column_type[] =
        "enum('CONTAINS SQL','NO SQL','READS SQL DATA','MODIFIES SQL DATA')";
    static const char security_type_column_type[] = "enum('DEFAULT','INVOKER','DEFINER')";
    static const char *const routines_columns[] = {
        "SPECIFIC_NAME",
        "ROUTINE_CATALOG",
        "ROUTINE_SCHEMA",
        "ROUTINE_NAME",
        "ROUTINE_TYPE",
        "DATA_TYPE",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
        "NUMERIC_PRECISION",
        "NUMERIC_SCALE",
        "DATETIME_PRECISION",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "DTD_IDENTIFIER",
        "ROUTINE_BODY",
        "ROUTINE_DEFINITION",
        "EXTERNAL_NAME",
        "EXTERNAL_LANGUAGE",
        "PARAMETER_STYLE",
        "IS_DETERMINISTIC",
        "SQL_DATA_ACCESS",
        "SQL_PATH",
        "SECURITY_TYPE",
        "CREATED",
        "LAST_ALTERED",
        "SQL_MODE",
        "ROUTINE_COMMENT",
        "DEFINER",
        "CHARACTER_SET_CLIENT",
        "COLLATION_CONNECTION",
        "DATABASE_COLLATION",
    };
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const routine_name_column[] = {"ROUTINE_NAME"};
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
        "ROUTINES",
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
        "SPECIFIC_NAME",
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
        "ROUTINE_CATALOG",
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
        "ROUTINE_SCHEMA",
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
        "utf8mb3_bin",
        "varchar(64)",
        "select",
        "ROUTINE_NAME",
        "4",
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
        "ROUTINE_TYPE",
        "5",
        NULL,
        "NO",
        "enum",
        "9",
        "27",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        routine_type_column_type,
        "select",
        "DATA_TYPE",
        "6",
        NULL,
        "YES",
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
        "CHARACTER_MAXIMUM_LENGTH",
        "7",
        NULL,
        "YES",
        "bigint",
        NULL,
        NULL,
        "19",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint",
        "select",
        "CHARACTER_OCTET_LENGTH",
        "8",
        NULL,
        "YES",
        "bigint",
        NULL,
        NULL,
        "19",
        "0",
        NULL,
        NULL,
        NULL,
        "bigint",
        "select",
        "NUMERIC_PRECISION",
        "9",
        NULL,
        "YES",
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
        "NUMERIC_SCALE",
        "10",
        NULL,
        "YES",
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
        "DATETIME_PRECISION",
        "11",
        NULL,
        "YES",
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
        "CHARACTER_SET_NAME",
        "12",
        NULL,
        "YES",
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
        "COLLATION_NAME",
        "13",
        NULL,
        "YES",
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
        "DTD_IDENTIFIER",
        "14",
        NULL,
        "YES",
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
        "ROUTINE_BODY",
        "15",
        "",
        "NO",
        "varchar",
        "8",
        "24",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(8)",
        "select",
        "ROUTINE_DEFINITION",
        "16",
        NULL,
        "YES",
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
        "EXTERNAL_NAME",
        "17",
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
        "EXTERNAL_LANGUAGE",
        "18",
        "SQL",
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
        "PARAMETER_STYLE",
        "19",
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
        "IS_DETERMINISTIC",
        "20",
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
        "SQL_DATA_ACCESS",
        "21",
        NULL,
        "NO",
        "enum",
        "17",
        "51",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        sql_data_access_column_type,
        "select",
        "SQL_PATH",
        "22",
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
        "SECURITY_TYPE",
        "23",
        NULL,
        "NO",
        "enum",
        "7",
        "21",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        security_type_column_type,
        "select",
        "CREATED",
        "24",
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
        "25",
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
        "SQL_MODE",
        "26",
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
        "ROUTINE_COMMENT",
        "27",
        NULL,
        "NO",
        "text",
        "65535",
        "65535",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_bin",
        "text",
        "select",
        "DEFINER",
        "28",
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
        "CHARACTER_SET_CLIENT",
        "29",
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
        "30",
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
        "31",
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
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open routines query db");
    failures += seed_database(database, "app");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = 'app'",
            .column_names = routines_columns,
            .column_count = sizeof(routines_columns) / sizeof(routines_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "empty routines wildcard",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "empty routines count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.routines WHERE ROUTINE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "case-insensitive routines table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT r.ROUTINE_NAME FROM INFORMATION_SCHEMA.ROUTINES AS r "
                   "WHERE r.ROUTINE_SCHEMA = 'app' ORDER BY r.ROUTINE_NAME LIMIT 1",
            .column_names = routine_name_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .context = "empty routines alias order limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'ROUTINES'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .context = "routines system table row",
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
                   "AND TABLE_NAME = 'ROUTINES' ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = sizeof(columns_metadata_values) / sizeof(columns_metadata_values[0]) /
                         (sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0])),
            .context = "routines columns metadata",
        }
    );
    failures += expect_row_count_status(database, "routines row count status");
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT ROUTINE_NAME FROM INFORMATION_SCHEMA.ROUTINES WHERE nope = 'x'",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_routines_reopen_preamble_and_handles(void) {
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
        "open first routines db"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second routines db"
    );
    failures += seed_database(first, "app");
    failures += seed_database(second, "other");
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "first handle routines count",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql =
                "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = 'other'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "second handle routines count",
        }
    );
    failures += read_file_at(first_path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after routines metadata query"
    );

    mylite_close(first);
    first = NULL;
    mylite_close(second);
    second = NULL;

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "reopen first routines db"
    );
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = 'app'",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .context = "reopened routines count",
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
