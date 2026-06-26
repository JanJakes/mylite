#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 11,
    no_op_scalar_value_count = 9,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
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

static int test_values_show_scope_and_warnings(void);
static int test_set_and_diagnostics(void);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

int main(void) {
    int failures = 0;

    failures += test_values_show_scope_and_warnings();
    failures += test_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_values_show_scope_and_warnings(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open M limit db");
    failures += expect_values(
        database,
        "SELECT @@master_verify_checksum, @@GLOBAL.master_verify_checksum, "
        "@@max_binlog_cache_size, @@max_binlog_size, @@max_binlog_stmt_cache_size, "
        "@@max_connect_errors, @@max_connections, @@max_digest_length, "
        "@@max_prepared_stmt_count, @@max_relay_log_size, @@max_write_lock_count",
        (const char *[]){"0",
                         "0",
                         "18446744073709547520",
                         "1073741824",
                         "18446744073709547520",
                         "100",
                         "151",
                         "1024",
                         "16382",
                         "0",
                         "18446744073709551615"},
        default_scalar_value_count,
        "M server limit defaults"
    );
    failures += expect_values(
        database,
        "SELECT @@master_verify_checksum",
        (const char *[]){"0"},
        1U,
        "master_verify_checksum scalar"
    );
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning",
                         "1287",
                         "'@@master_verify_checksum' is deprecated and will be removed in a "
                         "future release. Please use source_verify_checksum instead."},
        3U,
        "master_verify_checksum read warning"
    );

    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'master_verify_checksum'",
                                     .name = "master_verify_checksum",
                                     .value = "OFF",
                                     .context = "master_verify_checksum show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'max_binlog_cache_size'",
                                     .name = "max_binlog_cache_size",
                                     .value = "18446744073709547520",
                                     .context = "max_binlog_cache_size show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW GLOBAL VARIABLES LIKE 'max_connections'",
                                     .name = "max_connections",
                                     .value = "151",
                                     .context = "max_connections show global"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'max_digest_length'",
                                     .name = "max_digest_length",
                                     .value = "1024",
                                     .context = "max_digest_length show session"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'max_write_lock_count'",
                                     .name = "max_write_lock_count",
                                     .value = "18446744073709551615",
                                     .context = "max_write_lock_count show"}
    );

    failures +=
        execute_error(database, "SELECT @@SESSION.master_verify_checksum", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.max_binlog_size", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.max_digest_length", global_only_read);
    failures += execute_error(database, "SELECT @@SESSION.max_write_lock_count", global_only_read);

    mylite_close(database);
    return failures;
}

static int test_set_and_diagnostics(void) {
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

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open M limit SET db");

    failures += execute_statement_ok(database, "SET GLOBAL master_verify_checksum = DEFAULT");
    failures += expect_values(
        database,
        "SHOW WARNINGS",
        (const char *[]){"Warning",
                         "1287",
                         "'@@master_verify_checksum' is deprecated and will be removed in a "
                         "future release. Please use source_verify_checksum instead."},
        3U,
        "master_verify_checksum default SET warning"
    );
    failures += execute_statement_ok(database, "SET GLOBAL master_verify_checksum = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL max_binlog_cache_size = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET GLOBAL max_binlog_cache_size = 18446744073709547520");
    failures += execute_statement_ok(database, "SET GLOBAL max_binlog_size = 1073741824");
    failures += execute_statement_ok(
        database,
        "SET GLOBAL max_binlog_stmt_cache_size = 18446744073709547520"
    );
    failures += execute_statement_ok(database, "SET GLOBAL max_connect_errors = 100");
    failures += execute_statement_ok(database, "SET GLOBAL max_connections = 151");
    failures += execute_statement_ok(database, "SET GLOBAL max_prepared_stmt_count = 16382");
    failures += execute_statement_ok(database, "SET GLOBAL max_relay_log_size = 0");
    failures +=
        execute_statement_ok(database, "SET GLOBAL max_write_lock_count = 18446744073709551615");
    failures += expect_values(
        database,
        "SELECT @@GLOBAL.master_verify_checksum, @@GLOBAL.max_binlog_cache_size, "
        "@@GLOBAL.max_binlog_size, @@GLOBAL.max_binlog_stmt_cache_size, "
        "@@GLOBAL.max_connect_errors, @@GLOBAL.max_connections, "
        "@@GLOBAL.max_prepared_stmt_count, @@GLOBAL.max_relay_log_size, "
        "@@GLOBAL.max_write_lock_count",
        (const char *[]){"0",
                         "18446744073709547520",
                         "1073741824",
                         "18446744073709547520",
                         "100",
                         "151",
                         "16382",
                         "0",
                         "18446744073709551615"},
        no_op_scalar_value_count,
        "M server limit global no-op values"
    );

    failures += execute_error(database, "SET max_binlog_size = DEFAULT", global_only_set);
    failures += execute_error(database, "SET max_connections = DEFAULT", global_only_set);
    failures += execute_error(database, "SET GLOBAL max_digest_length = DEFAULT", read_only_set);
    failures +=
        execute_error(database, "SET GLOBAL master_verify_checksum = ON", unsupported_fixed_noop);
    failures +=
        execute_error(database, "SET GLOBAL max_connect_errors = 101", unsupported_fixed_noop);
    failures += execute_error(database, "SET GLOBAL max_connections = 152", unsupported_fixed_noop);
    failures += execute_error(
        database,
        "SET GLOBAL max_write_lock_count = 18446744073709551614",
        unsupported_fixed_noop
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
            "%s: expected OK for [%s], got %d %s\n",
            context,
            sql,
            rc,
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += expect_size(mylite_result_column_count(result), expected_count, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    for (size_t column = 0U; column < expected_count; ++column) {
        failures +=
            expect_text(mylite_result_value_text(result, 0U, column), expected[column], context);
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

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
