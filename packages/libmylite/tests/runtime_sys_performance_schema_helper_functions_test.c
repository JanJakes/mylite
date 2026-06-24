#include <mylite/mylite.h>

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
    insert_sql_capacity = 512,
    native_ps_sys_null_thread_value_index = 5,
    mysql_error_data_out_of_range = 1264,
    mysql_error_incorrect_parameter_count = 1582,
    mysql_error_truncated_wrong_value_for_field = 1366,
    mysql_error_invalid_argument_for_function = 3047,
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
    const char *context;
};

static int test_sys_ps_helper_no_source_and_dual(void);
static int test_sys_ps_helper_table_backed_contexts(void);
static int test_sys_ps_helper_diagnostics(void);
static int test_native_ps_helper_no_source_and_dual(void);
static int test_native_ps_helper_table_backed_contexts(void);
static int test_native_ps_helper_diagnostics(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int capture_connection_id(
    mylite_db *database,
    char *out_text,
    size_t out_text_size,
    const char *context
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_with_warning_count(
    mylite_db *database,
    struct expected_query expected,
    size_t expected_warning_count
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_sys_ps_helper_no_source_and_dual();
    failures += test_sys_ps_helper_table_backed_contexts();
    failures += test_sys_ps_helper_diagnostics();
    failures += test_native_ps_helper_no_source_and_dual();
    failures += test_native_ps_helper_table_backed_contexts();
    failures += test_native_ps_helper_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_sys_ps_helper_no_source_and_dual(void) {
    static const char *const columns[] = {
        "current_thread",
        "mapped_thread",
        "current_instr",
        "null_instr",
        "unknown_instr",
        "account",
        "missing_account",
        "account_enabled",
        "null_account_enabled",
        "consumer_thread",
        "consumer_wait",
        "disabled_inst",
        "disabled_timed",
        "enabled_inst",
        "memory_timed",
        "trx_info",
        "stack_json",
    };
    static const char stack_json[] =
        "{\"rankdir\": \"LR\",\"nodesep\": \"0.10\",\"stack_created\": "
        "\"1970-01-01 00:00:00\",\"mysql_version\": \"8.4.9\",\"mysql_user\": \"root@%\","
        "\"events\": []}";
    const char *values[] = {
        NULL,
        NULL,
        "YES",
        NULL,
        "UNKNOWN",
        "root@%",
        NULL,
        "YES",
        "YES",
        "YES",
        "NO",
        "NO",
        "NO",
        "YES",
        "NO",
        "[]",
        stack_json,
    };
    char path[test_path_capacity];
    char connection_id[connection_id_text_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures +=
        capture_connection_id(database, connection_id, sizeof(connection_id), "no-source id");
    values[0] = connection_id;
    values[1] = connection_id;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "sys.ps_thread_id(NULL) AS current_thread,"
                   "sys.ps_thread_id(CONNECTION_ID()) AS mapped_thread,"
                   "sys.ps_is_thread_instrumented(CONNECTION_ID()) AS current_instr,"
                   "sys.ps_is_thread_instrumented(NULL) AS null_instr,"
                   "sys.ps_is_thread_instrumented(999999) AS unknown_instr,"
                   "sys.ps_thread_account(sys.ps_thread_id(NULL)) AS account,"
                   "sys.ps_thread_account(999999) AS missing_account,"
                   "sys.ps_is_account_enabled('localhost','root') AS account_enabled,"
                   "sys.ps_is_account_enabled(NULL,NULL) AS null_account_enabled,"
                   "sys.ps_is_consumer_enabled('thread_instrumentation') AS consumer_thread,"
                   "sys.ps_is_consumer_enabled('events_waits_current') AS consumer_wait,"
                   "sys.ps_is_instrument_default_enabled("
                   "'wait/synch/mutex/pfs/LOCK_pfs_share_list') AS disabled_inst,"
                   "sys.ps_is_instrument_default_timed("
                   "'wait/synch/mutex/pfs/LOCK_pfs_share_list') AS disabled_timed,"
                   "sys.ps_is_instrument_default_enabled('statement/sql/select') "
                   "AS enabled_inst,"
                   "sys.ps_is_instrument_default_timed('memory/%') AS memory_timed,"
                   "sys.ps_thread_trx_info(sys.ps_thread_id(NULL)) AS trx_info,"
                   "sys.ps_thread_stack(NULL,0) AS stack_json "
                   "FROM DUAL",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "sys ps helper direct values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sys_ps_helper_table_backed_contexts(void) {
    static const char *const columns[] = {
        "id",
        "thread_id",
        "instrumented",
        "account",
        "consumer_enabled",
        "timed",
    };
    const char *values[] = {
        "1",
        NULL,
        "YES",
        "root@%",
        "NO",
        "NO",
        "2",
        NULL,
        "UNKNOWN",
        NULL,
        "NO",
        "NO",
    };
    char path[test_path_capacity];
    char connection_id[connection_id_text_capacity];
    char insert_sql[insert_sql_capacity];
    mylite_db *database = NULL;
    int failures = 0;
    int written = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += capture_connection_id(database, connection_id, sizeof(connection_id), "table id");
    values[1] = connection_id;
    written = snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO ps_samples VALUES "
        "(1,%s,'memory/%%','events_statements_history'),"
        "(2,999999,'wait/synch/mutex/pfs/LOCK_pfs_share_list','events_waits_current')",
        connection_id
    );
    if (written < 0 || (size_t)written >= sizeof(insert_sql)) {
        fprintf(stderr, "insert SQL truncated\n");
        ++failures;
    }
    failures += execute_ok(
        database,
        "CREATE TABLE ps_samples("
        "id INT,"
        "connection_value BIGINT,"
        "instrument_name VARCHAR(128),"
        "consumer_name VARCHAR(64)"
        ")",
        NULL
    );
    if (failures == 0) {
        failures += execute_ok(database, insert_sql, NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id,"
                   "ps_thread_id(connection_value) AS thread_id,"
                   "ps_is_thread_instrumented(connection_value) AS instrumented,"
                   "ps_thread_account(connection_value) AS account,"
                   "ps_is_consumer_enabled(consumer_name) AS consumer_enabled,"
                   "ps_is_instrument_default_timed(instrument_name) AS timed "
                   "FROM ps_samples ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 2U,
            .context = "sys ps helper row values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sys_ps_helper_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_error(
        database,
        "SELECT sys.ps_is_consumer_enabled('not_a_consumer')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_argument_for_function,
            .sqlstate = "HY000",
            .message_part =
                "Invalid argument error: not_a_consumer in function sys.ps_is_consumer_enabled.",
        }
    );
    failures += execute_error(
        database,
        "SELECT sys.ps_is_thread_instrumented('abc')",
        (struct expected_sql_error){
            .code = mysql_error_truncated_wrong_value_for_field,
            .sqlstate = "HY000",
            .message_part = "Incorrect integer value: 'abc' for column 'in_connection_id' at row 1",
        }
    );
    failures += execute_error(
        database,
        "SELECT sys.ps_thread_id(-1)",
        (struct expected_sql_error){
            .code = mysql_error_data_out_of_range,
            .sqlstate = "22003",
            .message_part = "Out of range value for column 'in_connection_id' at row 1",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_ps_helper_no_source_and_dual(void) {
    static const char *const columns[] = {
        "current_thread",
        "mapped_thread",
        "null_thread",
        "missing_thread",
        "negative_thread",
        "sys_null_thread",
    };
    const char *values[] = {NULL, NULL, NULL, NULL, NULL, NULL};
    char path[test_path_capacity];
    char connection_id[connection_id_text_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "native-no-source", path, sizeof(path));
    failures += capture_connection_id(
        database,
        connection_id,
        sizeof(connection_id),
        "native no-source id"
    );
    values[0] = connection_id;
    values[1] = connection_id;
    values[native_ps_sys_null_thread_value_index] = connection_id;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "PS_CURRENT_THREAD_ID() AS current_thread,"
                   "PS_THREAD_ID(CONNECTION_ID()) AS mapped_thread,"
                   "PS_THREAD_ID(NULL) AS null_thread,"
                   "PS_THREAD_ID(999999) AS missing_thread,"
                   "PS_THREAD_ID(-1) AS negative_thread,"
                   "sys.ps_thread_id(NULL) AS sys_null_thread "
                   "FROM DUAL",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "native ps helper direct values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_ps_helper_table_backed_contexts(void) {
    static const char *const columns[] = {"id", "mapped_thread", "missing_thread"};
    const char *values[] = {
        "1",
        NULL,
        NULL,
        "2",
        NULL,
        NULL,
    };
    char path[test_path_capacity];
    char connection_id[connection_id_text_capacity];
    char insert_sql[insert_sql_capacity];
    mylite_db *database = NULL;
    int failures = 0;
    int written = 0;

    failures += open_app_database(&database, "native-table", path, sizeof(path));
    failures +=
        capture_connection_id(database, connection_id, sizeof(connection_id), "native table id");
    values[1] = connection_id;
    written = snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO ps_native_samples VALUES (1,%s,999999),(2,999999,0)",
        connection_id
    );
    if (written < 0 || (size_t)written >= sizeof(insert_sql)) {
        fprintf(stderr, "native insert SQL truncated\n");
        ++failures;
    }
    failures += execute_ok(
        database,
        "CREATE TABLE ps_native_samples("
        "id INT,"
        "connection_value BIGINT,"
        "missing_value BIGINT"
        ")",
        NULL
    );
    if (failures == 0) {
        failures += execute_ok(database, insert_sql, NULL);
    }
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id,"
                   "PS_THREAD_ID(connection_value) AS mapped_thread,"
                   "PS_THREAD_ID(missing_value) AS missing_thread "
                   "FROM ps_native_samples ORDER BY id",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 2U,
            .context = "native ps helper row values",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_ps_helper_diagnostics(void) {
    static const char *const invalid_columns[] = {"abc_thread", "prefix_thread"};
    static const char *const invalid_values[] = {NULL, NULL};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_values[] = {
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: 'abc'",
        "Warning",
        "1292",
        "Truncated incorrect INTEGER value: '1abc'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "native-diagnostics", path, sizeof(path));
    failures += expect_query_with_warning_count(
        database,
        (struct expected_query){
            .sql = "SELECT PS_THREAD_ID('abc') AS abc_thread, "
                   "PS_THREAD_ID('1abc') AS prefix_thread",
            .columns = invalid_columns,
            .column_count = sizeof(invalid_columns) / sizeof(invalid_columns[0]),
            .values = invalid_values,
            .row_count = 1U,
            .context = "native ps invalid values",
        },
        2U
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 2U,
            .context = "native ps warnings",
        }
    );
    failures += execute_error(
        database,
        "SELECT PS_CURRENT_THREAD_ID(1)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'PS_CURRENT_THREAD_ID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT PS_THREAD_ID()",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'PS_THREAD_ID'",
        }
    );
    failures += execute_error(
        database,
        "SELECT PS_THREAD_ID(1,2)",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'PS_THREAD_ID'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (make_test_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int capture_connection_id(
    mylite_db *database,
    char *out_text,
    size_t out_text_size,
    const char *context
) {
    mylite_result *result = NULL;
    const char *value = NULL;
    int failures = execute_ok(database, "SELECT CONNECTION_ID()", &result);
    int written = 0;

    if (failures == 0) {
        failures += expect_size(mylite_result_row_count(result), 1U, context);
        failures += expect_size(mylite_result_column_count(result), 1U, context);
    }
    if (failures == 0) {
        value = mylite_result_value_text(result, 0U, 0U);
        written = snprintf(out_text, out_text_size, "%s", value == NULL ? "" : value);
        if (written < 0 || (size_t)written >= out_text_size) {
            fprintf(stderr, "%s: connection id truncated\n", context);
            ++failures;
        }
    }
    mylite_result_free(result);
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
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    return expect_query_with_warning_count(database, expected, 0U);
}

static int expect_query_with_warning_count(
    mylite_db *database,
    struct expected_query expected,
    size_t expected_warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures +=
        expect_size(mylite_result_warning_count(result), expected_warning_count, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += expect_text(
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

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-sys-performance-schema-helper-functions-%s-%d.mylite",
        name,
        current_process_id()
    );

    return written < 0 || (size_t)written >= path_size ? 1 : 0;
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: row %zu column %zu expected NULL, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
