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

enum innodb_page_read_purge_variable_scope {
    innodb_page_read_purge_variable_scope_dynamic_global = 0,
    innodb_page_read_purge_variable_scope_dynamic_boolean = 1,
    innodb_page_read_purge_variable_scope_read_only = 2,
    innodb_page_read_purge_variable_scope_dynamic_session = 3,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct innodb_page_read_purge_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_assignment;
    const char *unsupported_assignment;
    enum innodb_page_read_purge_variable_scope scope;
};

static const struct innodb_page_read_purge_variable innodb_page_read_purge_variables[] = {
    {"innodb_old_blocks_pct",
     "37",
     "37",
     "37",
     "38",
     innodb_page_read_purge_variable_scope_dynamic_global},
    {"innodb_old_blocks_time",
     "1000",
     "1000",
     "1000",
     "1001",
     innodb_page_read_purge_variable_scope_dynamic_global},
    {"innodb_online_alter_log_max_size",
     "134217728",
     "134217728",
     "134217728",
     "134217729",
     innodb_page_read_purge_variable_scope_dynamic_global},
    {"innodb_open_files",
     "4000",
     "4000",
     NULL,
     NULL,
     innodb_page_read_purge_variable_scope_read_only},
    {"innodb_optimize_fulltext_only",
     "0",
     "OFF",
     "OFF",
     "ON",
     innodb_page_read_purge_variable_scope_dynamic_boolean},
    {"innodb_page_cleaners", "1", "1", NULL, NULL, innodb_page_read_purge_variable_scope_read_only},
    {"innodb_page_size",
     "16384",
     "16384",
     NULL,
     NULL,
     innodb_page_read_purge_variable_scope_read_only},
    {"innodb_parallel_read_threads",
     "4",
     "4",
     "4",
     "5",
     innodb_page_read_purge_variable_scope_dynamic_session},
    {"innodb_print_all_deadlocks",
     "0",
     "OFF",
     "OFF",
     "ON",
     innodb_page_read_purge_variable_scope_dynamic_boolean},
    {"innodb_print_ddl_logs",
     "0",
     "OFF",
     "OFF",
     "ON",
     innodb_page_read_purge_variable_scope_dynamic_boolean},
    {"innodb_purge_batch_size",
     "300",
     "300",
     "300",
     "301",
     innodb_page_read_purge_variable_scope_dynamic_global},
    {"innodb_purge_rseg_truncate_frequency",
     "128",
     "128",
     "128",
     "127",
     innodb_page_read_purge_variable_scope_dynamic_global},
    {"innodb_purge_threads", "4", "4", NULL, NULL, innodb_page_read_purge_variable_scope_read_only},
    {"innodb_random_read_ahead",
     "0",
     "OFF",
     "OFF",
     "ON",
     innodb_page_read_purge_variable_scope_dynamic_boolean},
    {"innodb_read_ahead_threshold",
     "56",
     "56",
     "56",
     "57",
     innodb_page_read_purge_variable_scope_dynamic_global},
    {"innodb_read_io_threads", "9", "9", NULL, NULL, innodb_page_read_purge_variable_scope_read_only
    },
};

