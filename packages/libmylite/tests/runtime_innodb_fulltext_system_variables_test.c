#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_cant_set_value = 1231,
    mysql_error_session_variable_only = 1238,
    sql_buffer_capacity = 512,
};

enum innodb_ft_variable_scope {
    innodb_ft_variable_scope_dynamic_global = 0,
    innodb_ft_variable_scope_nullable_global = 1,
    innodb_ft_variable_scope_dynamic_session = 2,
    innodb_ft_variable_scope_nullable_session = 3,
    innodb_ft_variable_scope_read_only = 4,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct innodb_ft_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_assignment;
    const char *unsupported_assignment;
    enum innodb_ft_variable_scope scope;
};

static const struct innodb_ft_variable innodb_ft_variables[] = {
    {"innodb_ft_aux_table",
     NULL,
     "",
     "NULL",
     "'test/no_such_table'",
     innodb_ft_variable_scope_nullable_global},
    {"innodb_ft_cache_size", "8000000", "8000000", NULL, NULL, innodb_ft_variable_scope_read_only},
    {"innodb_ft_enable_diag_print", "0", "OFF", "OFF", "ON", innodb_ft_variable_scope_dynamic_global
    },
    {"innodb_ft_enable_stopword", "1", "ON", "ON", "OFF", innodb_ft_variable_scope_dynamic_session},
    {"innodb_ft_max_token_size", "84", "84", NULL, NULL, innodb_ft_variable_scope_read_only},
    {"innodb_ft_min_token_size", "3", "3", NULL, NULL, innodb_ft_variable_scope_read_only},
    {"innodb_ft_num_word_optimize",
     "2000",
     "2000",
     "2000",
     "2001",
     innodb_ft_variable_scope_dynamic_global},
    {"innodb_ft_result_cache_limit",
     "2000000000",
     "2000000000",
     "2000000000",
     "2000000001",
     innodb_ft_variable_scope_dynamic_global},
    {"innodb_ft_server_stopword_table",
     NULL,
     "",
     "NULL",
     "'test/no_such_table'",
     innodb_ft_variable_scope_nullable_global},
    {"innodb_ft_sort_pll_degree", "2", "2", NULL, NULL, innodb_ft_variable_scope_read_only},
    {"innodb_ft_total_cache_size",
     "640000000",
     "640000000",
     NULL,
     NULL,
     innodb_ft_variable_scope_read_only},
    {"innodb_ft_user_stopword_table",
     NULL,
     "",
     "NULL",
     "'test/no_such_table'",
     innodb_ft_variable_scope_nullable_session},
};

