#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_wrong_value_for_variable = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
    sql_buffer_capacity = 512,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

static const char *const innodb_monitor_variables[] = {
    "innodb_monitor_disable",
    "innodb_monitor_enable",
    "innodb_monitor_reset",
    "innodb_monitor_reset_all",
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
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

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

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB monitor db");
    for (size_t index = 0U;
         index < sizeof(innodb_monitor_variables) / sizeof(innodb_monitor_variables[0]);
         ++index) {
        const char *variable = innodb_monitor_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s, @@GLOBAL.%s", variable, variable);
        failures += expect_values(database, sql, (const char *[]){NULL, NULL}, 2U, variable);

        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable);
        failures += expect_values(database, sql, (const char *[]){variable, ""}, 2U, variable);
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable);
        failures += expect_values(database, sql, (const char *[]){variable, ""}, 2U, variable);
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable);
        failures += expect_values(database, sql, (const char *[]){variable, ""}, 2U, variable);

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable);
        failures += execute_error(database, sql, global_only_read);
        snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", variable);
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
    struct expected_sql_error null_value = {
        .code = mysql_error_wrong_value_for_variable,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'NULL'",
    };
    struct expected_sql_error integer_value = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    struct expected_sql_error bogus_value = {
        .code = mysql_error_wrong_value_for_variable,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'bogus'",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB monitor SET db");
    for (size_t index = 0U;
         index < sizeof(innodb_monitor_variables) / sizeof(innodb_monitor_variables[0]);
         ++index) {
        const char *variable = innodb_monitor_variables[index];

        snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable);
        failures += execute_error(database, sql, global_only_set);
        snprintf(sql, sizeof(sql), "SET SESSION %s = DEFAULT", variable);
        failures += execute_error(database, sql, global_only_set);
        snprintf(sql, sizeof(sql), "SET LOCAL %s = DEFAULT", variable);
        failures += execute_error(database, sql, global_only_set);

        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 'all'", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 'latch'", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 'module_buffer'", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 'buffer%%'", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = all", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = latch", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = module_buffer", variable);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = NULL", variable);
        failures += execute_error(database, sql, null_value);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 1", variable);
        failures += execute_error(database, sql, integer_value);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 'bogus'", variable);
        failures += execute_error(database, sql, bogus_value);
    }

    mylite_close(database);
    return failures;
}

static int test_user_variable_assignments(void) {
    struct expected_sql_error incorrect_argument_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    struct expected_sql_error bogus_value = {
        .code = mysql_error_wrong_value_for_variable,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'bogus'",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB monitor user-var db");
    for (size_t index = 0U;
         index < sizeof(innodb_monitor_variables) / sizeof(innodb_monitor_variables[0]);
         ++index) {
        const char *variable = innodb_monitor_variables[index];

        failures += execute_statement_ok(database, "SET @innodb_monitor_value = 'all'");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_monitor_value", variable);
        failures += execute_statement_ok(database, sql);

        failures += execute_statement_ok(database, "SET @innodb_monitor_value = 'latch'");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_monitor_value", variable);
        failures += execute_statement_ok(database, sql);

        failures += execute_statement_ok(database, "SET @innodb_monitor_value = 'module_buffer'");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_monitor_value", variable);
        failures += execute_statement_ok(database, sql);

        failures += execute_statement_ok(database, "SET @innodb_monitor_value = 'buffer%'");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_monitor_value", variable);
        failures += execute_statement_ok(database, sql);

        failures += execute_statement_ok(database, "SET @innodb_monitor_value = 1");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_monitor_value", variable);
        failures += execute_error(database, sql, incorrect_argument_type);

        failures += execute_statement_ok(database, "SET @innodb_monitor_value = NULL");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_monitor_value", variable);
        failures += execute_error(database, sql, incorrect_argument_type);

        failures += execute_statement_ok(database, "SET @innodb_monitor_value = 'bogus'");
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_monitor_value", variable);
        failures += execute_error(database, sql, bogus_value);
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
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

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

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
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
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "%s: expected [%s] to contain [%s]\n", context, actual, needle);
        return 1;
    }
    return 0;
}
