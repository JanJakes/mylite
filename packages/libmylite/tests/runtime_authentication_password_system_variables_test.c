#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    scalar_column_count = 33,
    show_variable_column_count = 2,
    show_variable_row_count = 15,
    global_noop_column_count = 10,
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

static int test_authentication_password_values_and_show_rows(void);
static int test_authentication_password_set_and_diagnostics(void);
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

    failures += test_authentication_password_values_and_show_rows();
    failures += test_authentication_password_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_authentication_password_values_and_show_rows(void) {
    static const char *const scalar_values[] = {
        "*,,",
        "*,,",
        "1",
        "1",
        "5000",
        "5000",
        "private_key.pem",
        "private_key.pem",
        "public_key.pem",
        "public_key.pem",
        "0",
        "0",
        "1",
        "1",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "1",
        "1",
        "private_key.pem",
        "private_key.pem",
        "0",
        "0",
        "public_key.pem",
        "public_key.pem",
        "0",
        "0",
        "-1",
    };
    static const char *const show_rows[] = {
        "authentication_policy",
        "*,,",
        "caching_sha2_password_auto_generate_rsa_keys",
        "ON",
        "caching_sha2_password_digest_rounds",
        "5000",
        "caching_sha2_password_private_key_path",
        "private_key.pem",
        "caching_sha2_password_public_key_path",
        "public_key.pem",
        "default_password_lifetime",
        "0",
        "disconnect_on_expired_password",
        "ON",
        "mysql_native_password_proxy_users",
        "OFF",
        "password_history",
        "0",
        "password_require_current",
        "OFF",
        "password_reuse_interval",
        "0",
        "sha256_password_auto_generate_rsa_keys",
        "ON",
        "sha256_password_private_key_path",
        "private_key.pem",
        "sha256_password_proxy_users",
        "OFF",
        "sha256_password_public_key_path",
        "public_key.pem",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open auth password db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@authentication_policy, @@GLOBAL.authentication_policy, "
                   "@@caching_sha2_password_auto_generate_rsa_keys, "
                   "@@GLOBAL.caching_sha2_password_auto_generate_rsa_keys, "
                   "@@caching_sha2_password_digest_rounds, "
                   "@@GLOBAL.caching_sha2_password_digest_rounds, "
                   "@@caching_sha2_password_private_key_path, "
                   "@@GLOBAL.caching_sha2_password_private_key_path, "
                   "@@caching_sha2_password_public_key_path, "
                   "@@GLOBAL.caching_sha2_password_public_key_path, "
                   "@@default_password_lifetime, @@GLOBAL.default_password_lifetime, "
                   "@@disconnect_on_expired_password, @@GLOBAL.disconnect_on_expired_password, "
                   "@@mysql_native_password_proxy_users, "
                   "@@GLOBAL.mysql_native_password_proxy_users, "
                   "@@password_history, @@GLOBAL.password_history, "
                   "@@password_require_current, @@GLOBAL.password_require_current, "
                   "@@password_reuse_interval, @@GLOBAL.password_reuse_interval, "
                   "@@sha256_password_auto_generate_rsa_keys, "
                   "@@GLOBAL.sha256_password_auto_generate_rsa_keys, "
                   "@@sha256_password_private_key_path, "
                   "@@GLOBAL.sha256_password_private_key_path, "
                   "@@sha256_password_proxy_users, @@GLOBAL.sha256_password_proxy_users, "
                   "@@sha256_password_public_key_path, "
                   "@@GLOBAL.sha256_password_public_key_path, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "auth password scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('authentication_policy',"
                   "'caching_sha2_password_auto_generate_rsa_keys',"
                   "'caching_sha2_password_digest_rounds',"
                   "'caching_sha2_password_private_key_path',"
                   "'caching_sha2_password_public_key_path',"
                   "'default_password_lifetime','disconnect_on_expired_password',"
                   "'mysql_native_password_proxy_users','password_history',"
                   "'password_require_current','password_reuse_interval',"
                   "'sha256_password_auto_generate_rsa_keys',"
                   "'sha256_password_private_key_path','sha256_password_proxy_users',"
                   "'sha256_password_public_key_path')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "auth password SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('authentication_policy',"
                   "'caching_sha2_password_auto_generate_rsa_keys',"
                   "'caching_sha2_password_digest_rounds',"
                   "'caching_sha2_password_private_key_path',"
                   "'caching_sha2_password_public_key_path',"
                   "'default_password_lifetime','disconnect_on_expired_password',"
                   "'mysql_native_password_proxy_users','password_history',"
                   "'password_require_current','password_reuse_interval',"
                   "'sha256_password_auto_generate_rsa_keys',"
                   "'sha256_password_private_key_path','sha256_password_proxy_users',"
                   "'sha256_password_public_key_path')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "auth password SHOW GLOBAL VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('authentication_policy',"
                   "'caching_sha2_password_auto_generate_rsa_keys',"
                   "'caching_sha2_password_digest_rounds',"
                   "'caching_sha2_password_private_key_path',"
                   "'caching_sha2_password_public_key_path',"
                   "'default_password_lifetime','disconnect_on_expired_password',"
                   "'mysql_native_password_proxy_users','password_history',"
                   "'password_require_current','password_reuse_interval',"
                   "'sha256_password_auto_generate_rsa_keys',"
                   "'sha256_password_private_key_path','sha256_password_proxy_users',"
                   "'sha256_password_public_key_path')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "auth password SHOW SESSION VARIABLES rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_authentication_password_set_and_diagnostics(void) {
    static const char *const global_noop_values[] = {
        "*,,",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
    };
    static const char *const global_only_variables[] = {
        "authentication_policy",
        "caching_sha2_password_auto_generate_rsa_keys",
        "caching_sha2_password_digest_rounds",
        "caching_sha2_password_private_key_path",
        "caching_sha2_password_public_key_path",
        "default_password_lifetime",
        "disconnect_on_expired_password",
        "mysql_native_password_proxy_users",
        "password_history",
        "password_require_current",
        "password_reuse_interval",
        "sha256_password_auto_generate_rsa_keys",
        "sha256_password_private_key_path",
        "sha256_password_proxy_users",
        "sha256_password_public_key_path",
    };
    static const char *const fixed_global_variables[] = {
        "authentication_policy",
        "default_password_lifetime",
        "mysql_native_password_proxy_users",
        "password_history",
        "password_require_current",
        "password_reuse_interval",
        "sha256_password_proxy_users",
    };
    static const char *const read_only_assignments[] = {
        "SET GLOBAL caching_sha2_password_auto_generate_rsa_keys = DEFAULT",
        "SET GLOBAL caching_sha2_password_digest_rounds = DEFAULT",
        "SET GLOBAL caching_sha2_password_private_key_path = DEFAULT",
        "SET GLOBAL caching_sha2_password_public_key_path = DEFAULT",
        "SET GLOBAL disconnect_on_expired_password = DEFAULT",
        "SET GLOBAL sha256_password_auto_generate_rsa_keys = DEFAULT",
        "SET GLOBAL sha256_password_private_key_path = DEFAULT",
        "SET GLOBAL sha256_password_public_key_path = DEFAULT",
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
        "open auth password SET db"
    );

    failures += execute_statement_ok(database, "SET GLOBAL authentication_policy = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL authentication_policy = '*,,'");
    failures += execute_statement_ok(database, "SET GLOBAL default_password_lifetime = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL default_password_lifetime = 0");
    failures +=
        execute_statement_ok(database, "SET GLOBAL mysql_native_password_proxy_users = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET GLOBAL mysql_native_password_proxy_users = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL password_history = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL password_history = 0");
    failures += execute_statement_ok(database, "SET GLOBAL password_require_current = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL password_require_current = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL password_reuse_interval = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL password_reuse_interval = 0");
    failures += execute_statement_ok(database, "SET GLOBAL sha256_password_proxy_users = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL sha256_password_proxy_users = OFF");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.authentication_policy, "
                   "@@GLOBAL.default_password_lifetime, "
                   "@@GLOBAL.mysql_native_password_proxy_users, "
                   "@@GLOBAL.password_history, @@GLOBAL.password_require_current, "
                   "@@GLOBAL.password_reuse_interval, @@GLOBAL.sha256_password_proxy_users, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = global_noop_values,
            .column_count = global_noop_column_count,
            .row_count = 1U,
            .context = "auth password global no-op SET readback",
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

    for (size_t index = 0U;
         index < sizeof(read_only_assignments) / sizeof(read_only_assignments[0]);
         ++index) {
        failures += execute_error(database, read_only_assignments[index], read_only);
    }

    failures +=
        execute_error(database, "SET GLOBAL authentication_policy = '*'", unsupported_global_set);
    failures +=
        execute_error(database, "SET GLOBAL default_password_lifetime = 1", unsupported_global_set);
    failures += execute_error(
        database,
        "SET GLOBAL mysql_native_password_proxy_users = ON",
        unsupported_global_set
    );
    failures += execute_error(database, "SET GLOBAL password_history = 1", unsupported_global_set);
    failures +=
        execute_error(database, "SET GLOBAL password_require_current = ON", unsupported_global_set);
    failures +=
        execute_error(database, "SET GLOBAL password_reuse_interval = 1", unsupported_global_set);
    failures += execute_error(
        database,
        "SET GLOBAL sha256_password_proxy_users = ON",
        unsupported_global_set
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
