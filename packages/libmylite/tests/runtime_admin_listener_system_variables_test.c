#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    scalar_column_count = 27,
    show_variable_column_count = 2,
    show_variable_row_count = 12,
    global_noop_column_count = 12,
    diagnostic_context_capacity = 512,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_admin_listener_values_and_show_rows(void);
static int test_admin_listener_set_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_admin_listener_values_and_show_rows();
    failures += test_admin_listener_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_admin_listener_values_and_show_rows(void) {
    static const char *const scalar_values[] = {
        NULL,
        NULL,
        "33062",
        "33062",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "TLSv1.2,TLSv1.3",
        "TLSv1.2,TLSv1.3",
        "0",
        "0",
        "0",
        "0",
        "-1",
    };
    static const char *const show_rows[] = {
        "admin_address",
        "",
        "admin_port",
        "33062",
        "admin_ssl_ca",
        "",
        "admin_ssl_capath",
        "",
        "admin_ssl_cert",
        "",
        "admin_ssl_cipher",
        "",
        "admin_ssl_crl",
        "",
        "admin_ssl_crlpath",
        "",
        "admin_ssl_key",
        "",
        "admin_tls_ciphersuites",
        "",
        "admin_tls_version",
        "TLSv1.2,TLSv1.3",
        "create_admin_listener_thread",
        "OFF",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open admin listener db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@admin_address, @@GLOBAL.admin_address, "
                   "@@admin_port, @@GLOBAL.admin_port, "
                   "@@admin_ssl_ca, @@GLOBAL.admin_ssl_ca, "
                   "@@admin_ssl_capath, @@GLOBAL.admin_ssl_capath, "
                   "@@admin_ssl_cert, @@GLOBAL.admin_ssl_cert, "
                   "@@admin_ssl_cipher, @@GLOBAL.admin_ssl_cipher, "
                   "@@admin_ssl_crl, @@GLOBAL.admin_ssl_crl, "
                   "@@admin_ssl_crlpath, @@GLOBAL.admin_ssl_crlpath, "
                   "@@admin_ssl_key, @@GLOBAL.admin_ssl_key, "
                   "@@admin_tls_ciphersuites, @@GLOBAL.admin_tls_ciphersuites, "
                   "@@admin_tls_version, @@GLOBAL.admin_tls_version, "
                   "@@create_admin_listener_thread, @@GLOBAL.create_admin_listener_thread, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "admin listener scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('admin_address','admin_port','admin_ssl_ca','admin_ssl_capath',"
                   "'admin_ssl_cert','admin_ssl_cipher','admin_ssl_crl',"
                   "'admin_ssl_crlpath','admin_ssl_key','admin_tls_ciphersuites',"
                   "'admin_tls_version','create_admin_listener_thread')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "admin listener SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('admin_address','admin_port','admin_ssl_ca','admin_ssl_capath',"
                   "'admin_ssl_cert','admin_ssl_cipher','admin_ssl_crl',"
                   "'admin_ssl_crlpath','admin_ssl_key','admin_tls_ciphersuites',"
                   "'admin_tls_version','create_admin_listener_thread')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "admin listener SHOW GLOBAL VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('admin_address','admin_port','admin_ssl_ca','admin_ssl_capath',"
                   "'admin_ssl_cert','admin_ssl_cipher','admin_ssl_crl',"
                   "'admin_ssl_crlpath','admin_ssl_key','admin_tls_ciphersuites',"
                   "'admin_tls_version','create_admin_listener_thread')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "admin listener SHOW SESSION VARIABLES rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_admin_listener_set_and_diagnostics(void) {
    static const char *const global_noop_values[] = {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "TLSv1.2,TLSv1.3",
        "0",
        "0",
        "0",
    };
    static const char *const global_only_variables[] = {
        "admin_address",
        "admin_port",
        "admin_ssl_ca",
        "admin_ssl_capath",
        "admin_ssl_cert",
        "admin_ssl_cipher",
        "admin_ssl_crl",
        "admin_ssl_crlpath",
        "admin_ssl_key",
        "admin_tls_ciphersuites",
        "admin_tls_version",
        "create_admin_listener_thread",
    };
    static const char *const fixed_global_variables[] = {
        "admin_ssl_ca",
        "admin_ssl_capath",
        "admin_ssl_cert",
        "admin_ssl_cipher",
        "admin_ssl_crl",
        "admin_ssl_crlpath",
        "admin_ssl_key",
        "admin_tls_ciphersuites",
        "admin_tls_version",
    };
    static const char *const read_only_variables[] = {
        "admin_address",
        "admin_port",
        "create_admin_listener_thread",
    };
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error unsupported_global_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET supports only fixed no-op system variable assignments",
    };
    struct expected_sql_error unsupported_user_variable_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "from user variables are not supported",
    };
    mylite_db *database = NULL;
    char sql[diagnostic_context_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open admin listener SET db");

    failures += execute_statement_ok(database, "SET GLOBAL admin_ssl_ca = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_ssl_capath = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_ssl_cert = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_ssl_cipher = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_ssl_crl = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_ssl_crlpath = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_ssl_key = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_tls_ciphersuites = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_tls_version = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL admin_tls_version = 'TLSv1.2,TLSv1.3'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.admin_ssl_ca, @@GLOBAL.admin_ssl_capath, "
                   "@@GLOBAL.admin_ssl_cert, @@GLOBAL.admin_ssl_cipher, "
                   "@@GLOBAL.admin_ssl_crl, @@GLOBAL.admin_ssl_crlpath, "
                   "@@GLOBAL.admin_ssl_key, @@GLOBAL.admin_tls_ciphersuites, "
                   "@@GLOBAL.admin_tls_version, @@warning_count, @@error_count, ROW_COUNT()",
            .values = global_noop_values,
            .column_count = global_noop_column_count,
            .row_count = 1U,
            .context = "admin listener global no-op SET readback",
        }
    );

    for (size_t index = 0U;
         index < sizeof(global_only_variables) / sizeof(global_only_variables[0]);
         ++index) {
        int written =
            snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", global_only_variables[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
        written = snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", global_only_variables[index]);
        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
    }

    for (size_t index = 0U;
         index < sizeof(fixed_global_variables) / sizeof(fixed_global_variables[0]);
         ++index) {
        int written = snprintf(sql, sizeof(sql), "SET %s = DEFAULT", fixed_global_variables[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_set);
        }
    }

    for (size_t index = 0U; index < sizeof(read_only_variables) / sizeof(read_only_variables[0]);
         ++index) {
        int written =
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", read_only_variables[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, read_only);
        }
        written = snprintf(sql, sizeof(sql), "SET %s = DEFAULT", read_only_variables[index]);
        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, read_only);
        }
    }

    failures += execute_error(database, "SET GLOBAL admin_ssl_ca = ''", unsupported_global_set);
    failures +=
        execute_error(database, "SET GLOBAL admin_tls_version = 'TLSv1.3'", unsupported_global_set);
    failures += execute_statement_ok(database, "SET @admin_tls_version = 'TLSv1.2,TLSv1.3'");
    failures += execute_error(
        database,
        "SET GLOBAL admin_tls_version = @admin_tls_version",
        unsupported_user_variable_set
    );

    mylite_close(database);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

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

    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures += expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    char context[diagnostic_context_capacity];
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    snprintf(context, sizeof(context), "%s error code", sql);
    failures += expect_int(mylite_errcode(database), expected.code, context);
    snprintf(context, sizeof(context), "%s SQLSTATE", sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    snprintf(context, sizeof(context), "%s error message", sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
    mylite_result_free(result);

    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return expect_text(actual, expected, context);
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