static int test_values_show_and_scope(void);
static int test_set_diagnostics(void);
static int test_session_mutation(void);
static int test_user_variable_assignments(void);
static bool innodb_ft_variable_allows_session_scope(const struct innodb_ft_variable *variable);
static bool innodb_ft_variable_is_nullable(const struct innodb_ft_variable *variable);
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
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB FT db");
    for (size_t index = 0U; index < sizeof(innodb_ft_variables) / sizeof(innodb_ft_variables[0]);
         ++index) {
        const struct innodb_ft_variable *variable = &innodb_ft_variables[index];

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

        if (innodb_ft_variable_allows_session_scope(variable)) {
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
    struct expected_sql_error cant_set_value = {
        .code = mysql_error_cant_set_value,
        .sqlstate = "42000",
        .message_part = "can't be set to the value",
    };
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open InnoDB FT SET db");
    for (size_t index = 0U; index < sizeof(innodb_ft_variables) / sizeof(innodb_ft_variables[0]);
         ++index) {
        const struct innodb_ft_variable *variable = &innodb_ft_variables[index];

        if (variable->scope == innodb_ft_variable_scope_read_only) {
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

        if (innodb_ft_variable_allows_session_scope(variable)) {
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
        failures += execute_error(
            database,
            sql,
            innodb_ft_variable_is_nullable(variable) ? cant_set_value : unsupported_fixed_noop
        );

        if (innodb_ft_variable_is_nullable(variable)) {
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = NULL", variable->name);
            failures += execute_statement_ok(database, sql);
        }
        if (variable->scope == innodb_ft_variable_scope_nullable_session) {
            snprintf(sql, sizeof(sql), "SET SESSION %s = NULL", variable->name);
            failures += execute_statement_ok(database, sql);
            snprintf(
                sql,
                sizeof(sql),
                "SET %s = %s",
                variable->name,
                variable->unsupported_assignment
            );
            failures += execute_error(database, sql, cant_set_value);
        }
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
        "open InnoDB FT session db"
    );
    failures += execute_statement_ok(database, "SET innodb_ft_enable_stopword = OFF");
    failures += expect_values(
        database,
        "SELECT @@innodb_ft_enable_stopword, @@SESSION.innodb_ft_enable_stopword, "
        "@@GLOBAL.innodb_ft_enable_stopword",
        (const char *[]){"0", "0", "1"},
        3U,
        "innodb_ft_enable_stopword session off"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'innodb_ft_enable_stopword'",
        (const char *[]){"innodb_ft_enable_stopword", "OFF"},
        2U,
        "innodb_ft_enable_stopword SHOW session off"
    );
    failures += expect_values(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'innodb_ft_enable_stopword'",
        (const char *[]){"innodb_ft_enable_stopword", "ON"},
        2U,
        "innodb_ft_enable_stopword SHOW global on"
    );
    failures += execute_statement_ok(database, "SET LOCAL innodb_ft_enable_stopword = ON");
    failures += expect_values(
        database,
        "SELECT @@innodb_ft_enable_stopword, @@LOCAL.innodb_ft_enable_stopword",
        (const char *[]){"1", "1"},
        2U,
        "innodb_ft_enable_stopword local on"
    );
    failures += execute_statement_ok(database, "SET SESSION innodb_ft_enable_stopword = DEFAULT");
    failures += expect_values(
        database,
        "SELECT @@innodb_ft_enable_stopword",
        (const char *[]){"1"},
        1U,
        "innodb_ft_enable_stopword default"
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
    mylite_db *database = NULL;
    int failures = 0;
    char sql[sql_buffer_capacity];

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open InnoDB FT user-var db"
    );
    for (size_t index = 0U; index < sizeof(innodb_ft_variables) / sizeof(innodb_ft_variables[0]);
         ++index) {
        const struct innodb_ft_variable *variable = &innodb_ft_variables[index];

        if (variable->scope == innodb_ft_variable_scope_read_only) {
            failures += execute_statement_ok(database, "SET @innodb_ft_value = NULL");
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_ft_value", variable->name);
            failures += execute_error(database, sql, read_only_set);
            continue;
        }

        if (innodb_ft_variable_is_nullable(variable)) {
            failures += execute_statement_ok(database, "SET @innodb_ft_value = NULL");
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_ft_value", variable->name);
            failures += execute_statement_ok(database, sql);
            if (variable->scope == innodb_ft_variable_scope_nullable_session) {
                snprintf(sql, sizeof(sql), "SET %s = @innodb_ft_value", variable->name);
                failures += execute_statement_ok(database, sql);
            }
            failures +=
                execute_statement_ok(database, "SET @innodb_ft_value = 'test/no_such_table'");
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_ft_value", variable->name);
            failures += execute_error(database, sql, cant_set_value);
            continue;
        }

        snprintf(sql, sizeof(sql), "SET @innodb_ft_value = %s", variable->exact_assignment);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_ft_value", variable->name);
        failures += execute_statement_ok(database, sql);

        snprintf(sql, sizeof(sql), "SET @innodb_ft_value = %s", variable->unsupported_assignment);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = @innodb_ft_value", variable->name);
        failures += execute_error(database, sql, unsupported_fixed_noop);

        if (variable->scope == innodb_ft_variable_scope_dynamic_session) {
            snprintf(sql, sizeof(sql), "SET %s = @innodb_ft_value", variable->name);
            failures += execute_statement_ok(database, sql);
            failures += expect_values(
                database,
                "SELECT @@innodb_ft_enable_stopword",
                (const char *[]){"0"},
                1U,
                "innodb_ft_enable_stopword user-var off"
            );
        }
    }

    mylite_close(database);
    return failures;
}

static bool innodb_ft_variable_allows_session_scope(const struct innodb_ft_variable *variable) {
    return variable != NULL && (variable->scope == innodb_ft_variable_scope_dynamic_session ||
                                variable->scope == innodb_ft_variable_scope_nullable_session);
}

static bool innodb_ft_variable_is_nullable(const struct innodb_ft_variable *variable) {
    return variable != NULL && (variable->scope == innodb_ft_variable_scope_nullable_global ||
                                variable->scope == innodb_ft_variable_scope_nullable_session);
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
