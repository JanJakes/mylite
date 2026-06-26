#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_cant_set_variable = 1231,
    mysql_error_session_variable_only = 1238,
    sql_buffer_capacity = 512,
};

enum innodb_stats_status_thread_undo_variable_scope {
    innodb_stats_status_thread_undo_variable_scope_dynamic_numeric = 0,
    innodb_stats_status_thread_undo_variable_scope_dynamic_boolean = 1,
    innodb_stats_status_thread_undo_variable_scope_dynamic_text = 2,
    innodb_stats_status_thread_undo_variable_scope_dynamic_session_boolean = 3,
    innodb_stats_status_thread_undo_variable_scope_dynamic_session_null_text = 4,
    innodb_stats_status_thread_undo_variable_scope_read_only = 5,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct innodb_stats_status_thread_undo_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_assignment;
    const char *alternate_assignment;
    enum innodb_stats_status_thread_undo_variable_scope scope;
};

static const struct innodb_stats_status_thread_undo_variable
    innodb_stats_status_thread_undo_variables[] = {
        {"innodb_stats_auto_recalc",
         "1",
         "ON",
         "ON",
         "OFF",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_stats_include_delete_marked",
         "0",
         "OFF",
         "OFF",
         "ON",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_stats_method",
         "nulls_equal",
         "nulls_equal",
         "'nulls_equal'",
         "'nulls_unequal'",
         innodb_stats_status_thread_undo_variable_scope_dynamic_text},
        {"innodb_stats_on_metadata",
         "0",
         "OFF",
         "OFF",
         "ON",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_stats_persistent",
         "1",
         "ON",
         "ON",
         "OFF",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_stats_persistent_sample_pages",
         "20",
         "20",
         "20",
         "21",
         innodb_stats_status_thread_undo_variable_scope_dynamic_numeric},
        {"innodb_stats_transient_sample_pages",
         "8",
         "8",
         "8",
         "9",
         innodb_stats_status_thread_undo_variable_scope_dynamic_numeric},
        {"innodb_status_output",
         "0",
         "OFF",
         "OFF",
         "ON",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_status_output_locks",
         "0",
         "OFF",
         "OFF",
         "ON",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_strict_mode",
         "1",
         "ON",
         "ON",
         "OFF",
         innodb_stats_status_thread_undo_variable_scope_dynamic_session_boolean},
        {"innodb_sync_array_size",
         "1",
         "1",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
        {"innodb_sync_spin_loops",
         "30",
         "30",
         "30",
         "31",
         innodb_stats_status_thread_undo_variable_scope_dynamic_numeric},
        {"innodb_table_locks",
         "1",
         "ON",
         "ON",
         "OFF",
         innodb_stats_status_thread_undo_variable_scope_dynamic_session_boolean},
        {"innodb_temp_data_file_path",
         "ibtmp1:12M:autoextend",
         "ibtmp1:12M:autoextend",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
        {"innodb_temp_tablespaces_dir",
         "./#innodb_temp/",
         "./#innodb_temp/",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
        {"innodb_thread_concurrency",
         "0",
         "0",
         "0",
         "1",
         innodb_stats_status_thread_undo_variable_scope_dynamic_numeric},
        {"innodb_thread_sleep_delay",
         "10000",
         "10000",
         "10000",
         "10001",
         innodb_stats_status_thread_undo_variable_scope_dynamic_numeric},
        {"innodb_tmpdir",
         NULL,
         "",
         "NULL",
         "'/tmp'",
         innodb_stats_status_thread_undo_variable_scope_dynamic_session_null_text},
        {"innodb_undo_directory",
         "./",
         "./",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
        {"innodb_undo_log_encrypt",
         "0",
         "OFF",
         "OFF",
         "ON",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_undo_log_truncate",
         "1",
         "ON",
         "ON",
         "OFF",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_undo_tablespaces",
         "2",
         "2",
         "2",
         "3",
         innodb_stats_status_thread_undo_variable_scope_dynamic_numeric},
        {"innodb_use_fdatasync",
         "1",
         "ON",
         "ON",
         "OFF",
         innodb_stats_status_thread_undo_variable_scope_dynamic_boolean},
        {"innodb_use_native_aio",
         "1",
         "ON",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
        {"innodb_validate_tablespace_paths",
         "1",
         "ON",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
        {"innodb_version",
         "8.4.9",
         "8.4.9",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
        {"innodb_write_io_threads",
         "4",
         "4",
         NULL,
         NULL,
         innodb_stats_status_thread_undo_variable_scope_read_only},
};

static int test_values_show_and_scope(void);
static int test_set_diagnostics(void);
static int test_session_boolean_assignments(void);
static int test_tmpdir_assignments(void);
static int test_user_variable_assignments(void);
static int test_independent_session_handles(void);
static bool innodb_stats_status_thread_undo_variable_is_read_only(
    const struct innodb_stats_status_thread_undo_variable *variable
);
static bool innodb_stats_status_thread_undo_variable_is_session_scoped(
    const struct innodb_stats_status_thread_undo_variable *variable
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
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_values_show_and_scope();
    failures += test_set_diagnostics();
    failures += test_session_boolean_assignments();
    failures += test_tmpdir_assignments();
    failures += test_user_variable_assignments();
    failures += test_independent_session_handles();

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

    failures += expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB stats/status/thread/undo db"
    );
    for (size_t index = 0U; index < sizeof(innodb_stats_status_thread_undo_variables) /
                                        sizeof(innodb_stats_status_thread_undo_variables[0]);
         ++index) {
        const struct innodb_stats_status_thread_undo_variable *variable =
            &innodb_stats_status_thread_undo_variables[index];

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

        snprintf(
            sql,
            sizeof(sql),
            "SELECT @@SESSION.%s, @@LOCAL.%s",
            variable->name,
            variable->name
        );
        if (innodb_stats_status_thread_undo_variable_is_session_scoped(variable)) {
            failures += expect_values(
                database,
                sql,
                (const char *[]){variable->scalar_value, variable->scalar_value},
                2U,
                variable->name
            );
        } else {
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

    failures += expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB stats/status/thread/undo SET db"
    );
    for (size_t index = 0U; index < sizeof(innodb_stats_status_thread_undo_variables) /
                                        sizeof(innodb_stats_status_thread_undo_variables[0]);
         ++index) {
        const struct innodb_stats_status_thread_undo_variable *variable =
            &innodb_stats_status_thread_undo_variables[index];

        if (innodb_stats_status_thread_undo_variable_is_read_only(variable)) {
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
        if (innodb_stats_status_thread_undo_variable_is_session_scoped(variable)) {
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
                variable->alternate_assignment
            );
            failures += execute_error(database, sql, unsupported_fixed_noop);
            continue;
        }

        snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
        failures += execute_error(database, sql, global_only_set);
        snprintf(sql, sizeof(sql), "SET SESSION %s = DEFAULT", variable->name);
        failures += execute_error(database, sql, global_only_set);
        snprintf(sql, sizeof(sql), "SET LOCAL %s = DEFAULT", variable->name);
        failures += execute_error(database, sql, global_only_set);

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
            variable->alternate_assignment
        );
        failures += execute_error(database, sql, unsupported_fixed_noop);
    }

    mylite_close(database);
    return failures;
}

static int test_session_boolean_assignments(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB session boolean db");
    failures += execute_statement_ok(database, "SET innodb_strict_mode = OFF");
    failures += expect_values(
        database,
        "SELECT @@innodb_strict_mode, @@SESSION.innodb_strict_mode, @@GLOBAL.innodb_strict_mode",
        (const char *[]){"0", "0", "1"},
        3U,
        "session innodb_strict_mode off"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'innodb_strict_mode'",
        (const char *[]){"innodb_strict_mode", "OFF"},
        2U,
        "show session innodb_strict_mode off"
    );
    failures += expect_values(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'innodb_strict_mode'",
        (const char *[]){"innodb_strict_mode", "ON"},
        2U,
        "show global innodb_strict_mode fixed"
    );
    failures += execute_statement_ok(database, "SET LOCAL innodb_table_locks = OFF");
    failures += expect_values(
        database,
        "SELECT @@innodb_table_locks, @@LOCAL.innodb_table_locks, @@GLOBAL.innodb_table_locks",
        (const char *[]){"0", "0", "1"},
        3U,
        "session innodb_table_locks off"
    );
    failures += execute_statement_ok(database, "SET SESSION innodb_strict_mode = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION innodb_table_locks = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@innodb_strict_mode, @@innodb_table_locks",
        (const char *[]){"1", "1"},
        2U,
        "session boolean defaults restored"
    );

    mylite_close(database);
    return failures;
}

static int test_tmpdir_assignments(void) {
    struct expected_sql_error unsupported_fixed_noop = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error cant_set_bogus = {
        .code = mysql_error_cant_set_variable,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'bogus'",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open innodb_tmpdir db");
    failures += execute_statement_ok(database, "SET innodb_tmpdir = '/tmp'");
    failures += expect_values(
        database,
        "SELECT @@innodb_tmpdir, @@SESSION.innodb_tmpdir, @@GLOBAL.innodb_tmpdir",
        (const char *[]){"/tmp", "/tmp", NULL},
        3U,
        "session innodb_tmpdir path"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'innodb_tmpdir'",
        (const char *[]){"innodb_tmpdir", "/tmp"},
        2U,
        "show session innodb_tmpdir path"
    );
    failures += expect_values(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'innodb_tmpdir'",
        (const char *[]){"innodb_tmpdir", ""},
        2U,
        "show global innodb_tmpdir fixed"
    );
    failures += execute_statement_ok(database, "SET SESSION innodb_tmpdir = NULL");
    failures += expect_values(
        database,
        "SELECT @@innodb_tmpdir, @@SESSION.innodb_tmpdir, @@GLOBAL.innodb_tmpdir",
        (const char *[]){NULL, NULL, NULL},
        3U,
        "session innodb_tmpdir null"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'innodb_tmpdir'",
        (const char *[]){"innodb_tmpdir", ""},
        2U,
        "show session innodb_tmpdir null"
    );
    failures += execute_statement_ok(database, "SET GLOBAL innodb_tmpdir = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL innodb_tmpdir = NULL");
    failures +=
        execute_error(database, "SET GLOBAL innodb_tmpdir = '/tmp'", unsupported_fixed_noop);
    failures += execute_error(database, "SET innodb_tmpdir = 'bogus'", cant_set_bogus);

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
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures += expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB stats/status/thread/undo user-var db"
    );
    for (size_t index = 0U; index < sizeof(innodb_stats_status_thread_undo_variables) /
                                        sizeof(innodb_stats_status_thread_undo_variables[0]);
         ++index) {
        const struct innodb_stats_status_thread_undo_variable *variable =
            &innodb_stats_status_thread_undo_variables[index];

        if (innodb_stats_status_thread_undo_variable_is_read_only(variable)) {
            failures += execute_statement_ok(database, "SET @innodb_status_thread_undo_value = 0");
            snprintf(
                sql,
                sizeof(sql),
                "SET GLOBAL %s = @innodb_status_thread_undo_value",
                variable->name
            );
            failures += execute_error(database, sql, read_only_set);
            continue;
        }

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_status_thread_undo_value = %s",
            variable->exact_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(
            sql,
            sizeof(sql),
            "SET GLOBAL %s = @innodb_status_thread_undo_value",
            variable->name
        );
        failures += execute_statement_ok(database, sql);

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_status_thread_undo_value = %s",
            variable->alternate_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(
            sql,
            sizeof(sql),
            "SET GLOBAL %s = @innodb_status_thread_undo_value",
            variable->name
        );
        failures += execute_error(database, sql, unsupported_fixed_noop);

        if (variable->scope ==
                innodb_stats_status_thread_undo_variable_scope_dynamic_session_boolean ||
            variable->scope ==
                innodb_stats_status_thread_undo_variable_scope_dynamic_session_null_text) {
            snprintf(sql, sizeof(sql), "SET %s = @innodb_status_thread_undo_value", variable->name);
            failures += execute_statement_ok(database, sql);
        }
    }

    failures += execute_statement_ok(database, "SET @innodb_status_thread_undo_value = NULL");
    failures +=
        execute_statement_ok(database, "SET innodb_tmpdir = @innodb_status_thread_undo_value");
    failures += expect_values(
        database,
        "SELECT @@innodb_tmpdir",
        (const char *[]){NULL},
        1U,
        "innodb_tmpdir user-var null"
    );

    mylite_close(database);
    return failures;
}

static int test_independent_session_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first InnoDB session db");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second InnoDB session db");
    failures += execute_statement_ok(first, "SET innodb_strict_mode = OFF");
    failures += execute_statement_ok(first, "SET innodb_table_locks = OFF");
    failures += execute_statement_ok(first, "SET innodb_tmpdir = '/tmp'");
    failures += expect_values(
        first,
        "SELECT @@innodb_strict_mode, @@innodb_table_locks, @@innodb_tmpdir",
        (const char *[]){"0", "0", "/tmp"},
        3U,
        "first handle session values"
    );
    failures += expect_values(
        second,
        "SELECT @@innodb_strict_mode, @@innodb_table_locks, @@innodb_tmpdir",
        (const char *[]){"1", "1", NULL},
        3U,
        "second handle default values"
    );

    mylite_close(first);
    mylite_close(second);
    return failures;
}

static bool innodb_stats_status_thread_undo_variable_is_read_only(
    const struct innodb_stats_status_thread_undo_variable *variable
) {
    return variable != NULL &&
           variable->scope == innodb_stats_status_thread_undo_variable_scope_read_only;
}

static bool innodb_stats_status_thread_undo_variable_is_session_scoped(
    const struct innodb_stats_status_thread_undo_variable *variable
) {
    return variable != NULL &&
           (variable->scope ==
                innodb_stats_status_thread_undo_variable_scope_dynamic_session_boolean ||
            variable->scope ==
                innodb_stats_status_thread_undo_variable_scope_dynamic_session_null_text);
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
            "%s: expected result for [%s], got %d %s\n",
            context,
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_size(mylite_result_column_count(result), expected_count, context);
    if (failures == 0) {
        for (size_t column = 0U; column < expected_count; ++column) {
            failures += expect_text(
                mylite_result_value_text(result, 0U, column),
                expected[column],
                context
            );
        }
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

    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got OK\n", sql);
        mylite_result_free(result);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "NULL" : expected,
        actual == NULL ? "NULL" : actual
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
        actual == NULL ? "NULL" : actual,
        needle == NULL ? "NULL" : needle
    );
    return 1;
}
