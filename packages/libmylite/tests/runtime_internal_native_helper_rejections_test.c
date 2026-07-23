#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_native_function_rejected = 3566,
    native_rejection_sql_capacity = 256,
    native_rejection_message_capacity = 128,
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
    const char *context;
};

static int test_internal_native_helper_rejections(void);
static int open_app_database(mylite_db **out_database);
static int test_rejected_names(mylite_db *database);
static int test_rejected_contexts(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, struct expected_sql_error expected);

int main(void) {
    return test_internal_native_helper_rejections() == 0 ? 0 : 1;
}

static int test_internal_native_helper_rejections(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database);
    if (database != NULL) {
        failures += test_rejected_names(database);
        failures += test_rejected_contexts(database);
    }
    mylite_close(database);
    return failures;
}

static int open_app_database(mylite_db **out_database) {
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_test_open_temporary(out_database),
        MYLITE_OK,
        "open database"
    );
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app");
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app");
    }
    return failures;
}

static int test_rejected_names(mylite_db *database) {
    static const char *const names[] = {
        "CAN_ACCESS_COLUMN",
        "CAN_ACCESS_DATABASE",
        "CAN_ACCESS_TABLE",
        "CAN_ACCESS_USER",
        "CAN_ACCESS_VIEW",
        "GET_DD_COLUMN_PRIVILEGES",
        "GET_DD_CREATE_OPTIONS",
        "GET_DD_INDEX_SUB_PART_LENGTH",
        "INTERNAL_AUTO_INCREMENT",
        "INTERNAL_AVG_ROW_LENGTH",
        "INTERNAL_CHECK_TIME",
        "INTERNAL_CHECKSUM",
        "INTERNAL_DATA_FREE",
        "INTERNAL_DATA_LENGTH",
        "INTERNAL_DD_CHAR_LENGTH",
        "INTERNAL_GET_COMMENT_OR_ERROR",
        "INTERNAL_GET_ENABLED_ROLE_JSON",
        "INTERNAL_GET_HOSTNAME",
        "INTERNAL_GET_USERNAME",
        "INTERNAL_GET_VIEW_WARNING_OR_ERROR",
        "INTERNAL_INDEX_COLUMN_CARDINALITY",
        "INTERNAL_INDEX_LENGTH",
        "INTERNAL_IS_ENABLED_ROLE",
        "INTERNAL_IS_MANDATORY_ROLE",
        "INTERNAL_KEYS_DISABLED",
        "INTERNAL_MAX_DATA_LENGTH",
        "INTERNAL_TABLE_ROWS",
        "INTERNAL_UPDATE_TIME",
    };
    char sql[native_rejection_sql_capacity];
    char message[native_rejection_message_capacity];
    int failures = 0;

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        int written = snprintf(sql, sizeof(sql), "SELECT %s('a','b','c')", names[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            fprintf(stderr, "native rejection SQL truncated for %s\n", names[index]);
            ++failures;
            continue;
        }
        written = snprintf(
            message,
            sizeof(message),
            "Access to native function '%s' is rejected.",
            names[index]
        );
        if (written < 0 || (size_t)written >= sizeof(message)) {
            fprintf(stderr, "native rejection message truncated for %s\n", names[index]);
            ++failures;
            continue;
        }
        failures += execute_error(
            database,
            (struct expected_sql_error){
                .sql = sql,
                .code = mysql_error_native_function_rejected,
                .sqlstate = "HY000",
                .message_part = message,
                .context = names[index],
            }
        );
    }
    return failures;
}

static int test_rejected_contexts(mylite_db *database) {
    int failures = 0;

    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT CAN_ACCESS_TABLE()",
            .code = mysql_error_native_function_rejected,
            .sqlstate = "HY000",
            .message_part = "Access to native function 'CAN_ACCESS_TABLE' is rejected.",
            .context = "zero arity native rejection",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT INTERNAL_TABLE_ROWS(1,2,3,4,5,6,7,8,9,10)",
            .code = mysql_error_native_function_rejected,
            .sqlstate = "HY000",
            .message_part = "Access to native function 'INTERNAL_TABLE_ROWS' is rejected.",
            .context = "too many arguments native rejection",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT GET_DD_CREATE_OPTIONS('')",
            .code = mysql_error_native_function_rejected,
            .sqlstate = "HY000",
            .message_part = "Access to native function 'GET_DD_CREATE_OPTIONS' is rejected.",
            .context = "too few arguments native rejection",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT can_access_table()",
            .code = mysql_error_native_function_rejected,
            .sqlstate = "HY000",
            .message_part = "Access to native function 'can_access_table' is rejected.",
            .context = "native rejection preserves function name case",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT GET_DD_CREATE_OPTIONS('',0,0) FROM DUAL",
            .code = mysql_error_native_function_rejected,
            .sqlstate = "HY000",
            .message_part = "Access to native function 'GET_DD_CREATE_OPTIONS' is rejected.",
            .context = "DUAL native rejection",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "DO INTERNAL_KEYS_DISABLED('')",
            .code = mysql_error_native_function_rejected,
            .sqlstate = "HY000",
            .message_part = "Access to native function 'INTERNAL_KEYS_DISABLED' is rejected.",
            .context = "DO native rejection",
        }
    );
    failures += execute_ok(database, "CREATE TABLE native_rejection_probe(id INT)");
    failures += execute_ok(database, "INSERT INTO native_rejection_probe VALUES (1)");
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SELECT CAN_ACCESS_DATABASE(id) FROM native_rejection_probe",
            .code = mysql_error_native_function_rejected,
            .sqlstate = "HY000",
            .message_part = "Access to native function 'CAN_ACCESS_DATABASE' is rejected.",
            .context = "row-backed native rejection",
        }
    );
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            rc,
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", expected.context);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, expected.context);
    failures +=
        mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, expected.context);
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        expected.context
    );
    mylite_result_free(result);
    return failures;
}
