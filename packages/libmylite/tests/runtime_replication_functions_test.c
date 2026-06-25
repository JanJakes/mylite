#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_no_database_selected = 1046,
    mysql_error_routine_does_not_exist = 1305,
    mysql_error_incorrect_parameter_count = 1582,
    mysql_error_gtid_wait_with_gtid_mode_off = 3062,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_async_failover_functions(void);
static int test_position_wait_functions(void);
static int test_replication_function_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int open_file_database(mylite_db **out_database, char *path, size_t path_size);
static int expect_single_value(mylite_db *database, const char *sql, const char *expected);
static int expect_null_value(mylite_db *database, const char *sql);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_async_failover_functions();
    failures += test_position_wait_functions();
    failures += test_replication_function_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_async_failover_functions(void) {
    static const char *const cases[][2] = {
        {
            "SELECT asynchronous_connection_failover_add_source("
            "'mylite_rf_channel', '127.0.0.1', 3310, '', 80) AS value",
            "The UDF asynchronous_connection_failover_add_source() executed successfully.",
        },
        {
            "SELECT asynchronous_connection_failover_delete_source("
            "'mylite_rf_channel', '127.0.0.1', 3310, '') AS value",
            "The UDF asynchronous_connection_failover_delete_source() executed successfully.",
        },
        {
            "SELECT asynchronous_connection_failover_add_managed("
            "'mylite_rf_channel',"
            "'GroupReplication',"
            "'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa',"
            "'127.0.0.1',"
            "3310,"
            "'',"
            "80,"
            "60) AS value",
            "The UDF asynchronous_connection_failover_add_managed() executed successfully.",
        },
        {
            "SELECT asynchronous_connection_failover_delete_managed("
            "'mylite_rf_channel', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa') AS value",
            "The UDF asynchronous_connection_failover_delete_managed() executed successfully.",
        },
        {
            "SELECT asynchronous_connection_failover_reset() AS value",
            "The UDF asynchronous_connection_failover_reset() executed successfully.",
        },
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    for (size_t index = 0U; database != NULL && index < sizeof(cases) / sizeof(cases[0]); ++index) {
        failures += expect_single_value(database, cases[index][0], cases[index][1]);
    }

    mylite_close(database);
    return failures;
}

static int test_position_wait_functions(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    if (database != NULL) {
        failures +=
            expect_null_value(database, "SELECT MASTER_POS_WAIT('binlog.000001', 4, 0) AS value");
        failures +=
            expect_null_value(database, "SELECT SOURCE_POS_WAIT('binlog.000001', 4, 0) AS value");
    }

    mylite_close(database);
    return failures;
}

static int test_replication_function_diagnostics(void) {
    char path[test_path_capacity] = "";
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open memory database");
    if (database != NULL) {
        failures += execute_error(
            database,
            "SELECT WAIT_FOR_EXECUTED_GTID_SET('', 0)",
            (struct expected_sql_error){
                .code = mysql_error_gtid_wait_with_gtid_mode_off,
                .sqlstate = "HY000",
                .message_part = "Cannot use WAIT_FOR_EXECUTED_GTID_SET when GTID_MODE = OFF.",
            }
        );
        failures += execute_error(
            database,
            "SELECT WAIT_FOR_EXECUTED_GTID_SET()",
            (struct expected_sql_error){
                .code = mysql_error_incorrect_parameter_count,
                .sqlstate = "42000",
                .message_part = "Incorrect parameter count",
            }
        );
        failures += execute_error(
            database,
            "SELECT group_replication_get_communication_protocol()",
            (struct expected_sql_error){
                .code = mysql_error_no_database_selected,
                .sqlstate = "3D000",
                .message_part = "No database selected",
            }
        );
    }
    mylite_close(database);
    database = NULL;

    failures += open_file_database(&database, path, sizeof(path));
    if (database != NULL) {
        failures += execute_error(
            database,
            "SELECT group_replication_get_communication_protocol()",
            (struct expected_sql_error){
                .code = mysql_error_routine_does_not_exist,
                .sqlstate = "42000",
                .message_part =
                    "FUNCTION app.group_replication_get_communication_protocol does not exist",
            }
        );
    }

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL) {
        mylite_result_free(result);
    } else {
        *out_result = result;
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        return 1;
    }
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int open_file_database(mylite_db **out_database, char *path, size_t path_size) {
    int failures = 0;

    if (make_test_path(path, path_size) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, out_database), MYLITE_OK, "open file database");
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int expect_single_value(mylite_db *database, const char *sql, const char *expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result != NULL) {
        failures += expect_int((int)mylite_result_column_count(result), 1, "column count");
        failures += expect_int((int)mylite_result_row_count(result), 1, "row count");
        failures += expect_text(mylite_result_value_text(result, 0U, 0U), expected, sql);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_null_value(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result != NULL) {
        failures += expect_int((int)mylite_result_column_count(result), 1, "column count");
        failures += expect_int((int)mylite_result_row_count(result), 1, "row count");
        if (mylite_result_value_text(result, 0U, 0U) != NULL) {
            fprintf(
                stderr,
                "%s: expected NULL, got [%s]\n",
                sql,
                mylite_result_value_text(result, 0U, 0U)
            );
            ++failures;
        }
        failures += expect_int(
            (int)mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            "position wait result type"
        );
        failures += expect_int(mylite_result_column_nullable(result, 0U), 1, "nullable result");
    }
    mylite_result_free(result);
    return failures;
}

static int make_test_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite-replication-functions-%d.mylite",
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
    if (path == NULL || path[0] == '\0') {
        return;
    }
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

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}