static int test_values_show_and_scope(void);
static int test_set_diagnostics(void);
static int test_parallel_read_threads_session_mutation(void);
static int test_user_variable_assignments(void);
static bool innodb_page_read_purge_variable_allows_session_scope(
    const struct innodb_page_read_purge_variable *variable
);
static bool innodb_page_read_purge_variable_is_boolean(
    const struct innodb_page_read_purge_variable *variable
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
    failures += test_parallel_read_threads_session_mutation();
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
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB page/read/purge db");
    for (size_t index = 0U; index < sizeof(innodb_page_read_purge_variables) /
                                        sizeof(innodb_page_read_purge_variables[0]);
         ++index) {
        const struct innodb_page_read_purge_variable *variable =
            &innodb_page_read_purge_variables[index];

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

        if (innodb_page_read_purge_variable_allows_session_scope(variable)) {
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

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB page/read/purge SET db");
    for (size_t index = 0U; index < sizeof(innodb_page_read_purge_variables) /
                                        sizeof(innodb_page_read_purge_variables[0]);
         ++index) {
        const struct innodb_page_read_purge_variable *variable =
            &innodb_page_read_purge_variables[index];

        if (variable->scope == innodb_page_read_purge_variable_scope_read_only) {
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

        if (innodb_page_read_purge_variable_allows_session_scope(variable)) {
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

static int test_parallel_read_threads_session_mutation(void) {
    struct expected_sql_error incorrect_argument_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB parallel read threads db"
    );
    failures += execute_statement_ok(database, "SET innodb_parallel_read_threads = 5");
    failures += expect_values(
        database,
        "SELECT @@innodb_parallel_read_threads, @@SESSION.innodb_parallel_read_threads, "
        "@@GLOBAL.innodb_parallel_read_threads",
        (const char *[]){"5", "5", "4"},
        3U,
        "innodb_parallel_read_threads session value"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'innodb_parallel_read_threads'",
        (const char *[]){"innodb_parallel_read_threads", "5"},
        2U,
        "innodb_parallel_read_threads SHOW session"
    );
    failures += expect_values(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'innodb_parallel_read_threads'",
        (const char *[]){"innodb_parallel_read_threads", "4"},
        2U,
        "innodb_parallel_read_threads SHOW global"
    );
    failures += execute_statement_ok(database, "SET LOCAL innodb_parallel_read_threads = 6");
    failures += expect_values(
        database,
        "SELECT @@innodb_parallel_read_threads, @@LOCAL.innodb_parallel_read_threads",
        (const char *[]){"6", "6"},
        2U,
        "innodb_parallel_read_threads local value"
    );
    failures +=
        execute_statement_ok(database, "SET SESSION innodb_parallel_read_threads = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@innodb_parallel_read_threads",
        (const char *[]){"4"},
        1U,
        "innodb_parallel_read_threads default"
    );

    failures += execute_statement_ok(database, "SET innodb_parallel_read_threads = 0");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]
        ){"Warning", "1292", "Truncated incorrect innodb_parallel_read_threads value: '0'"},
        3U,
        "innodb_parallel_read_threads low warning"
    );
    failures += expect_values(
        database,
        "SELECT @@innodb_parallel_read_threads, @@warning_count",
        (const char *[]){"1", "1"},
        2U,
        "innodb_parallel_read_threads low clamp"
    );
    failures += execute_statement_ok(database, "SET innodb_parallel_read_threads = 257");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]
        ){"Warning", "1292", "Truncated incorrect innodb_parallel_read_threads value: '257'"},
        3U,
        "innodb_parallel_read_threads high warning"
    );
    failures += expect_values(
        database,
        "SELECT @@innodb_parallel_read_threads, @@warning_count",
        (const char *[]){"256", "1"},
        2U,
        "innodb_parallel_read_threads high clamp"
    );
    failures += execute_error(
        database,
        "SET innodb_parallel_read_threads = 'bad'",
        incorrect_argument_type
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

    failures += expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB page/read/purge user-var db"
    );
    for (size_t index = 0U; index < sizeof(innodb_page_read_purge_variables) /
                                        sizeof(innodb_page_read_purge_variables[0]);
         ++index) {
        const struct innodb_page_read_purge_variable *variable =
            &innodb_page_read_purge_variables[index];

        if (variable->scope == innodb_page_read_purge_variable_scope_read_only) {
            failures += execute_statement_ok(database, "SET @innodb_page_read_purge_value = 0");
            snprintf(
                sql,
                sizeof(sql),
                "SET GLOBAL %s = @innodb_page_read_purge_value",
                variable->name
            );
            failures += execute_error(database, sql, read_only_set);
            continue;
        }

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_page_read_purge_value = %s",
            variable->exact_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_page_read_purge_value", variable->name);
        failures += execute_statement_ok(database, sql);

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_page_read_purge_value = %s",
            variable->unsupported_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_page_read_purge_value", variable->name);
        failures += execute_error(database, sql, unsupported_fixed_noop);

        if (innodb_page_read_purge_variable_allows_session_scope(variable)) {
            failures += execute_statement_ok(database, "SET @innodb_page_read_purge_value = 5");
            failures += execute_statement_ok(
                database,
                "SET innodb_parallel_read_threads = @innodb_page_read_purge_value"
            );
            failures += expect_values(
                database,
                "SELECT @@innodb_parallel_read_threads",
                (const char *[]){"5"},
                1U,
                "innodb_parallel_read_threads user-var session"
            );
        }

        failures += execute_statement_ok(database, "SET @innodb_page_read_purge_value = 'bad'");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_page_read_purge_value", variable->name);
        failures += execute_error(
            database,
            sql,
            innodb_page_read_purge_variable_is_boolean(variable) ? cant_set_value
                                                                 : incorrect_argument_type
        );
    }

    mylite_close(database);
    return failures;
}

static bool innodb_page_read_purge_variable_allows_session_scope(
    const struct innodb_page_read_purge_variable *variable
) {
    return variable != NULL &&
           variable->scope == innodb_page_read_purge_variable_scope_dynamic_session;
}

static bool innodb_page_read_purge_variable_is_boolean(
    const struct innodb_page_read_purge_variable *variable
) {
    return variable != NULL &&
           variable->scope == innodb_page_read_purge_variable_scope_dynamic_boolean;
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
