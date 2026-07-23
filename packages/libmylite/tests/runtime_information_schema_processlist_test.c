#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
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
    connection_id_text_capacity = 32,
    dynamic_sql_capacity = 512,
    long_comment_length = 160,
    processlist_column_count = 8,
    processlist_id_column = 0,
    processlist_user_column = 1,
    processlist_host_column = 2,
    processlist_db_column = 3,
    processlist_command_column = 4,
    processlist_time_column = 5,
    processlist_state_column = 6,
    processlist_info_column = 7,
};

struct expected_processlist_result {
    const char *sql;
    const char *expected_db;
    const char *expected_info;
    size_t expected_info_length;
    const char *context;
    char *out_connection_id;
    size_t out_connection_id_size;
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

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const processlist_column_names[processlist_column_count] = {
    "ID",
    "USER",
    "HOST",
    "DB",
    "COMMAND",
    "TIME",
    "STATE",
    "INFO",
};
static const char *const processlist_warning_message =
    "'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future release. "
    "Please use performance_schema.processlist instead";

static int test_information_schema_processlist_queries_and_preamble(void);
static int test_information_schema_processlist_reopen_and_independent_handles(void);
static int expect_processlist_result(
    mylite_db *database,
    struct expected_processlist_result expected
);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_status(mylite_db *database, struct expected_status expected);
static int expect_show_warnings_row(mylite_db *database, const char *context);
static int expect_error(mylite_db *database, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int build_id_sql(
    char *buffer,
    size_t buffer_size,
    const char *format,
    const char *connection_id
);
static int build_long_info_sql(char *buffer, size_t buffer_size, const char *connection_id);
static int expect_decimal_text(const char *text, const char *context);
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

    failures += test_information_schema_processlist_queries_and_preamble();
    failures += test_information_schema_processlist_reopen_and_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_information_schema_processlist_queries_and_preamble(void) {
    static const char *const id_column[] = {"ID"};
    static const char *const info_column[] = {"INFO"};
    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_one[] = {"1"};
    static const char *const count_zero[] = {"0"};
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
        "PROCESSLIST",
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
        "PROCESSLIST",
        "ID",
        "1",
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
        "PROCESSLIST",
        "USER",
        "2",
        "",
        "NO",
        "varchar",
        "10",
        "32",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(32)",
        "select",
        "PROCESSLIST",
        "HOST",
        "3",
        "",
        "NO",
        "varchar",
        "87",
        "261",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(261)",
        "select",
        "PROCESSLIST",
        "DB",
        "4",
        "",
        "YES",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "PROCESSLIST",
        "COMMAND",
        "5",
        "",
        "NO",
        "varchar",
        "5",
        "16",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(16)",
        "select",
        "PROCESSLIST",
        "TIME",
        "6",
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
        "PROCESSLIST",
        "STATE",
        "7",
        "",
        "YES",
        "varchar",
        "21",
        "64",
        NULL,
        NULL,
        NULL,
        "utf8mb3",
        "utf8mb3_general_ci",
        "varchar(64)",
        "select",
        "PROCESSLIST",
        "INFO",
        "8",
        "",
        "YES",
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
    };
    char path[test_path_capacity];
    char connection_id[connection_id_text_capacity];
    char sql[dynamic_sql_capacity];
    char long_sql[dynamic_sql_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    mylite_result_free(NULL);
    if (mylite_test_make_path(path, sizeof(path), "queries") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open processlist file");
    failures += expect_processlist_result(
        database,
        (struct expected_processlist_result){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_db = NULL,
            .expected_info = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_info_length = strlen("SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST"),
            .context = "bare processlist table",
            .out_connection_id = connection_id,
            .out_connection_id_size = sizeof(connection_id),
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "1",
            .row_count = "-1",
            .context = "bare processlist status",
        }
    );
    failures += expect_processlist_result(
        database,
        (struct expected_processlist_result){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_db = NULL,
            .expected_info = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_info_length = strlen("SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST"),
            .context = "processlist warning source",
        }
    );
    failures += expect_show_warnings_row(database, "processlist warning row");

    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += build_id_sql(
        sql,
        sizeof(sql),
        "SELECT ID, USER, HOST, DB, COMMAND, TIME, STATE, INFO "
        "FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = %s",
        connection_id
    );
    if (failures == 0) {
        failures += expect_processlist_result(
            database,
            (struct expected_processlist_result){
                .sql = sql,
                .expected_db = "app",
                .expected_info = sql,
                .expected_info_length = strlen(sql),
                .context = "selected schema processlist row",
            }
        );
    }
    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "processlist catalog generation unchanged"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "processlist sqlite generation unchanged"
    );

    failures += build_id_sql(
        sql,
        sizeof(sql),
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = %s",
        connection_id
    );
    if (failures == 0) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = sql,
                .column_names = count_column,
                .column_count = 1U,
                .values = count_one,
                .row_count = 1U,
                .warning_count = 1U,
                .context = "processlist count current row",
            }
        );
        failures += expect_status(
            database,
            (struct expected_status){
                .warning_count = "1",
                .row_count = "-1",
                .context = "processlist count status",
            }
        );
    }

    failures += build_id_sql(
        sql,
        sizeof(sql),
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.processlist WHERE ID = %s",
        connection_id
    );
    if (failures == 0) {
        failures += expect_query(
            database,
            (struct expected_query){
                .sql = sql,
                .column_names = count_column,
                .column_count = 1U,
                .values = count_one,
                .row_count = 1U,
                .warning_count = 1U,
                .context = "lowercase processlist source",
            }
        );
    }

    failures += build_id_sql(
        sql,
        sizeof(sql),
        "SELECT p.ID FROM INFORMATION_SCHEMA.PROCESSLIST AS p "
        "WHERE p.ID = %s ORDER BY p.ID DESC LIMIT 1",
        connection_id
    );
    if (failures == 0) {
        const char *const values[] = {connection_id};

        failures += expect_query(
            database,
            (struct expected_query){
                .sql = sql,
                .column_names = id_column,
                .column_count = 1U,
                .values = values,
                .row_count = 1U,
                .warning_count = 1U,
                .context = "processlist alias order limit",
            }
        );
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = -1",
            .column_names = id_column,
            .column_count = 1U,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "processlist no-match rows",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "processlist no-match status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = -1",
            .column_names = count_column,
            .column_count = 1U,
            .values = count_zero,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "processlist no-match count",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "processlist no-match count status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST LIMIT 0",
            .column_names = processlist_column_names,
            .column_count = processlist_column_count,
            .values = NULL,
            .row_count = 0U,
            .warning_count = 0U,
            .context = "processlist limit zero",
        }
    );
    failures += expect_status(
        database,
        (struct expected_status){
            .warning_count = "0",
            .row_count = "-1",
            .context = "processlist limit zero status",
        }
    );

    failures += build_long_info_sql(long_sql, sizeof(long_sql), connection_id);
    if (failures == 0) {
        const char *const values[] = {long_sql};

        failures += expect_query(
            database,
            (struct expected_query){
                .sql = long_sql,
                .column_names = info_column,
                .column_count = 1U,
                .values = values,
                .row_count = 1U,
                .warning_count = 1U,
                .context = "processlist untruncated info",
            }
        );
    }

    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "
                   "TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PROCESSLIST'",
            .column_names = system_table_columns,
            .column_count = sizeof(system_table_columns) / sizeof(system_table_columns[0]),
            .values = system_table_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "processlist system table metadata",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, "
                   "IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "
                   "NUMERIC_PRECISION, NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, "
                   "COLLATION_NAME, COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PROCESSLIST' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = columns_metadata_columns,
            .column_count = sizeof(columns_metadata_columns) / sizeof(columns_metadata_columns[0]),
            .values = columns_metadata_values,
            .row_count = processlist_column_count,
            .warning_count = 0U,
            .context = "processlist columns metadata",
        }
    );

    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT nope FROM INFORMATION_SCHEMA.PROCESSLIST",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'field list'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE nope = 1",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'where clause'",
        }
    );
    failures += expect_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST ORDER BY nope",
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'nope' in 'order clause'",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "processlist preamble unchanged"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_information_schema_processlist_reopen_and_independent_handles(void) {
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    char first_id[connection_id_text_capacity];
    char second_id[connection_id_text_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += expect_statement_ok(first, "CREATE DATABASE first_app");
    failures += expect_statement_ok(first, "USE first_app");
    failures += expect_statement_ok(second, "CREATE DATABASE second_app");
    failures += expect_statement_ok(second, "USE second_app");

    failures += expect_processlist_result(
        first,
        (struct expected_processlist_result){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_db = "first_app",
            .expected_info = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_info_length = strlen("SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST"),
            .context = "first handle processlist",
            .out_connection_id = first_id,
            .out_connection_id_size = sizeof(first_id),
        }
    );
    failures += expect_processlist_result(
        second,
        (struct expected_processlist_result){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_db = "second_app",
            .expected_info = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_info_length = strlen("SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST"),
            .context = "second handle processlist",
            .out_connection_id = second_id,
            .out_connection_id_size = sizeof(second_id),
        }
    );
    if (strcmp(first_id, second_id) == 0) {
        fprintf(stderr, "expected independent processlist ids, both were %s\n", first_id);
        ++failures;
    }

    mylite_close(first);
    first = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "reopen first handle");
    failures += expect_statement_ok(first, "CREATE DATABASE reopened_app");
    failures += expect_statement_ok(first, "USE reopened_app");
    failures += expect_processlist_result(
        first,
        (struct expected_processlist_result){
            .sql = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_db = "reopened_app",
            .expected_info = "SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST",
            .expected_info_length = strlen("SELECT * FROM INFORMATION_SCHEMA.PROCESSLIST"),
            .context = "reopened handle processlist",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int expect_processlist_result(
    mylite_db *database,
    struct expected_processlist_result expected
) {
    char connection_id_copy[connection_id_text_capacity];
    mylite_result *result = NULL;
    const char *connection_id = NULL;
    const char *row_info = NULL;
    const char *info = NULL;
    size_t row_count = 0U;
    size_t row_index = 0U;
    int failures = 0;

    connection_id_copy[0] = '\0';
    failures += execute_ok(database, expected.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        processlist_column_count,
        expected.context
    );
    for (size_t column_index = 0U; column_index < processlist_column_count; ++column_index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column_index),
            processlist_column_names[column_index],
            expected.context
        );
    }
    row_count = mylite_result_row_count(result);
    if (row_count == 0U) {
        fprintf(stderr, "%s: expected at least one processlist row\n", expected.context);
        ++failures;
        mylite_result_free(result);
        return failures;
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 1U, expected.context);

    while (row_index < row_count) {
        row_info = mylite_result_value_text(result, row_index, processlist_info_column);
        if (row_info != NULL && strcmp(row_info, expected.expected_info) == 0) {
            break;
        }
        ++row_index;
    }
    if (row_index == row_count) {
        fprintf(stderr, "%s: expected current query processlist row\n", expected.context);
        ++failures;
        row_index = 0U;
    }

    connection_id = mylite_result_value_text(result, row_index, processlist_id_column);
    failures += expect_decimal_text(connection_id, expected.context);
    if (connection_id != NULL) {
        int written = snprintf(connection_id_copy, sizeof(connection_id_copy), "%s", connection_id);

        if (written < 0 || (size_t)written >= sizeof(connection_id_copy)) {
            fprintf(stderr, "%s: failed to copy processlist id\n", expected.context);
            ++failures;
        }
    }
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, row_index, processlist_user_column),
        "root",
        expected.context
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, row_index, processlist_host_column),
        "%",
        expected.context
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, row_index, processlist_db_column),
        expected.expected_db,
        expected.context
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, row_index, processlist_command_column),
        "Query",
        expected.context
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, row_index, processlist_time_column),
        "0",
        expected.context
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, row_index, processlist_state_column),
        "executing",
        expected.context
    );
    info = mylite_result_value_text(result, row_index, processlist_info_column);
    failures += mylite_test_expect_text_or_null(info, expected.expected_info, expected.context);
    if (info == NULL) {
        fprintf(stderr, "%s: expected non-null processlist info\n", expected.context);
        ++failures;
    } else {
        failures +=
            mylite_test_expect_size(strlen(info), expected.expected_info_length, expected.context);
    }
    if (expected.out_connection_id != NULL) {
        int written = snprintf(
            expected.out_connection_id,
            expected.out_connection_id_size,
            "%s",
            connection_id_copy
        );

        if (written < 0 || (size_t)written >= expected.out_connection_id_size) {
            fprintf(stderr, "%s: failed to copy output processlist id\n", expected.context);
            ++failures;
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (result == NULL) {
        return failures + 1;
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
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        expected.context
    );
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
    const char *const warning_values[] = {"Warning", "1287", processlist_warning_message};

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
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, expected.sql);
    mylite_result_free(result);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result != NULL) {
        failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
        failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got rc=%d code=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    return 0;
}

