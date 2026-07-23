#include "mylite_test_support.h"

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
    mysql_error_unknown_database = 1049,
    mysql_error_routine_does_not_exist = 1305,
    mysql_error_trigger_does_not_exist = 1360,
    mysql_error_operation_failed = 1396,
    mysql_error_unknown_event = 1539,
    show_create_user_column_count = 1,
};

struct expected_sql_error {
    const char *sql;
    int code;
    const char *sqlstate;
    const char *message_part;
};

static int test_show_create_user_embedded_root(void);
static int test_show_create_residual_missing_objects(void);
static int expect_show_create_user(mylite_db *database, const char *sql);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(void) {
    int failures = 0;

    failures += test_show_create_user_embedded_root();
    failures += test_show_create_residual_missing_objects();

    return failures == 0 ? 0 : 1;
}

static int test_show_create_user_embedded_root(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open show create user");
    failures += expect_show_create_user(database, "SHOW CREATE USER CURRENT_USER");
    failures += expect_show_create_user(database, "SHOW CREATE USER CURRENT_USER()");
    failures += expect_show_create_user(database, "SHOW CREATE USER root");
    failures += expect_show_create_user(database, "SHOW CREATE USER 'root'@'%'");
    failures += expect_show_create_user(database, "SHOW CREATE USER root@'%'");
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE USER 'no_such_mylite_user'@'%'",
            .code = mysql_error_operation_failed,
            .sqlstate = "HY000",
            .message_part = "Operation SHOW CREATE USER failed for 'no_such_mylite_user'@'%'",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE USER root@",
            .code = mysql_error_operation_failed,
            .sqlstate = "HY000",
            .message_part = "Operation SHOW CREATE USER failed for 'root'@''",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_show_create_residual_missing_objects(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "residuals") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open show create residuals"
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE FUNCTION no_such_function",
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE EVENT no_such_event",
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE TRIGGER no_such_trigger",
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE FUNCTION no_such_function",
            .code = mysql_error_routine_does_not_exist,
            .sqlstate = "42000",
            .message_part = "FUNCTION no_such_function does not exist",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE FUNCTION no_such_schema.no_such_function",
            .code = mysql_error_routine_does_not_exist,
            .sqlstate = "42000",
            .message_part = "FUNCTION no_such_function does not exist",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE EVENT no_such_event",
            .code = mysql_error_unknown_event,
            .sqlstate = "HY000",
            .message_part = "Unknown event 'no_such_event'",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE EVENT no_such_schema.no_such_event",
            .code = mysql_error_unknown_event,
            .sqlstate = "HY000",
            .message_part = "Unknown event 'no_such_event'",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE TRIGGER no_such_trigger",
            .code = mysql_error_trigger_does_not_exist,
            .sqlstate = "HY000",
            .message_part = "Trigger does not exist",
        }
    );
    failures += execute_error(
        database,
        (struct expected_sql_error){
            .sql = "SHOW CREATE TRIGGER no_such_schema.no_such_trigger",
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database 'no_such_schema'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_show_create_user(mylite_db *database, const char *sql) {
    static const char *const expected_column = "CREATE USER for root@%";
    static const char *const expected_value =
        "CREATE USER `root`@`%` IDENTIFIED WITH 'caching_sha2_password' REQUIRE NONE PASSWORD "
        "EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT "
        "PASSWORD REQUIRE CURRENT DEFAULT";
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (result == NULL) {
        return failures + 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        show_create_user_column_count,
        "show create user column count"
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), 1U, "show create user row count");
    failures += mylite_test_expect_text_or_null(
        mylite_result_column_name(result, 0U),
        expected_column,
        "show create user column"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected_value,
        "show create user value"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "show create user warning count"
    );
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

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
    return 0;
}

static int execute_error(mylite_db *database, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

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

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char buffer[test_path_capacity + path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return;
    }
    (void)remove(buffer);
}
