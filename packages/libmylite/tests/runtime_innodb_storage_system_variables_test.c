#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
    sql_buffer_capacity = 256,
};

enum innodb_variable_scope {
    innodb_variable_scope_dynamic_global = 0,
    innodb_variable_scope_dynamic_session = 1,
    innodb_variable_scope_read_only = 2,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct innodb_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_assignment;
    const char *unsupported_assignment;
    enum innodb_variable_scope scope;
    bool text_value;
    bool boolean_value;
};

static const struct innodb_variable innodb_variables[] = {
    {"innodb_checksum_algorithm",
     "crc32",
     "crc32",
     "'crc32'",
     "'strict_crc32'",
     innodb_variable_scope_dynamic_global,
     true,
     false},
    {"innodb_cmp_per_index_enabled",
     "0",
     "OFF",
     "OFF",
     "ON",
     innodb_variable_scope_dynamic_global,
     false,
     true},
    {"innodb_commit_concurrency",
     "0",
     "0",
     "0",
     "1",
     innodb_variable_scope_dynamic_global,
     false,
     false},
    {"innodb_compression_failure_threshold_pct",
     "5",
     "5",
     "5",
     "6",
     innodb_variable_scope_dynamic_global,
     false,
     false},
    {"innodb_compression_level",
     "6",
     "6",
     "6",
     "7",
     innodb_variable_scope_dynamic_global,
     false,
     false},
    {"innodb_compression_pad_pct_max",
     "50",
     "50",
     "50",
     "51",
     innodb_variable_scope_dynamic_global,
     false,
     false},
    {"innodb_concurrency_tickets",
     "5000",
     "5000",
     "5000",
     "5001",
     innodb_variable_scope_dynamic_global,
     false,
     false},
    {"innodb_data_file_path",
     "ibdata1:12M:autoextend",
     "ibdata1:12M:autoextend",
     NULL,
     NULL,
     innodb_variable_scope_read_only,
     true,
     false},
    {"innodb_data_home_dir", NULL, "", NULL, NULL, innodb_variable_scope_read_only, true, false},
    {"innodb_ddl_buffer_size",
     "1048576",
     "1048576",
     "1048576",
     "1048577",
     innodb_variable_scope_dynamic_session,
     false,
     false},
    {"innodb_ddl_threads", "4", "4", "4", "5", innodb_variable_scope_dynamic_session, false, false},
    {"innodb_deadlock_detect",
     "1",
     "ON",
     "ON",
     "OFF",
     innodb_variable_scope_dynamic_global,
     false,
     true},
    {"innodb_dedicated_server", "0", "OFF", NULL, NULL, innodb_variable_scope_read_only, false, true
    },
    {"innodb_default_row_format",
     "dynamic",
     "dynamic",
     "'dynamic'",
     "'compact'",
     innodb_variable_scope_dynamic_global,
     true,
     false},
    {"innodb_directories", NULL, "", NULL, NULL, innodb_variable_scope_read_only, true, false},
    {"innodb_disable_sort_file_cache",
     "0",
     "OFF",
     "OFF",
     "ON",
     innodb_variable_scope_dynamic_global,
     false,
     true},
    {"innodb_doublewrite",
     "ON",
     "ON",
     "'ON'",
     "'OFF'",
     innodb_variable_scope_dynamic_global,
     true,
     false},
    {"innodb_doublewrite_batch_size",
     "0",
     "0",
     NULL,
     NULL,
     innodb_variable_scope_read_only,
     false,
     false},
    {"innodb_doublewrite_dir", NULL, "", NULL, NULL, innodb_variable_scope_read_only, true, false},
    {"innodb_doublewrite_files", "2", "2", NULL, NULL, innodb_variable_scope_read_only, false, false
    },
    {"innodb_doublewrite_pages",
     "128",
     "128",
     NULL,
     NULL,
     innodb_variable_scope_read_only,
     false,
     false},
    {"innodb_extend_and_initialize",
     "1",
     "ON",
     "ON",
     "OFF",
     innodb_variable_scope_dynamic_global,
     false,
     true},
    {"innodb_fast_shutdown", "1", "1", "1", "0", innodb_variable_scope_dynamic_global, false, false
    },
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
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB storage db");
    for (size_t index = 0U; index < sizeof(innodb_variables) / sizeof(innodb_variables[0]);
         ++index) {
        const struct innodb_variable *variable = &innodb_variables[index];

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
        if (variable->scope == innodb_variable_scope_dynamic_session) {
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
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB SET db");
    for (size_t index = 0U; index < sizeof(innodb_variables) / sizeof(innodb_variables[0]);
         ++index) {
        const struct innodb_variable *variable = &innodb_variables[index];

        if (variable->scope == innodb_variable_scope_read_only) {
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

        if (variable->scope == innodb_variable_scope_dynamic_global) {
            snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, global_only_set);
            snprintf(sql, sizeof(sql), "SET SESSION %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, global_only_set);
            snprintf(sql, sizeof(sql), "SET LOCAL %s = DEFAULT", variable->name);
            failures += execute_error(database, sql, global_only_set);
        } else {
            snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SET SESSION %s = DEFAULT", variable->name);
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SET LOCAL %s = DEFAULT", variable->name);
            failures += execute_statement_ok(database, sql);
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

        if (variable->scope == innodb_variable_scope_dynamic_session) {
            snprintf(sql, sizeof(sql), "SET %s = %s", variable->name, variable->exact_assignment);
            failures += execute_statement_ok(database, sql);
            snprintf(
                sql,
                sizeof(sql),
                "SET SESSION %s = %s",
                variable->name,
                variable->exact_assignment
            );
            failures += execute_statement_ok(database, sql);
            snprintf(
                sql,
                sizeof(sql),
                "SET LOCAL %s = %s",
                variable->name,
                variable->exact_assignment
            );
            failures += execute_statement_ok(database, sql);
            snprintf(
                sql,
                sizeof(sql),
                "SET %s = %s",
                variable->name,
                variable->unsupported_assignment
            );
            failures += execute_error(database, sql, unsupported_fixed_noop);
        }
    }

    mylite_close(database);
    return failures;
}

static int test_user_variable_assignments(void) {
    struct expected_sql_error unsupported_fixed_noop = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error incorrect_argument_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB user-var db");
    for (size_t index = 0U; index < sizeof(innodb_variables) / sizeof(innodb_variables[0]);
         ++index) {
        const struct innodb_variable *variable = &innodb_variables[index];

        if (variable->scope == innodb_variable_scope_read_only) {
            continue;
        }

        snprintf(sql, sizeof(sql), "SET @innodb_storage_value = %s", variable->exact_assignment);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_storage_value", variable->name);
        failures += execute_statement_ok(database, sql);
        if (variable->scope == innodb_variable_scope_dynamic_session) {
            snprintf(sql, sizeof(sql), "SET %s = @innodb_storage_value", variable->name);
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SET SESSION %s = @innodb_storage_value", variable->name);
            failures += execute_statement_ok(database, sql);
            snprintf(sql, sizeof(sql), "SET LOCAL %s = @innodb_storage_value", variable->name);
            failures += execute_statement_ok(database, sql);
        }

        snprintf(
            sql,
            sizeof(sql),
            "SET @innodb_storage_value = %s",
            variable->unsupported_assignment
        );
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_storage_value", variable->name);
        failures += execute_error(database, sql, unsupported_fixed_noop);
        if (variable->scope == innodb_variable_scope_dynamic_session) {
            snprintf(sql, sizeof(sql), "SET %s = @innodb_storage_value", variable->name);
            failures += execute_error(database, sql, unsupported_fixed_noop);
        }

        if (!variable->text_value && !variable->boolean_value) {
            failures += execute_statement_ok(database, "SET @innodb_storage_value = 'not_numeric'");
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_storage_value", variable->name);
            failures += execute_error(database, sql, incorrect_argument_type);
        }
    }

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
