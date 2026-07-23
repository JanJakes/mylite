#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_capacity = 384,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct bootstrap_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    bool session_scope;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_value {
    const char *sql;
    const char *name;
    const char *value;
    const char *context;
};

static int test_bootstrap_values_show_and_scope(void);
static int test_bootstrap_set_and_diagnostics(void);
static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
);
static int expect_show_value(mylite_db *database, struct expected_show_value expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static const struct bootstrap_variable bootstrap_variables[] = {
    {"activate_all_roles_on_login", "0", "OFF", false},
    {"auto_generate_certs", "1", "ON", false},
    {"automatic_sp_privileges", "1", "ON", false},
    {"block_encryption_mode", "aes-128-ecb", "aes-128-ecb", true},
    {"build_id",
     "66e221b3840955d27f740799b5b2c6eb0baf3283",
     "66e221b3840955d27f740799b5b2c6eb0baf3283",
     false},
    {"bulk_insert_buffer_size", "8388608", "8388608", true},
    {"character_sets_dir", "/usr/share/mysql-8.4/charsets/", "/usr/share/mysql-8.4/charsets/", false
    },
    {"check_proxy_users", "0", "OFF", false},
};

int main(void) {
    int failures = 0;

    failures += test_bootstrap_values_show_and_scope();
    failures += test_bootstrap_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_bootstrap_values_show_and_scope(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open bootstrap db");
    for (size_t index = 0U; index < sizeof(bootstrap_variables) / sizeof(bootstrap_variables[0]);
         ++index) {
        const struct bootstrap_variable *variable = &bootstrap_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s, @@GLOBAL.%s", variable->name, variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->scalar_value, variable->scalar_value},
            2U,
            "bootstrap scalar/global"
        );

        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "bootstrap show"}
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "bootstrap show global"}
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "bootstrap show session"}
        );

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
        if (variable->session_scope) {
            failures += expect_values(
                database,
                sql,
                (const char *[]){variable->scalar_value},
                1U,
                "bootstrap session scalar"
            );
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
    }

    mylite_close(database);
    return failures;
}

static int test_bootstrap_set_and_diagnostics(void) {
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
    struct expected_sql_error unsupported_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error unsupported_user_variable_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "bootstrap system variables from user variables",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open bootstrap SET db");

    failures +=
        execute_error(database, "SET activate_all_roles_on_login = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL activate_all_roles_on_login = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL activate_all_roles_on_login = OFF");
    failures +=
        execute_error(database, "SET GLOBAL activate_all_roles_on_login = ON", unsupported_set);
    failures += execute_error(database, "SET GLOBAL auto_generate_certs = DEFAULT", read_only_set);
    failures += execute_error(database, "SET SESSION auto_generate_certs = DEFAULT", read_only_set);
    failures += execute_error(database, "SET automatic_sp_privileges = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL automatic_sp_privileges = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL automatic_sp_privileges = ON");
    failures +=
        execute_error(database, "SET GLOBAL automatic_sp_privileges = OFF", unsupported_set);

    failures += execute_statement_ok(database, "SET block_encryption_mode = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION block_encryption_mode = 'aes-128-ecb'");
    failures += execute_statement_ok(database, "SET GLOBAL block_encryption_mode = 'aes-128-ecb'");
    failures +=
        execute_error(database, "SET block_encryption_mode = 'aes-256-ecb'", unsupported_set);

    failures += execute_error(database, "SET GLOBAL build_id = DEFAULT", read_only_set);
    failures += execute_error(database, "SET SESSION build_id = DEFAULT", read_only_set);
    failures += execute_statement_ok(database, "SET bulk_insert_buffer_size = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION bulk_insert_buffer_size = 8388608");
    failures += execute_statement_ok(database, "SET GLOBAL bulk_insert_buffer_size = 8388608");
    failures += execute_error(database, "SET bulk_insert_buffer_size = 8388609", unsupported_set);
    failures += execute_error(database, "SET GLOBAL character_sets_dir = DEFAULT", read_only_set);
    failures += execute_error(database, "SET SESSION character_sets_dir = DEFAULT", read_only_set);
    failures += execute_error(database, "SET check_proxy_users = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL check_proxy_users = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL check_proxy_users = OFF");
    failures += execute_error(database, "SET GLOBAL check_proxy_users = ON", unsupported_set);

    failures += execute_statement_ok(database, "SET @bootstrap_value = 'aes-128-ecb'");
    failures += execute_error(
        database,
        "SET SESSION block_encryption_mode = @bootstrap_value",
        unsupported_user_variable_set
    );
    failures += execute_error(
        database,
        "SET GLOBAL check_proxy_users = @bootstrap_value",
        unsupported_user_variable_set
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
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_execute(database, sql, strlen(sql), &result),
        MYLITE_OK,
        context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), expected_count, context);
    for (size_t index = 0U; index < expected_count; ++index) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, index),
            expected[index],
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_value expected) {
    return expect_values(
        database,
        expected.sql,
        (const char *[]){expected.name, expected.value},
        2U,
        expected.context
    );
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_execute(database, sql, strlen(sql), &result), MYLITE_OK, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_execute(database, sql, strlen(sql), &result),
        MYLITE_ERROR,
        sql
    );
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}
