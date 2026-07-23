#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

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
    path_suffix_capacity = 16,
    connection_id_text_capacity = 32,
    mixed_database_index = 0,
    mixed_schema_index = 1,
    mixed_version_index = 2,
    mixed_connection_id_index = 3,
    mixed_last_insert_id_index = 4,
    mixed_literal_index = 5,
    mixed_null_index = 6,
    mixed_true_index = 7,
    mixed_if_index = 8,
    mixed_warning_count_index = 9,
    mixed_row_count_index = 10,
    mixed_column_count = 11,
    identity_column_count = 7,
    parenthesized_column_count = 5,
    warning_column_count = 6,
    double_warning_column_count = 4,
    mysql_error_parse = 1064,
    mysql_error_incorrect_parameter_count = 1582,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    size_t warning_count;
    const char *context;
};

static int test_session_value_scalar_projection_values_and_file_safety(void);
static int test_session_value_scalar_projection_warning_order(void);
static int test_session_value_scalar_projection_unsupported_forms(void);
static int test_session_value_scalar_projection_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
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

    failures += test_session_value_scalar_projection_values_and_file_safety();
    failures += test_session_value_scalar_projection_warning_order();
    failures += test_session_value_scalar_projection_unsupported_forms();
    failures += test_session_value_scalar_projection_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_session_value_scalar_projection_values_and_file_safety(void) {
    static const char *const mixed_columns[] = {
        "DATABASE()",
        "SCHEMA()",
        "VERSION()",
        "CONNECTION_ID()",
        "LAST_INSERT_ID()",
        "1",
        "NULL",
        "TRUE",
        "IF(1,2,3)",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const parenthesized_columns[] = {
        "(VERSION())",
        "1",
        "(IF(1,2,3))",
        "(@@warning_count)",
        "(DATABASE())",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()"};
    static const char *const row_count_values[] = {"-1"};
    char connection_id_text[connection_id_text_capacity];
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const char *mixed_values[mixed_column_count] = {0};
    const char *parenthesized_values[parenthesized_column_count] = {0};
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "values") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open values file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    if (snprintf(
            connection_id_text,
            sizeof(connection_id_text),
            "%llu",
            (unsigned long long)session->connection_id
        ) < 0) {
        failures += mylite_test_expect_int(1, 0, "format connection id");
    }

    mixed_values[mixed_database_index] = "app";
    mixed_values[mixed_schema_index] = "app";
    mixed_values[mixed_version_index] = MYLITE_MYSQL_SERVER_VERSION_STRING;
    mixed_values[mixed_connection_id_index] = connection_id_text;
    mixed_values[mixed_last_insert_id_index] = "0";
    mixed_values[mixed_literal_index] = "1";
    mixed_values[mixed_null_index] = NULL;
    mixed_values[mixed_true_index] = "1";
    mixed_values[mixed_if_index] = "2";
    mixed_values[mixed_warning_count_index] = "0";
    mixed_values[mixed_row_count_index] = "0";
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ALL DATABASE(), SCHEMA(), VERSION(), CONNECTION_ID(), "
                   "LAST_INSERT_ID(), 1, NULL, TRUE, IF(1,2,3), @@warning_count, "
                   "ROW_COUNT() FROM DUAL",
            .columns = mixed_columns,
            .column_count = mixed_column_count,
            .values = mixed_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "mixed session and scalar values",
        }
    );

    parenthesized_values[0] = MYLITE_MYSQL_SERVER_VERSION_STRING;
    parenthesized_values[1] = "1";
    parenthesized_values[2] = "2";
    parenthesized_values[3] = "0";
    parenthesized_values[4] = "app";
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT (VERSION()), (1), (IF(1,2,3)), (@@warning_count), "
                   "(DATABASE())",
            .columns = parenthesized_columns,
            .column_count = parenthesized_column_count,
            .values = parenthesized_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "parenthesized mixed scalar labels",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .columns = row_count_columns,
            .column_count = 1U,
            .values = row_count_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "row count after mixed scalar projection",
        }
    );

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "mixed scalar select leaves catalog generation unchanged"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "mixed scalar select leaves sqlite schema generation unchanged"
    );

    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read mixed scalar preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "mixed scalar select leaves preamble unchanged"
    );

    remove_related_files(path);
    return failures;
}

