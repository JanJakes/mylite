#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_cant_set_variable = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
    sql_buffer_capacity = 512,
};

enum performance_schema_variable_scope {
    performance_schema_variable_scope_read_only = 0,
    performance_schema_variable_scope_dynamic_numeric = 1,
    performance_schema_variable_scope_dynamic_boolean = 2,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct performance_schema_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_assignment;
    const char *alternate_assignment;
    enum performance_schema_variable_scope scope;
};

static const struct performance_schema_variable performance_schema_variables[] = {
    {"performance_schema", "1", "ON", NULL, NULL, performance_schema_variable_scope_read_only},
    {"performance_schema_accounts_size",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_digests_size",
     "10000",
     "10000",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_error_size",
     "5556",
     "5556",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_stages_history_long_size",
     "10000",
     "10000",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_stages_history_size",
     "10",
     "10",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_statements_history_long_size",
     "10000",
     "10000",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_statements_history_size",
     "10",
     "10",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_transactions_history_long_size",
     "10000",
     "10000",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_transactions_history_size",
     "10",
     "10",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_waits_history_long_size",
     "10000",
     "10000",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_events_waits_history_size",
     "10",
     "10",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_hosts_size",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_cond_classes",
     "150",
     "150",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_cond_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_digest_length",
     "1024",
     "1024",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_digest_sample_age",
     "60",
     "60",
     "60",
     "61",
     performance_schema_variable_scope_dynamic_numeric},
    {"performance_schema_max_file_classes",
     "80",
     "80",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_file_handles",
     "32768",
     "32768",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_file_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_index_stat",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_memory_classes",
     "470",
     "470",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_metadata_locks",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_meter_classes",
     "30",
     "30",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_metric_classes",
     "600",
     "600",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_mutex_classes",
     "350",
     "350",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_mutex_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_prepared_statements_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_program_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_rwlock_classes",
     "100",
     "100",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_rwlock_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_socket_classes",
     "10",
     "10",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_socket_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_sql_text_length",
     "1024",
     "1024",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_stage_classes",
     "175",
     "175",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_statement_classes",
     "220",
     "220",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_statement_stack",
     "10",
     "10",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_table_handles",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_table_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_table_lock_stat",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_thread_classes",
     "100",
     "100",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_max_thread_instances",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_session_connect_attrs_size",
     "512",
     "512",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_setup_actors_size",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_setup_objects_size",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
    {"performance_schema_show_processlist",
     "0",
     "OFF",
     "OFF",
     "ON",
     performance_schema_variable_scope_dynamic_boolean},
    {"performance_schema_users_size",
     "-1",
     "-1",
     NULL,
     NULL,
     performance_schema_variable_scope_read_only},
};

static int test_values_show_and_scope(void);
static int test_set_diagnostics(void);
static int test_user_variable_assignments(void);
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
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open PFS variable db");
    for (size_t index = 0U;
         index < sizeof(performance_schema_variables) / sizeof(performance_schema_variables[0]);
         ++index) {
        const struct performance_schema_variable *variable = &performance_schema_variables[index];

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
        failures += execute_error(database, sql, global_only_read);
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

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open PFS SET db");
    for (size_t index = 0U;
         index < sizeof(performance_schema_variables) / sizeof(performance_schema_variables[0]);
         ++index) {
        const struct performance_schema_variable *variable = &performance_schema_variables[index];

        if (variable->scope == performance_schema_variable_scope_read_only) {
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
    struct expected_sql_error cant_set_null = {
        .code = mysql_error_cant_set_variable,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'NULL'",
    };
    struct expected_sql_error incorrect_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open PFS user-var db");
    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = 0");
    failures += execute_error(
        database,
        "SET GLOBAL performance_schema_accounts_size = @pfs_dynamic_value",
        read_only_set
    );

    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = 60");
    failures += execute_statement_ok(
        database,
        "SET GLOBAL performance_schema_max_digest_sample_age = @pfs_dynamic_value"
    );
    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = 61");
    failures += execute_error(
        database,
        "SET GLOBAL performance_schema_max_digest_sample_age = @pfs_dynamic_value",
        unsupported_fixed_noop
    );
    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = '60'");
    failures += execute_error(
        database,
        "SET GLOBAL performance_schema_max_digest_sample_age = @pfs_dynamic_value",
        incorrect_type
    );
    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = NULL");
    failures += execute_error(
        database,
        "SET GLOBAL performance_schema_max_digest_sample_age = @pfs_dynamic_value",
        incorrect_type
    );

    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = 0");
    failures += execute_statement_ok(
        database,
        "SET GLOBAL performance_schema_show_processlist = @pfs_dynamic_value"
    );
    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = 1");
    failures += execute_error(
        database,
        "SET GLOBAL performance_schema_show_processlist = @pfs_dynamic_value",
        unsupported_fixed_noop
    );
    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = 'OFF'");
    failures += execute_statement_ok(
        database,
        "SET GLOBAL performance_schema_show_processlist = @pfs_dynamic_value"
    );
    failures += execute_statement_ok(database, "SET @pfs_dynamic_value = NULL");
    failures += execute_error(
        database,
        "SET GLOBAL performance_schema_show_processlist = @pfs_dynamic_value",
        cant_set_null
    );

    snprintf(sql, sizeof(sql), "SET performance_schema_show_processlist = @pfs_dynamic_value");
    failures += execute_error(
        database,
        sql,
        (struct expected_sql_error){
            .code = mysql_error_global_variable_scope,
            .sqlstate = "HY000",
            .message_part = "should be set with SET GLOBAL",
        }
    );

    mylite_close(database);
    return failures;
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

    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), expected_count, context);
    if (failures == 0) {
        for (size_t column = 0U; column < expected_count; ++column) {
            failures += mylite_test_expect_text(
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

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}
