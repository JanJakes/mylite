#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_capacity = 256,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct server_tls_variable {
    const char *name;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_single_value {
    const char *sql;
    const char *value;
    const char *context;
};

struct expected_show_variable {
    const char *sql;
    const char *name;
    const char *value;
    const char *context;
};

static int test_server_tls_values_show_and_scope(void);
static int test_server_tls_set_and_diagnostics(void);
static int expect_single_value(mylite_db *database, struct expected_single_value expected);
static int expect_show_value(mylite_db *database, struct expected_show_variable expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

static const struct server_tls_variable server_tls_variables[] = {
    {"ssl_ca"},
    {"ssl_capath"},
    {"ssl_cert"},
    {"ssl_cipher"},
    {"ssl_crl"},
    {"ssl_crlpath"},
    {"ssl_key"},
    {"tls_ciphersuites"},
};

int main(void) {
    int failures = 0;

    failures += test_server_tls_values_show_and_scope();
    failures += test_server_tls_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_server_tls_values_show_and_scope(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open server TLS db");
    for (size_t index = 0U; index < sizeof(server_tls_variables) / sizeof(server_tls_variables[0]);
         ++index) {
        const struct server_tls_variable *variable = &server_tls_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s", variable->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = NULL,
                .context = "server TLS scalar",
            }
        );
        snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", variable->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = NULL,
                .context = "server TLS global scalar",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = variable->name,
                .value = "",
                .context = "server TLS show",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = variable->name,
                .value = "",
                .context = "server TLS show global",
            }
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_variable){
                .sql = sql,
                .name = variable->name,
                .value = "",
                .context = "server TLS show session",
            }
        );

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
        failures += execute_error(database, sql, global_only_read);
        snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", variable->name);
        failures += execute_error(database, sql, global_only_read);
    }

    mylite_close(database);
    return failures;
}

static int test_server_tls_set_and_diagnostics(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error unsupported_global_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET supports only fixed no-op system variable assignments",
    };
    struct expected_sql_error unsupported_user_variable_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "server TLS system variables from user variables are not supported",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open server TLS SET db");
    for (size_t index = 0U; index < sizeof(server_tls_variables) / sizeof(server_tls_variables[0]);
         ++index) {
        const struct server_tls_variable *variable = &server_tls_variables[index];

        snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
        failures += execute_error(database, sql, global_only_set);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", variable->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = NULL,
                .context = "server TLS global default no-op",
            }
        );

        snprintf(sql, sizeof(sql), "SET GLOBAL %s = NULL", variable->name);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", variable->name);
        failures += expect_single_value(
            database,
            (struct expected_single_value){
                .sql = sql,
                .value = NULL,
                .context = "server TLS global NULL no-op",
            }
        );

        snprintf(sql, sizeof(sql), "SET GLOBAL %s = ''", variable->name);
        failures += execute_error(database, sql, unsupported_global_set);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = 'server.pem'", variable->name);
        failures += execute_error(database, sql, unsupported_global_set);
    }

    failures += execute_statement_ok(database, "SET @server_tls = NULL");
    failures +=
        execute_error(database, "SET GLOBAL ssl_ca = @server_tls", unsupported_user_variable_set);

    mylite_close(database);
    return failures;
}

static int expect_single_value(mylite_db *database, struct expected_single_value expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK for [%s], got %d %s\n",
            expected.context,
            expected.sql,
            rc,
            mylite_errmsg(database)
        );
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 0U), expected.value, expected.context);
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_variable expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK for [%s], got %d %s\n",
            expected.context,
            expected.sql,
            rc,
            mylite_errmsg(database)
        );
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 2U, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 0U), expected.name, expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 1U), expected.value, expected.context);
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
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

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
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
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
}
