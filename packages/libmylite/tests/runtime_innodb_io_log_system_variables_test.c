#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_cant_set_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
    sql_buffer_capacity = 512,
};

enum innodb_io_log_variable_scope {
    innodb_io_log_variable_scope_dynamic_global = 0,
    innodb_io_log_variable_scope_dynamic_session = 1,
    innodb_io_log_variable_scope_read_only = 2,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct innodb_io_log_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_assignment;
    const char *unsupported_assignment;
    const char *session_assignment;
    enum innodb_io_log_variable_scope scope;
    bool boolean_value;
};

static const struct innodb_io_log_variable innodb_io_log_variables[] = {
    {"innodb_idle_flush_pct",
     "100",
     "100",
     "100",
     "101",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_io_capacity",
     "10000",
     "10000",
     "10000",
     "10001",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_io_capacity_max",
     "20000",
     "20000",
     "20000",
     "20001",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_lock_wait_timeout",
     "50",
     "50",
     "50",
     "51",
     "49",
     innodb_io_log_variable_scope_dynamic_session,
     false},
    {"innodb_log_buffer_size",
     "67108864",
     "67108864",
     "67108864",
     "67108865",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_log_checksums",
     "1",
     "ON",
     "ON",
     "OFF",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     true},
    {"innodb_log_compressed_pages",
     "1",
     "ON",
     "ON",
     "OFF",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     true},
    {"innodb_log_file_size",
     "50331648",
     "50331648",
     NULL,
     NULL,
     NULL,
     innodb_io_log_variable_scope_read_only,
     false},
    {"innodb_log_files_in_group",
     "2",
     "2",
     NULL,
     NULL,
     NULL,
     innodb_io_log_variable_scope_read_only,
     false},
    {"innodb_log_group_home_dir",
     "./",
     "./",
     NULL,
     NULL,
     NULL,
     innodb_io_log_variable_scope_read_only,
     false},
    {"innodb_log_spin_cpu_abs_lwm",
     "80",
     "80",
     "80",
     "81",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_log_spin_cpu_pct_hwm",
     "50",
     "50",
     "50",
     "51",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_log_wait_for_flush_spin_hwm",
     "400",
     "400",
     "400",
     "401",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_log_write_ahead_size",
     "8192",
     "8192",
     "8192",
     "8193",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
    {"innodb_log_writer_threads",
     "1",
     "ON",
     "ON",
     "OFF",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     true},
    {"innodb_lru_scan_depth",
     "1024",
     "1024",
     "1024",
     "1025",
     NULL,
     innodb_io_log_variable_scope_dynamic_global,
     false},
};

static int test_values_show_and_scope(void);
static int test_set_diagnostics(void);
static int test_session_mutation(void);
static int test_user_variable_assignments(void);
static bool innodb_io_log_variable_allows_session_scope(
    const struct innodb_io_log_variable *variable
);
static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);

int main(void) {
    int failures = 0;

    failures += test_values_show_and_scope();
    failures += test_set_diagnostics();
    failures += test_session_mutation();
    failures += test_user_variable_assignments();

    return failures == 0 ? 0 : 1;
}

static int test_values_show_and_scope(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB IO/log db");
    for (size_t index = 0U;
         index < sizeof(innodb_io_log_variables) / sizeof(innodb_io_log_variables[0]);
         ++index) {
        const struct innodb_io_log_variable *variable = &innodb_io_log_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s, @@GLOBAL.%s", variable->name, variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->scalar_value, variable->scalar_value},
            2U,
            variable->name
        );

        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->name, variable->show_value},
            2U,
            variable->name
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->name, variable->show_value},
            2U,
            variable->name
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->name, variable->show_value},
            2U,
            variable->name
        );

        if (innodb_io_log_variable_allows_session_scope(variable)) {
            snprintf(
                sql,
                sizeof(sql),
                "SELECT @@SESSION.%s, @@LOCAL.%s",
                variable->name,
                variable->name
            );
            failures += expect_values(
                database,
                sql,
                (const char *[]){variable->scalar_value, variable->scalar_value},
                2U,
                variable->name
            );
        } else {
            snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
            failures += execute_error(database, sql, global_only_read);
            snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", variable->name);
            failures += execute_error(database, sql, global_only_read);
        }
    }

    mylite_close(database);
    return failures;
}