static int build_id_sql(
    char *buffer,
    size_t buffer_size,
    const char *format,
    const char *connection_id
) {
    int written = snprintf(buffer, buffer_size, format, connection_id);

    if (written < 0 || (size_t)written >= buffer_size) {
        fprintf(stderr, "failed to build processlist SQL\n");
        return 1;
    }
    return 0;
}

static int build_long_info_sql(char *buffer, size_t buffer_size, const char *connection_id) {
    static const char prefix[] = "SELECT /* ";
    static const char suffix_format[] =
        " */ INFO FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = %s";
    char suffix[dynamic_sql_capacity];
    size_t prefix_length = strlen(prefix);
    size_t suffix_length = 0U;
    size_t required = 0U;
    int written = snprintf(suffix, sizeof(suffix), suffix_format, connection_id);

    if (written < 0 || (size_t)written >= sizeof(suffix)) {
        fprintf(stderr, "failed to build long processlist suffix\n");
        return 1;
    }
    suffix_length = strlen(suffix);
    required = prefix_length + long_comment_length + suffix_length;
    if (required >= buffer_size) {
        fprintf(stderr, "long processlist SQL buffer is too small\n");
        return 1;
    }
    memcpy(buffer, prefix, prefix_length);
    memset(buffer + prefix_length, 'x', long_comment_length);
    memcpy(buffer + prefix_length + long_comment_length, suffix, suffix_length);
    buffer[required] = '\0';
    return 0;
}

static int expect_decimal_text(const char *text, const char *context) {
    if (text == NULL || text[0] == '\0') {
        fprintf(stderr, "%s: expected decimal text, got null or empty\n", context);
        return 1;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            fprintf(stderr, "%s: expected decimal text, got %s\n", context, text);
            return 1;
        }
    }
    return 0;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related_path)) {
        remove(related_path);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "%s: expected readable file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    if (bytes_read != size) {
        fprintf(stderr, "%s: failed to read %zu bytes\n", path, size);
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
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