static int test_session_value_scalar_projection_warning_order(void) {
    static const char *const warning_columns[] = {
        "@@sql_slave_skip_counter",
        "1",
        "IF(1,2,3)",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"0", "1", "2", "1", "0", "-1"};
    static const char *const diagnostic_columns[] = {"@@warning_count", "ROW_COUNT()"};
    static const char *const diagnostic_values[] = {"1", "-1"};
    static const char *const double_warning_columns[] = {
        "@@sql_slave_skip_counter",
        "@@global.sql_slave_skip_counter",
        "1",
        "@@warning_count",
    };
    static const char *const double_warning_values[] = {"0", "0", "1", "2"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open warning memory");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT 1",
            .columns = (const char *const[]){"1"},
            .column_count = 1U,
            .values = (const char *const[]){"1"},
            .row_count = 1U,
            .warning_count = 0U,
            .context = "seed previous row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@sql_slave_skip_counter, 1, IF(1,2,3), @@warning_count, "
                   "@@error_count, ROW_COUNT()",
            .columns = warning_columns,
            .column_count = warning_column_count,
            .values = warning_values,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "mixed scalar warning sequencing",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@warning_count, ROW_COUNT()",
            .columns = diagnostic_columns,
            .column_count = 2U,
            .values = diagnostic_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "mixed scalar warning diagnostics",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@sql_slave_skip_counter, @@global.sql_slave_skip_counter, 1, "
                   "@@warning_count",
            .columns = double_warning_columns,
            .column_count = double_warning_column_count,
            .values = double_warning_values,
            .row_count = 1U,
            .warning_count = 2U,
            .context = "two deprecated system variable reads",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_session_value_scalar_projection_unsupported_forms(void) {
    static const char *const connection_id_table_columns[] = {"CONNECTION_ID()", "id"};
    static const char *const version_literal_columns[] = {"VERSION()", "1"};
    static const char *const version_table_columns[] = {"VERSION()", "id"};
    static const char *const current_database_table_columns[] = {"DATABASE()", "SCHEMA()", "id"};
    static const char *const identity_table_columns[] = {
        "USER()",
        "CURRENT_USER",
        "CURRENT_USER()",
        "SESSION_USER()",
        "SYSTEM_USER()",
        "CURRENT_ROLE()",
        "id",
    };
    static const char *const current_database_table_values[] = {
        "app",
        "app",
        "1",
        "app",
        "app",
        "2",
    };
    const char *version_literal_values[] = {MYLITE_MYSQL_SERVER_VERSION_STRING, "1"};
    const char *version_table_values[] = {
        MYLITE_MYSQL_SERVER_VERSION_STRING,
        "1",
        MYLITE_MYSQL_SERVER_VERSION_STRING,
        "2",
    };
    static const char *const identity_table_values[] = {
        "root@%",
        "root@%",
        "root@%",
        "root@%",
        "root@%",
        "NONE",
        "1",
        "root@%",
        "root@%",
        "root@%",
        "root@%",
        "root@%",
        "NONE",
        "2",
    };
    const struct mylite_session_state *session = NULL;
    char connection_id_text[connection_id_text_capacity];
    const char *connection_id_table_values[] = {
        connection_id_text,
        "1",
        connection_id_text,
        "2",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "unsupported") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open unsupported file");
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(database, "CREATE TABLE t(id INT)", NULL);
    failures += execute_ok(database, "INSERT INTO t VALUES (1)", NULL);
    session = mylite_connection_session_state(database);
    if (snprintf(
            connection_id_text,
            sizeof(connection_id_text),
            "%llu",
            (unsigned long long)session->connection_id
        ) < 0) {
        failures += mylite_test_expect_int(1, 0, "format connection id");
    }

    failures += execute_error(
        database,
        "SELECT VERSION(1), 1",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function 'VERSION'",
        }
    );
    failures += execute_error(
        database,
        "SELECT VERSION(), 1.0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT scalar projection supports only session scalar values",
        }
    );
    failures += execute_error(
        database,
        "SELECT IF(@@warning_count,1,0), 2",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT IF() supports only signed 64-bit integer",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE(), 1 WHERE TRUE",
            .columns = (const char *const[]){"DATABASE()", "1"},
            .column_count = 2U,
            .values = (const char *const[]){"app", "1"},
            .row_count = 1U,
            .warning_count = 0U,
            .context = "tableless session scalar with where true",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VERSION(), 1 FROM t",
            .columns = version_literal_columns,
            .column_count = 2U,
            .values = version_literal_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "table-backed session scalar and literal projection",
        }
    );
    failures += execute_ok(database, "INSERT INTO t VALUES (2)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT CONNECTION_ID(), id FROM t "
                   "WHERE CONNECTION_ID() = CONNECTION_ID() "
                   "ORDER BY CONNECTION_ID(), id",
            .columns = connection_id_table_columns,
            .column_count = 2U,
            .values = connection_id_table_values,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "source-backed connection id predicate and order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VERSION(), id FROM t WHERE VERSION() = VERSION() "
                   "ORDER BY VERSION(), id",
            .columns = version_table_columns,
            .column_count = 2U,
            .values = version_table_values,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "source-backed version predicate and order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE(), SCHEMA(), id FROM t WHERE DATABASE() = 'app' "
                   "ORDER BY SCHEMA(), id",
            .columns = current_database_table_columns,
            .column_count = 3U,
            .values = current_database_table_values,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "source-backed current database predicate and order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT USER(), CURRENT_USER, CURRENT_USER(), SESSION_USER(), SYSTEM_USER(), "
                   "CURRENT_ROLE(), id FROM t "
                   "WHERE USER() = SESSION_USER() AND CURRENT_USER = CURRENT_USER() "
                   "AND SYSTEM_USER() = USER() AND CURRENT_ROLE() = CURRENT_ROLE() "
                   "ORDER BY USER(), CURRENT_USER, SESSION_USER(), SYSTEM_USER(), "
                   "CURRENT_ROLE(), id",
            .columns = identity_table_columns,
            .column_count = identity_column_count,
            .values = identity_table_values,
            .row_count = 2U,
            .warning_count = 0U,
            .context = "source-backed identity predicate and order",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_session_value_scalar_projection_independent_handles(void) {
    static const char *const first_columns[] = {"VERSION()", "1"};
    static const char *const second_columns[] = {"DATABASE()", "IFNULL(NULL,4)"};
    static const char *const second_values[] = {"second_app", "4"};
    const char *first_values[] = {NULL, "1"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first-handle") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second-handle") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    first_values[0] = MYLITE_MYSQL_SERVER_VERSION_STRING;
    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first file handle"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second file handle"
    );
    failures += execute_ok(second, "CREATE DATABASE second_app", NULL);
    failures += execute_ok(second, "USE second_app", NULL);
    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT VERSION(), 1",
            .columns = first_columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "first handle mixed scalar",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT DATABASE(), IFNULL(NULL,4)",
            .columns = second_columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "second handle mixed scalar",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
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

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_size = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    read_size = fread(buffer, 1U, size, file);
    fclose(file);

    return read_size == size ? 0 : 1;
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
