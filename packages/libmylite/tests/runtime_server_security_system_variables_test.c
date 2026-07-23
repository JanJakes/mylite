#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    scalar_column_count = 29,
    show_variable_column_count = 2,
    show_variable_row_count = 13,
    global_noop_column_count = 7,
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

static int test_server_security_values_and_show_rows(void);
static int test_server_security_set_and_diagnostics(void);
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

int main(void) {
    int failures = 0;

    failures += test_server_security_values_and_show_rows();
    failures += test_server_security_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_server_security_values_and_show_rows(void) {
    static const char *const scalar_values[] = {
        "0",
        "0",
        "/var/lib/mysql-files/",
        "/var/lib/mysql-files/",
        "1",
        "1",
        "1",
        "1",
        "0",
        "0",
        "0",
        "0",
        "OFF",
        "OFF",
        "1",
        "1",
        "300",
        "300",
        "one-thread-per-connection",
        "one-thread-per-connection",
        "0",
        "0",
        "TLSv1.2,TLSv1.3",
        "TLSv1.2,TLSv1.3",
        "/tmp",
        "/tmp",
        "0",
        "0",
        "-1",
    };
    static const char *const show_rows[] = {
        "require_secure_transport",
        "OFF",
        "secure_file_priv",
        "/var/lib/mysql-files/",
        "skip_external_locking",
        "ON",
        "skip_name_resolve",
        "ON",
        "skip_networking",
        "OFF",
        "skip_show_database",
        "OFF",
        "ssl_fips_mode",
        "OFF",
        "ssl_session_cache_mode",
        "ON",
        "ssl_session_cache_timeout",
        "300",
        "thread_handling",
        "one-thread-per-connection",
        "tls_certificates_enforced_validation",
        "OFF",
        "tls_version",
        "TLSv1.2,TLSv1.3",
        "tmpdir",
        "/tmp",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open server security db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@require_secure_transport, @@GLOBAL.require_secure_transport, "
                   "@@secure_file_priv, @@GLOBAL.secure_file_priv, "
                   "@@skip_external_locking, @@GLOBAL.skip_external_locking, "
                   "@@skip_name_resolve, @@GLOBAL.skip_name_resolve, "
                   "@@skip_networking, @@GLOBAL.skip_networking, "
                   "@@skip_show_database, @@GLOBAL.skip_show_database, "
                   "@@ssl_fips_mode, @@GLOBAL.ssl_fips_mode, "
                   "@@ssl_session_cache_mode, @@GLOBAL.ssl_session_cache_mode, "
                   "@@ssl_session_cache_timeout, @@GLOBAL.ssl_session_cache_timeout, "
                   "@@thread_handling, @@GLOBAL.thread_handling, "
                   "@@tls_certificates_enforced_validation, "
                   "@@GLOBAL.tls_certificates_enforced_validation, "
                   "@@tls_version, @@GLOBAL.tls_version, "
                   "@@tmpdir, @@GLOBAL.tmpdir, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "server security scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('require_secure_transport','secure_file_priv',"
                   "'skip_external_locking','skip_name_resolve','skip_networking',"
                   "'skip_show_database','ssl_fips_mode','ssl_session_cache_mode',"
                   "'ssl_session_cache_timeout','thread_handling',"
                   "'tls_certificates_enforced_validation','tls_version','tmpdir')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server security SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('require_secure_transport','secure_file_priv',"
                   "'skip_external_locking','skip_name_resolve','skip_networking',"
                   "'skip_show_database','ssl_fips_mode','ssl_session_cache_mode',"
                   "'ssl_session_cache_timeout','thread_handling',"
                   "'tls_certificates_enforced_validation','tls_version','tmpdir')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server security SHOW GLOBAL VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('require_secure_transport','secure_file_priv',"
                   "'skip_external_locking','skip_name_resolve','skip_networking',"
                   "'skip_show_database','ssl_fips_mode','ssl_session_cache_mode',"
                   "'ssl_session_cache_timeout','thread_handling',"
                   "'tls_certificates_enforced_validation','tls_version','tmpdir')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "server security SHOW SESSION VARIABLES rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_server_security_set_and_diagnostics(void) {
    static const char *const global_noop_values[] = {
        "0",
        "1",
        "300",
        "TLSv1.2,TLSv1.3",
        "0",
        "0",
        "0",
    };
    static const char *const global_only_variables[] = {
        "require_secure_transport",
        "secure_file_priv",
        "skip_external_locking",
        "skip_name_resolve",
        "skip_networking",
        "skip_show_database",
        "ssl_fips_mode",
        "ssl_session_cache_mode",
        "ssl_session_cache_timeout",
        "thread_handling",
        "tls_certificates_enforced_validation",
        "tls_version",
        "tmpdir",
    };
    static const char *const read_only_assignments[] = {
        "SET GLOBAL secure_file_priv = DEFAULT",
        "SET GLOBAL skip_external_locking = DEFAULT",
        "SET GLOBAL skip_name_resolve = DEFAULT",
        "SET GLOBAL skip_networking = DEFAULT",
        "SET GLOBAL skip_show_database = DEFAULT",
        "SET GLOBAL ssl_fips_mode = DEFAULT",
        "SET GLOBAL thread_handling = DEFAULT",
        "SET GLOBAL tls_certificates_enforced_validation = DEFAULT",
        "SET GLOBAL tmpdir = DEFAULT",
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
    mylite_db *database = NULL;
    char sql[diagnostic_context_capacity];
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open server security SET db"
    );

    failures += execute_statement_ok(database, "SET GLOBAL require_secure_transport = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL require_secure_transport = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL ssl_session_cache_mode = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL ssl_session_cache_mode = ON");
    failures += execute_statement_ok(database, "SET GLOBAL ssl_session_cache_timeout = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL ssl_session_cache_timeout = 300");
    failures += execute_statement_ok(database, "SET GLOBAL tls_version = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL tls_version = 'TLSv1.2,TLSv1.3'");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.require_secure_transport, "
                   "@@GLOBAL.ssl_session_cache_mode, "
                   "@@GLOBAL.ssl_session_cache_timeout, @@GLOBAL.tls_version, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = global_noop_values,
            .column_count = global_noop_column_count,
            .row_count = 1U,
            .context = "server security global no-op SET readback",
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

    failures += execute_error(database, "SET require_secure_transport = DEFAULT", global_only_set);
    failures += execute_error(database, "SET ssl_session_cache_mode = DEFAULT", global_only_set);
    failures += execute_error(database, "SET ssl_session_cache_timeout = DEFAULT", global_only_set);
    failures += execute_error(database, "SET tls_version = DEFAULT", global_only_set);

    for (size_t index = 0U;
         index < sizeof(read_only_assignments) / sizeof(read_only_assignments[0]);
         ++index) {
        failures += execute_error(database, read_only_assignments[index], read_only);
    }

    failures +=
        execute_error(database, "SET GLOBAL require_secure_transport = ON", unsupported_global_set);
    failures +=
        execute_error(database, "SET GLOBAL ssl_session_cache_mode = OFF", unsupported_global_set);
    failures += execute_error(
        database,
        "SET GLOBAL ssl_session_cache_timeout = 301",
        unsupported_global_set
    );
    failures +=
        execute_error(database, "SET GLOBAL tls_version = 'TLSv1.3'", unsupported_global_set);

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

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
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
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    snprintf(context, sizeof(context), "%s error code", sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, context);
    snprintf(context, sizeof(context), "%s SQLSTATE", sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    snprintf(context, sizeof(context), "%s error message", sql);
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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

    return mylite_test_expect_text(actual, expected, context);
}