static int test_set_diagnostics(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error read_only_set = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error unsupported_fixed_noop = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB IO/log SET db"
    );
    for (size_t index = 0U;
         index < sizeof(innodb_io_log_variables) / sizeof(innodb_io_log_variables[0]);
         ++index) {
        const struct innodb_io_log_variable *variable = &innodb_io_log_variables[index];

        if (variable->scope == innodb_io_log_variable_scope_read_only) {
            snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, read_only_set);
            snprintf(sql, sizeof(sql), "SET SESSION %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, read_only_set);
            snprintf(sql, sizeof(sql), "SET LOCAL %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, read_only_set);
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, read_only_set);
            continue;
        }

        if (innodb_io_log_variable_allows_session_scope(variable)) {
            snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SET SESSION %s = DEFAULT", variable->name);
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SET LOCAL %s = DEFAULT", variable->name);
            failures += execute_statement_ok(database, sql);
        } else {
            snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, global_only_set);
            snprintf(sql, sizeof(sql), "SET SESSION %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, global_only_set);
            snprintf(sql, sizeof(sql), "SET LOCAL %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, global_only_set);
        }

        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
        failures += execute_statement_ok(database, sql);
        snprintf(
            sql,
            sizeof(sql),
            "SET GLOBAL %s = %s",
            variable->name,
            variable->exact_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(
            sql,
            sizeof(sql),
            "SET GLOBAL %s = %s",
            variable->name,
            variable->unsupported_assignment
        );
        failures += execute_error(database, sql, unsupported_fixed_noop);
    }

    mylite_close(database);
    return failures;
}

static int test_session_mutation(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB lock timeout db"
    );
    failures += execute_statement_ok(database, "SET innodb_lock_wait_timeout = 49");
    failures += expect_values(
        database,
        "SELECT @@innodb_lock_wait_timeout, @@SESSION.innodb_lock_wait_timeout, "
        "@@GLOBAL.innodb_lock_wait_timeout",
        (const char *[]){"49", "49", "50"},
        3U,
        "innodb_lock_wait_timeout session value"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'innodb_lock_wait_timeout'",
        (const char *[]){"innodb_lock_wait_timeout", "49"},
        2U,
        "innodb_lock_wait_timeout SHOW session"
    );
    failures += expect_values(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'innodb_lock_wait_timeout'",
        (const char *[]){"innodb_lock_wait_timeout", "50"},
        2U,
        "innodb_lock_wait_timeout SHOW global"
    );
    failures += execute_statement_ok(database, "SET LOCAL innodb_lock_wait_timeout = 50");
    failures += expect_values(
        database,
        "SELECT @@innodb_lock_wait_timeout, @@LOCAL.innodb_lock_wait_timeout",
        (const char *[]){"50", "50"},
        2U,
        "innodb_lock_wait_timeout local value"
    );
    failures += execute_statement_ok(database, "SET SESSION innodb_lock_wait_timeout = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@innodb_lock_wait_timeout",
        (const char *[]){"50"},
        1U,
        "innodb_lock_wait_timeout default"
    );

    mylite_close(database);
    return failures;
}

static int test_user_variable_assignments(void) {
    struct expected_sql_error read_only_set = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error unsupported_fixed_noop = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error cant_set_value = {
        .code = mysql_error_cant_set_value,
        .sqlstate = "42000",
        .message_part = "can't be set to the value",
    };
    struct expected_sql_error incorrect_argument_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB IO/log user-var db"
    );
    for (size_t index = 0U;
         index < sizeof(innodb_io_log_variables) / sizeof(innodb_io_log_variables[0]);
         ++index) {
        const struct innodb_io_log_variable *variable = &innodb_io_log_variables[index];

        if (variable->scope == innodb_io_log_variable_scope_read_only) {
            failures += execute_statement_ok(database, "SET @innodb_io_log_value = 0");
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_io_log_value", variable->name);
            failures += execute_error(database, sql, read_only_set);
            continue;
        }

        snprintf(sql, sizeof(sql), "SET @innodb_io_log_value = %s", variable->exact_assignment);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_io_log_value", variable->name);
        failures += execute_statement_ok(database, sql);

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_io_log_value = %s",
            variable->unsupported_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_io_log_value", variable->name);
        failures += execute_error(database, sql, unsupported_fixed_noop);

        if (variable->scope == innodb_io_log_variable_scope_dynamic_session) {
            snprintf(
                sql,
                sizeof(sql),
                "SET @innodb_io_log_value = %s",
                variable->session_assignment
            );
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SET %s = @innodb_io_log_value", variable->name);
            failures += execute_statement_ok(database, sql);
            failures += expect_values(
                database,
                "SELECT @@innodb_lock_wait_timeout",
                (const char *[]){"49"},
                1U,
                "innodb_lock_wait_timeout user-var session"
            );
        }

        failures += execute_statement_ok(database, "SET @innodb_io_log_value = 'bad'");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_io_log_value", variable->name);
        failures += execute_error(
            database,
            sql,
            variable->boolean_value ? cant_set_value : incorrect_argument_type
        );
    }

    mylite_close(database);
    return failures;
}

static bool innodb_io_log_variable_allows_session_scope(
    const struct innodb_io_log_variable *variable
) {
    return variable != NULL && variable->scope == innodb_io_log_variable_scope_dynamic_session;
}

static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK for [%s], got %d %s\n",
            context,
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), expected_count, context);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    for (size_t column = 0U; column < expected_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, column),
            expected[column],
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "expected OK for [%s], got %d %s\n", sql, rc, mylite_errmsg(database));
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}
