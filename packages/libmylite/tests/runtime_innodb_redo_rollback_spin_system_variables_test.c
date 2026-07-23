#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
    sql_buffer_capacity = 512,
};

enum innodb_redo_rollback_spin_variable_scope {
    innodb_redo_rollback_spin_variable_scope_dynamic_null = 0,
    innodb_redo_rollback_spin_variable_scope_dynamic_numeric = 1,
    innodb_redo_rollback_spin_variable_scope_dynamic_boolean = 2,
    innodb_redo_rollback_spin_variable_scope_dynamic_decimal = 3,
    innodb_redo_rollback_spin_variable_scope_read_only = 4,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct innodb_redo_rollback_spin_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_assignment;
    const char *decimal_assignment;
    const char *unsupported_assignment;
    enum innodb_redo_rollback_spin_variable_scope scope;
};

static const struct innodb_redo_rollback_spin_variable innodb_redo_rollback_spin_variables[] = {
    {"innodb_redo_log_archive_dirs",
     NULL,
     "",
     "NULL",
     NULL,
     "'bogus'",
     innodb_redo_rollback_spin_variable_scope_dynamic_null},
    {"innodb_redo_log_capacity",
     "104857600",
     "104857600",
     "104857600",
     NULL,
     "104857601",
     innodb_redo_rollback_spin_variable_scope_dynamic_numeric},
    {"innodb_redo_log_encrypt",
     "0",
     "OFF",
     "OFF",
     NULL,
     "ON",
     innodb_redo_rollback_spin_variable_scope_dynamic_boolean},
    {"innodb_replication_delay",
     "0",
     "0",
     "0",
     NULL,
     "1",
     innodb_redo_rollback_spin_variable_scope_dynamic_numeric},
    {"innodb_rollback_on_timeout",
     "0",
     "OFF",
     NULL,
     NULL,
     NULL,
     innodb_redo_rollback_spin_variable_scope_read_only},
    {"innodb_rollback_segments",
     "128",
     "128",
     "128",
     NULL,
     "127",
     innodb_redo_rollback_spin_variable_scope_dynamic_numeric},
    {"innodb_segment_reserve_factor",
     "12.500000",
     "12.500000",
     "12.5",
     "12.500000",
     "13.5",
     innodb_redo_rollback_spin_variable_scope_dynamic_decimal},
    {"innodb_sort_buffer_size",
     "1048576",
     "1048576",
     NULL,
     NULL,
     NULL,
     innodb_redo_rollback_spin_variable_scope_read_only},
    {"innodb_spin_wait_delay",
     "6",
     "6",
     "6",
     NULL,
     "7",
     innodb_redo_rollback_spin_variable_scope_dynamic_numeric},
    {"innodb_spin_wait_pause_multiplier",
     "50",
     "50",
     "50",
     NULL,
     "51",
     innodb_redo_rollback_spin_variable_scope_dynamic_numeric},
};

static int test_values_show_and_scope(void);
static int test_set_diagnostics(void);
static int test_user_variable_assignments(void);
static bool innodb_redo_rollback_spin_variable_is_read_only(
    const struct innodb_redo_rollback_spin_variable *variable
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

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB redo/rollback/spin db"
    );
    for (size_t index = 0U; index < sizeof(innodb_redo_rollback_spin_variables) /
                                        sizeof(innodb_redo_rollback_spin_variables[0]);
         ++index) {
        const struct innodb_redo_rollback_spin_variable *variable =
            &innodb_redo_rollback_spin_variables[index];

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

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
        failures += execute_error(database, sql, global_only_read);
        snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", variable->name);
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

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB redo/rollback/spin SET db"
    );
    for (size_t index = 0U; index < sizeof(innodb_redo_rollback_spin_variables) /
                                        sizeof(innodb_redo_rollback_spin_variables[0]);
         ++index) {
        const struct innodb_redo_rollback_spin_variable *variable =
            &innodb_redo_rollback_spin_variables[index];

        if (innodb_redo_rollback_spin_variable_is_read_only(variable)) {
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
        if (variable->decimal_assignment != NULL) {
            snprintf(
                sql,
                sizeof(sql),
                "SET GLOBAL %s = %s",
                variable->name,
                variable->decimal_assignment
            );
            failures += execute_statement_ok(database, sql);
        }
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

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB redo/rollback/spin user-var db"
    );
    for (size_t index = 0U; index < sizeof(innodb_redo_rollback_spin_variables) /
                                        sizeof(innodb_redo_rollback_spin_variables[0]);
         ++index) {
        const struct innodb_redo_rollback_spin_variable *variable =
            &innodb_redo_rollback_spin_variables[index];

        if (innodb_redo_rollback_spin_variable_is_read_only(variable)) {
            failures += execute_statement_ok(database, "SET @innodb_redo_rollback_spin_value = 0");
            snprintf(
                sql,
                sizeof(sql),
                "SET GLOBAL %s = @innodb_redo_rollback_spin_value",
                variable->name
            );
            failures += execute_error(database, sql, read_only_set);
            continue;
        }

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_redo_rollback_spin_value = %s",
            variable->exact_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(
            sql,
            sizeof(sql),
            "SET GLOBAL %s = @innodb_redo_rollback_spin_value",
            variable->name
        );
        failures += execute_statement_ok(database, sql);

        if (variable->decimal_assignment != NULL) {
            snprintf(
                sql,
                sizeof(sql),
                "SET @innodb_redo_rollback_spin_value = %s",
                variable->decimal_assignment
            );
            failures += execute_statement_ok(database, sql);
            snprintf(
                sql,
                sizeof(sql),
                "SET GLOBAL %s = @innodb_redo_rollback_spin_value",
                variable->name
            );
            failures += execute_statement_ok(database, sql);
        }

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_redo_rollback_spin_value = %s",
            variable->unsupported_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(
            sql,
            sizeof(sql),
            "SET GLOBAL %s = @innodb_redo_rollback_spin_value",
            variable->name
        );
        failures += execute_error(database, sql, unsupported_fixed_noop);
    }

    mylite_close(database);
    return failures;
}

static bool innodb_redo_rollback_spin_variable_is_read_only(
    const struct innodb_redo_rollback_spin_variable *variable
) {
    return variable != NULL &&
           variable->scope == innodb_redo_rollback_spin_variable_scope_read_only;
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
